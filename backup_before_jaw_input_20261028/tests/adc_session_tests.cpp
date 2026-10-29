// Real Sensoray/Festo code, in-memory SDK and ownership only. No hardware library.
#include "Sensoray826.h"
#include "Festo.h"
#include "826api.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

namespace fake {
std::vector<std::string> events;
std::array<std::array<uint, 3>, 16> config{};
uint enabled = 0, mask = 8, trigger = 16, available = 0x30, timestamp = 100;
int status = 0;
bool deny_owner = false, ignore_mask = false, ignore_slot = false, fail_stop = false;
bool returning_data = false, slow_read = false;
unsigned dac_calls = 0, open_calls = 0, nonzero_timeout = 0;
std::atomic<unsigned> adc_inside{0}, collisions{0};
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
}

BoardOwnership::~BoardOwnership() { release(); }
bool BoardOwnership::acquire(unsigned) { fake::events.push_back("lock"); return !fake::deny_owner; }
void BoardOwnership::release() { fake::events.push_back("unlock"); }
bool BoardOwnership::audit(unsigned) { return true; }

extern "C" {
int S826_SystemOpen() { ++fake::open_calls; fake::events.push_back("open"); return 1; }
int S826_SystemClose() { fake::events.push_back("close"); return 0; }
int S826_AdcEnableRead(uint, uint* value) { *value = fake::enabled; return 0; }
int S826_AdcEnableWrite(uint, uint value) {
    fake::events.push_back(value ? "start" : "stop");
    if (!value && fake::fail_stop) return -5;
    fake::enabled = value; return 0;
}
int S826_AdcSlotlistRead(uint, uint* value) { *value = fake::mask; return 0; }
int S826_AdcSlotlistWrite(uint, uint value, uint mode) {
    fake::events.push_back("mask");
    if (fake::ignore_mask && value == 0x30) return 0;
    fake::mask = mode == S826_BITSET ? fake::mask | value : value; return 0;
}
int S826_AdcTrigModeRead(uint, uint* value) { *value = fake::trigger; return 0; }
int S826_AdcTrigModeWrite(uint, uint value) { fake::trigger = value; return 0; }
int S826_AdcSlotConfigRead(uint, uint slot, uint* channel, uint* settling, uint* gain) {
    const auto& value = fake::config[slot]; *channel=value[0]; *settling=value[1]; *gain=value[2]; return 0;
}
int S826_AdcSlotConfigWrite(uint, uint slot, uint channel, uint settling, uint gain) {
    fake::events.push_back("slot" + std::to_string(slot));
    if (!(fake::ignore_slot && slot == 5 && channel == 10)) fake::config[slot] = {channel, settling, gain};
    return 0;
}
int S826_AdcRead(uint, int* values, uint* timestamps, uint* slots, uint timeout) {
    if (timeout) ++fake::nonzero_timeout;
    ++fake::adc_inside;
    if (fake::slow_read) std::this_thread::sleep_for(std::chrono::microseconds(50));
    *slots = fake::returning_data ? fake::available & *slots : 0;
    if (values) { values[4]=0x55000000 | 0xF000; values[5]=0xAA000000 | 8192; values[6]=16384; }
    if (timestamps) for (unsigned i=0;i<16;++i) timestamps[i]=fake::timestamp;
    --fake::adc_inside;
    return fake::returning_data ? fake::status : static_cast<int>(S826_ERR_NOTREADY);
}
int S826_DacRangeWrite(uint, uint, uint, uint) { ++fake::dac_calls; return 0; }
int S826_DacDataWrite(uint, uint, uint value, uint) {
    if (fake::adc_inside.load()) ++fake::collisions;
    ++fake::dac_calls;
    fake::events.push_back("dac" + std::to_string(value)); return 0;
}
}

int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "adc_only";
        Sensoray826 board;
        if (name == "owner_refused") {
            fake::deny_owner=true;
            fake::require(board.initialize()!=0 && fake::open_calls==0, "owner must refuse before SDK open"); return 0;
        }
        if (name == "enabled_refused") {
            fake::enabled=1;
            fake::require(board.initialize()!=0, "enabled ADC must refuse");
            fake::require(fake::enabled==1 && std::find(fake::events.begin(),fake::events.end(),"stop")==fake::events.end(), "must not stop unknown acquisition"); return 0;
        }
        fake::require(board.initialize()==0, "initialize");
        if (name == "combined") {
            std::array<Festo,3> festos{{Festo(board,0,15,0),Festo(board,1,14,1),Festo(board,2,13,2)}};
            for (auto& festo:festos) fake::require(festo.initialize()==0, "Festo initialize");
            fake::require(board.startPotentiometers()==0, "combined ADC start");
            for (auto& festo:festos) fake::require(festo.setPressure(0,false)==0, "unchanged zero pressure");
            fake::require(std::count(fake::events.begin(),fake::events.end(),"dac16384")==6, "zero remains DAC16384, startup+explicit");
        } else {
            const bool supply=name=="supply";
            if (name=="mask_mismatch") fake::ignore_mask=true;
            if (name=="slot_mismatch") fake::ignore_slot=true;
            if (name=="stale_leftover") fake::returning_data=true;
            const int started=board.startPotentiometers(supply);
            if (name=="mask_mismatch" || name=="slot_mismatch") {
                fake::require(started!=0, "silently wrong configuration must fail");
            } else {
                fake::require(started==0, "start");
                fake::require(fake::mask==(supply?0x70u:0x30u), "owned slot mask");
                fake::require(fake::config[4]==std::array<uint,3>{11,100,0} && fake::config[5]==std::array<uint,3>{10,100,0}, "channel/settle/gain");
                fake::returning_data=true;
                if (supply) fake::available=0x70;
                if (name=="partial") {fake::available=0x10; fake::status=-3;}
                if (name=="missed_trigger") fake::status=-6;
                if (name=="critical_error") fake::status=-5;
                if (name=="wrap") fake::timestamp=0xFFFFFFF0u;
                auto first=board.readPotentiometers();
                if (name=="stale_leftover") fake::require(!first.valid[0] && !first.have_new_data, "old stopped-ADC data cannot become fresh");
                else if (name=="critical_error") fake::require(!first.valid[0] && first.sdk_status==-5, "critical error invalidates");
                else if (name=="missed_trigger") fake::require(!first.valid[0] && first.have_new_data && first.sdk_status==-6, "warning preserved, data invalid");
                else {
                    fake::require(first.valid[0] && first.have_new_data, "fresh first slot");
                    fake::require(std::abs(first.voltage_v[0]-(-4096.0*10/32767))<1e-12, "signed low16 conversion ignores metadata");
                    fake::require(first.valid[1]!=(name=="partial"), "partial availability");
                    if (supply) fake::require(first.supply_valid && std::abs(first.supply_voltage_v-16384.0*10/32767)<1e-12, "third source metadata");
                    auto duplicate=board.readPotentiometers();
                    fake::require(!duplicate.valid[0] && !duplicate.have_new_data, "duplicate conversion is not fresh");
                    if (name=="wrap") {
                        fake::timestamp=17; fake::require(board.readPotentiometers().valid[0], "32bit wrap progresses");
                        fake::timestamp=5; fake::require(!board.readPotentiometers().valid[0], "backward source invalidates");
                        fake::timestamp=25; fake::require(!board.readPotentiometers().valid[0], "backward fault remains latched");
                    }
                }
                if (name=="concurrency") {
                    fake::slow_read=true;
                    std::thread read([&] {for(int i=0;i<200;++i)board.readPotentiometers();});
                    std::thread write([&] {for(int i=0;i<200;++i)board.writeDac(0,16384);});
                    read.join();write.join();fake::require(fake::collisions==0,"SDK ADC/DAC calls must never overlap");
                }
            }
        }
        if (name=="stop_failure") fake::fail_stop=true;
        const auto before_close=fake::events.size();
        const int closed=board.close();
        if (name=="stop_failure") {
            fake::require(closed!=0, "failed stop must report cleanup error");
            fake::require(std::find(fake::events.begin()+before_close,fake::events.end(),"slot4")==fake::events.end(), "never restore live ADC configuration");
        } else {
            fake::require(closed==0 && fake::enabled==0, "close confirms stopped");
            fake::require(fake::mask==8 && fake::trigger==16, "restore original list/trigger");
            for(const auto& config:fake::config)fake::require(config==std::array<uint,3>{0,0,0}, "restore original touched slots");
            fake::require(fake::events[before_close]=="stop", "stop before restore/SDK close");
        }
        fake::require(fake::events[fake::events.size()-2]=="close" && fake::events.back()=="unlock", "SDK close before advisory unlock");
        fake::require(fake::nonzero_timeout==0, "every ADC batch/discard read must be tmax0");
        if(name!="combined" && name!="concurrency")fake::require(fake::dac_calls==0,"ADC-only must never configure/write DAC");
        std::cout<<"PASS "<<name<<'\n';return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

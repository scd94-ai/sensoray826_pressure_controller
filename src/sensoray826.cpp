#include "Sensoray826.h"
#include "826api.h"

#include <iostream>
#include <limits>
#include <mutex>

namespace {
    constexpr unsigned int ADC_SETTLING_TIME_US = 0;
    std::recursive_mutex sdk_mutex;
    Sensoray826* sdk_owner = nullptr;
    bool sdk_used = false;
}

Sensoray826::Sensoray826(unsigned int board_num)
    : board_num_(board_num),
      system_open_(false),
      adc_running_(false)
{
}

Sensoray826::~Sensoray826()
{
    close();
}

int Sensoray826::initialize()
{
    if (system_open_) return S826_ERR_OK;

    initialization_error_.clear();

    if (board_num_ > 15 || sdk_owner || sdk_used) {
        initialization_error_ = "Invalid board number or Sensoray SDK already used";
        return -1;
    }

    if (!ownership_.acquire(board_num_)) {
        initialization_error_ = ownership_.error();
        std::cerr << initialization_error_ << '\n';
        return -1;
    }
    sdk_owner = this;
    sdk_used = true; // This SDK has process-global state; restart after closing.
    int status = S826_SystemOpen();
    if (status < 0) {
        initialization_error_ =
            "S826_SystemOpen failed, status=" +
            std::to_string(status);

        sdk_owner = nullptr;
        ownership_.release();
        return status;
    }
    system_open_ = true;
    auto failed = [this](int error) {
        if (initialization_error_.empty()) {
            initialization_error_ =
                "Sensoray ADC initialization failed, status=" +
                std::to_string(error);
        }

        close();
        return error;
    };
    if (!(status & (1u << board_num_))) return failed(-1);
    uint enabled = 0;
    status = S826_AdcEnableRead(board_num_, &enabled);
    if (status != S826_ERR_OK) return failed(status);
    // Never stop or reconfigure another acquisition, even with no visible owner.
if (enabled != 0) {
    initialization_error_ =
        "ADC is already enabled (state=" +
        std::to_string(enabled) +
        "); refusing to stop or reconfigure";

    return failed(-1);
}

    status = S826_AdcSlotlistRead(board_num_, &original_slotlist_);
    if (status != S826_ERR_OK) return failed(status);
    status = S826_AdcTrigModeRead(board_num_, &original_trigger_);
    if (status != S826_ERR_OK) return failed(status);
    for (uint slot = 0; slot < 16; ++slot) {
        auto& config = original_slots_[slot];
        status = S826_AdcSlotConfigRead(board_num_, slot, &config[0], &config[1], &config[2]);
        if (status != S826_ERR_OK) return failed(status);
    }
    snapshot_valid_ = true;
    return S826_ERR_OK;
}

int Sensoray826::verifyAdc(unsigned int enabled, unsigned int slots, unsigned int trigger)
{
    uint actual_enable = 0, actual_slots = 0, actual_trigger = 0;
    int status = S826_AdcEnableRead(board_num_, &actual_enable);
    if (status != S826_ERR_OK) return status;
    status = S826_AdcSlotlistRead(board_num_, &actual_slots);
    if (status != S826_ERR_OK) return status;
    status = S826_AdcTrigModeRead(board_num_, &actual_trigger);
    if (status != S826_ERR_OK) return status;
    return actual_enable == enabled && actual_slots == slots && actual_trigger == trigger ? S826_ERR_OK : -1;
}

int Sensoray826::close()
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_) return S826_ERR_OK;
    int first_error = S826_ERR_OK;
    auto record = [&first_error](int status) {
        if (first_error == S826_ERR_OK && status != S826_ERR_OK) first_error = status;
        return status == S826_ERR_OK;
    };
    if (snapshot_valid_ && (adc_control_touched_ || touched_slots_)) {
        uint enabled = 1;
        bool stopped = record(S826_AdcEnableWrite(board_num_, 0));
        stopped = record(S826_AdcEnableRead(board_num_, &enabled)) && stopped && enabled == 0;
        if (!stopped) record(-1); // Never reconfigure an ADC whose stop was not confirmed.
        else {
            for (uint slot = 0; slot < 16; ++slot) if (touched_slots_ & (1u << slot)) {
                const auto& config = original_slots_[slot];
                record(S826_AdcSlotConfigWrite(board_num_, slot, config[0], config[1], config[2]));
            }
            record(S826_AdcSlotlistWrite(board_num_, original_slotlist_, S826_BITWRITE));
            record(S826_AdcTrigModeWrite(board_num_, original_trigger_));
            record(verifyAdc(0, original_slotlist_, original_trigger_));
            for (uint slot = 0; slot < 16; ++slot) if (touched_slots_ & (1u << slot)) {
                std::array<uint, 3> actual{};
                record(S826_AdcSlotConfigRead(board_num_, slot, &actual[0], &actual[1], &actual[2]));
                if (actual != original_slots_[slot]) record(-1);
            }
        }
    }
    record(S826_SystemClose());
    system_open_ = adc_running_ = potentiometers_ = snapshot_valid_ = false;
    touched_slots_ = 0;
    adc_control_touched_ = false;
    sdk_owner = nullptr;
    ownership_.release();
    return first_error;
}

int Sensoray826::configureDac(unsigned int dac_channel)
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || dac_channel > 7) return -1;

    return S826_DacRangeWrite(
        board_num_,
        dac_channel,
        S826_DAC_SPAN_0_10,
        0
    );
}

int Sensoray826::writeDac(
    unsigned int dac_channel,
    unsigned int setpoint
)
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || dac_channel > 7 || setpoint > 0xFFFFu) return -1;

    return S826_DacDataWrite(
        board_num_,
        dac_channel,
        setpoint,
        0
    );
}

int Sensoray826::configureAdcSlot(
    unsigned int adc_slot,
    unsigned int adc_channel
)
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || adc_slot > 15 || adc_channel > 15) {
        return -1;
    }

    touched_slots_ |= (1u << adc_slot);
    adc_control_touched_ = true;
    int status = S826_AdcSlotConfigWrite(
        board_num_,
        adc_slot,
        adc_channel,
        ADC_SETTLING_TIME_US,
        S826_ADC_GAIN_1
    );

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotConfigWrite failed: "
                  << status << '\n';
        return status;
    }

    status = S826_AdcSlotlistWrite(
        board_num_,
        (1u << adc_slot),
        S826_BITSET
    );

    if (status != S826_ERR_OK) {
        std::cerr << "S826_AdcSlotlistWrite failed: "
                  << status << '\n';
    }

    return status;
}

int Sensoray826::startAdc()
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_) return -1;
    if (adc_running_) return S826_ERR_OK;

    adc_control_touched_ = true;
    int status = S826_AdcTrigModeWrite(board_num_, 0);
    if (status != S826_ERR_OK) return status;

    status = S826_AdcEnableWrite(board_num_, 1);

    if (status == S826_ERR_OK) {
        adc_running_ = true;
    }

    return status;
}

int Sensoray826::stopAdc()
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || !adc_running_) return S826_ERR_OK;

    int status = S826_AdcEnableWrite(board_num_, 0);

    if (status == S826_ERR_OK) {
        adc_running_ = false;
    }

    return status;
}

int Sensoray826::discardAdcSample(unsigned int adc_slot)
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || !adc_running_ || adc_slot > 15) return -1;

    int buf[16] = {};
    uint slotlist = (1u << adc_slot);

    int status = S826_AdcRead(
        board_num_,
        buf,
        nullptr,
        &slotlist,
        0
    );

    if (status == S826_ERR_OK || status == S826_ERR_NOTREADY) {
        return S826_ERR_OK;
    }
    // Preserve missed-trigger warnings rather than silently discarding them.

    return status;
}

int Sensoray826::readAdcVoltage(
    unsigned int adc_slot,
    double& voltage,
    unsigned int timeout_ms
)
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || !adc_running_ || adc_slot > 15) {
        return -1;
    }

    int buf[16] = {};
    uint slotlist = (1u << adc_slot);

    int status = S826_AdcRead(
        board_num_,
        buf,
        nullptr,
        &slotlist,
        timeout_ms
    );

    if (status != S826_ERR_OK &&
        status != S826_ERR_MISSEDTRIG) {
        return status;
    }
    if (status == S826_ERR_MISSEDTRIG) {
        std::cerr << "Warning: Sensoray ADC missed a trigger on slot "
                  << adc_slot << "\n";
    }

    if ((slotlist & (1u << adc_slot)) == 0) {
        return -1;
    }

    const short raw =
        static_cast<short>(buf[adc_slot] & 0xFFFF);

    voltage =
        static_cast<double>(raw) * 10.0 / 32767.0;

    return status;
}

unsigned int Sensoray826::boardNumber() const
{
    return board_num_;
}

bool Sensoray826::isOpen() const
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    return system_open_;
}

int Sensoray826::startPotentiometers(bool include_supply, unsigned int settling_us)
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    if (!system_open_ || adc_running_ || potentiometers_ || settling_us < 1 || settling_us > 100000) return -1;
    uint enabled = 1;
    int status = S826_AdcEnableRead(board_num_, &enabled);
    if (status != S826_ERR_OK || enabled) return status != S826_ERR_OK ? status : -1;
    include_supply_ = include_supply;
    potentiometer_mask_ = include_supply ? 0x70u : 0x30u;
    int values[16]{};
    uint timestamps[16]{}, available = potentiometer_mask_;
    status = S826_AdcRead(board_num_, values, timestamps, &available, 0);
    if (status != S826_ERR_OK && status != static_cast<int>(S826_ERR_NOTREADY) &&
        status != static_cast<int>(S826_ERR_MISSEDTRIG)) return status;
    for (uint i = 0; i < (include_supply ? 3u : 2u); ++i) {
        have_timestamp_[i] = (available & (1u << (i + 4))) != 0;
        previous_timestamp_[i] = timestamps[i + 4];
    }
    // Recheck immediately before the first configuration write.
    status = S826_AdcEnableRead(board_num_, &enabled);
    if (status != S826_ERR_OK || enabled) return status != S826_ERR_OK ? status : -1;
    adc_control_touched_ = true;
    for (uint i = 0; i < (include_supply ? 3u : 2u); ++i) {
        const uint slot = i + 4, channel = 11 - i;
        touched_slots_ |= 1u << slot;
        status = S826_AdcSlotConfigWrite(board_num_, slot, channel, settling_us, S826_ADC_GAIN_1);
        if (status != S826_ERR_OK) return status;
        std::array<uint, 3> actual{};
        status = S826_AdcSlotConfigRead(board_num_, slot, &actual[0], &actual[1], &actual[2]);
        if (status != S826_ERR_OK) return status;
        if (actual != std::array<uint, 3>{channel, settling_us, S826_ADC_GAIN_1}) return -1;
    }
    status = S826_AdcSlotlistWrite(board_num_, potentiometer_mask_, S826_BITWRITE);
    if (status != S826_ERR_OK) return status;
    status = startAdc();
    if (status != S826_ERR_OK) return status;
    status = verifyAdc(1, potentiometer_mask_, 0);
    if (status == S826_ERR_OK) potentiometers_ = true;
    return status;
}

PotentiometerSample Sensoray826::readPotentiometers()
{
    std::lock_guard<std::recursive_mutex> lock(sdk_mutex);
    PotentiometerSample result;
    result.voltage_v.fill(std::numeric_limits<double>::quiet_NaN());
    result.supply_voltage_v = std::numeric_limits<double>::quiet_NaN();
    if (!system_open_ || !potentiometers_ || !adc_running_) {
        result.sdk_status = -1;
        return result;
    }
    int values[16]{};
    uint timestamps[16]{}, available = potentiometer_mask_;
    result.sdk_status = S826_AdcRead(board_num_, values, timestamps, &available, 0);
    const bool readable = result.sdk_status == S826_ERR_OK ||
        result.sdk_status == static_cast<int>(S826_ERR_NOTREADY) ||
        result.sdk_status == static_cast<int>(S826_ERR_MISSEDTRIG);
    if (!readable || (available & ~potentiometer_mask_)) {
        if (readable) result.sdk_status = -1;
        return result;
    }
    for (uint i = 0; i < (include_supply_ ? 3u : 2u); ++i) {
        const uint slot = i + 4;
        if (!(available & (1u << slot))) continue;
        const uint timestamp = timestamps[slot];
        const uint delta = timestamp - previous_timestamp_[i];
        const bool fresh = !have_timestamp_[i] || (delta > 0 && delta < 0x80000000u);
        if (have_timestamp_[i] && delta >= 0x80000000u) timestamp_fault_[i] = true;
        if (fresh) {
            previous_timestamp_[i] = timestamp;
            have_timestamp_[i] = true;
            result.have_new_data = true;
        }
        const double voltage = static_cast<std::int16_t>(values[slot] & 0xFFFF) * 10.0 / 32767.0;
        const bool valid = fresh && !timestamp_fault_[i] && result.sdk_status != static_cast<int>(S826_ERR_MISSEDTRIG);
        if (i < 2) {
            result.voltage_v[i] = voltage;
            result.timestamps_us[i] = timestamp;
            result.valid[i] = valid;
        } else {
            result.supply_voltage_v = voltage;
            result.supply_timestamp_us = timestamp;
            result.supply_valid = valid;
        }
    }
    return result;



    
}


std::string Sensoray826::initializationError() const
{
    return initialization_error_;
}
// Exercise the actual Festo implementation with fake Sensoray members only.
// No Sensoray SDK library, board initialization, ROS, or hardware I/O is used.
#include "Festo.h"
#include "826api.h"

#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
std::vector<std::pair<unsigned, unsigned>> writes;
int dac_status = S826_ERR_OK;
unsigned cases = 0;
unsigned checks = 0;

void require(bool condition, const std::string& name)
{
    ++checks;
    if (!condition) throw std::runtime_error(name);
}

class Capture {
public:
    explicit Capture(std::ostream& stream)
        : stream_(stream), previous_(stream.rdbuf(output_.rdbuf())) {}
    ~Capture() {stream_.rdbuf(previous_);}
    std::string text() const {return output_.str();}
private:
    std::ostringstream output_;
    std::ostream& stream_;
    std::streambuf* previous_;
};

// Wrapper and explicit-verbose calls must preserve the existing CLI behavior.
enum class Mode { Wrapper, Verbose, Quiet };
constexpr std::array<Mode, 3> modes{Mode::Wrapper, Mode::Verbose, Mode::Quiet};

int command(Festo& festo, double pressure, Mode mode)
{
    return mode == Mode::Wrapper ? festo.setPressure(pressure) :
        festo.setPressure(pressure, mode == Mode::Verbose);
}

void testPressureMappingAndSuccessOutput()
{
    // Independent expected DAC codes: -50..50 kPa -> 0..5 V, with the
    // physical DAC still configured for 0..10 V and 16-bit rounding.
    const std::array<std::pair<double, unsigned>, 5> expected{{
        {-50.0, 0}, {-40.0, 3277}, {0.0, 16384}, {45.0, 31129}, {50.0, 32768}
    }};
    Sensoray826 sensoray;
    Festo festo(sensoray, 2, 13, 2);
    for (const auto& sample : expected) {
        std::string verbose_output;
        for (Mode mode : modes) {
            ++cases;
            writes.clear();
            dac_status = S826_ERR_OK;
            Capture stdout_capture(std::cout), stderr_capture(std::cerr);
            require(command(festo, sample.first, mode) == S826_ERR_OK,
                    "valid command retains its success result");
            require(writes.size() == 1 && writes[0].first == 2 &&
                    writes[0].second == sample.second,
                    "logging mode does not change DAC channel, code, or call count");
            require(stderr_capture.text().empty(), "successful command emits no error");
            if (mode == Mode::Quiet) {
                require(stdout_capture.text().empty(), "quiet success writes no stdout bytes");
            } else {
                const auto text = stdout_capture.text();
                require(text.find("Pressure command : ") != std::string::npos &&
                        text.find("Command voltage  : ") != std::string::npos &&
                        text.find("DAC setpoint     : ") != std::string::npos,
                        "CLI success output keeps all three existing diagnostic lines");
                if (mode == Mode::Wrapper) verbose_output = text;
                else require(text == verbose_output,
                             "legacy wrapper and explicit verbose output match exactly");
            }
        }
    }
}

void testRejectedPressureDoesNotWrite()
{
    const std::array<double, 5> invalid{{
        -50.001, 50.001, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()
    }};
    Sensoray826 sensoray;
    Festo festo(sensoray, 0, 15, 0);
    for (double pressure : invalid) {
        for (Mode mode : modes) {
            ++cases;
            writes.clear();
            Capture stdout_capture(std::cout), stderr_capture(std::cerr);
            require(command(festo, pressure, mode) == -1,
                    "invalid pressure retains the validation error result");
            require(writes.empty(), "invalid pressure never calls the DAC");
            require(stdout_capture.text().empty(), "invalid pressure emits no success output");
            require(stderr_capture.text().find("Pressure must be between ") != std::string::npos,
                    "quiet mode preserves validation errors");
        }
    }
}

void testDacErrorsArePreserved()
{
    Sensoray826 sensoray;
    Festo festo(sensoray, 1, 14, 1);
    for (int status : {-17, 17}) {
        for (Mode mode : modes) {
            ++cases;
            writes.clear();
            dac_status = status;
            Capture stdout_capture(std::cout), stderr_capture(std::cerr);
            require(command(festo, 0.0, mode) == status,
                    "DAC error code is returned without translation");
            require(writes.size() == 1 && writes[0] == std::make_pair(1U, 16384U),
                    "failed zero command retains the exact DAC request");
            require(stdout_capture.text().empty(), "failed DAC call emits no success output");
            require(stderr_capture.text() == "DAC write failed: " + std::to_string(status) + "\n",
                    "quiet mode preserves the original DAC error message");
        }
    }
    dac_status = S826_ERR_OK;
}

void testAllThreeQuietChannels()
{
    Sensoray826 sensoray;
    for (unsigned channel = 0; channel < 3; ++channel) {
        ++cases;
        Festo festo(sensoray, channel, 15 - channel, channel);
        writes.clear();
        Capture stdout_capture(std::cout), stderr_capture(std::cerr);
        require(festo.setPressure(0.0, false) == S826_ERR_OK,
                "every actuator channel supports a quiet zero command");
        require(writes.size() == 1 && writes[0] == std::make_pair(channel, 16384U),
                "quiet zero preserves the selected physical channel");
        require(stdout_capture.text().empty() && stderr_capture.text().empty(),
                "quiet successful three-channel zero writes have no console output");
    }
}
} // namespace

// Fake definitions satisfy the real festo.cpp object's references. These
// deliberately replace sensoray826.cpp and never link or call the SDK.
Sensoray826::Sensoray826(unsigned int board_num)
    : board_num_(board_num), system_open_(false), adc_running_(false) {}
Sensoray826::~Sensoray826() = default;
int Sensoray826::configureDac(unsigned int) {return S826_ERR_OK;}
int Sensoray826::configureAdcSlot(unsigned int, unsigned int) {return S826_ERR_OK;}
int Sensoray826::discardAdcSample(unsigned int) {return S826_ERR_OK;}
int Sensoray826::readAdcVoltage(unsigned int, double& voltage, unsigned int)
{
    voltage = 0.0;
    return S826_ERR_OK;
}
int Sensoray826::writeDac(unsigned int channel, unsigned int setpoint)
{
    writes.emplace_back(channel, setpoint);
    return dac_status;
}

int main()
{
    try {
        testPressureMappingAndSuccessOutput();
        testRejectedPressureDoesNotWrite();
        testDacErrorsArePreserved();
        testAllThreeQuietChannels();
    } catch (const std::exception& error) {
        std::cerr << "Festo offline regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Festo fake-I/O regression: " << cases << " cases and " << checks
              << " checks passed; no SDK or hardware access.\n";
}

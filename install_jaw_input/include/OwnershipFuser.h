#pragma once

#include <string>
#include <vector>

namespace sensoray826_ownership {
struct CommandResult {
    int exit_code = -1;
    bool completed = false;
    bool timed_out = false;
    std::string output;
    std::string error;
};

// Internal subprocess seam for hardware-free audit tests. Production calls only
// the fixed read-only sudo -n /usr/bin/fuser command, never a shell.
CommandResult runBoundedCommand(const std::vector<std::string>& arguments,
                               int timeout_ms = 2000);
bool verifiedClear(const CommandResult& result);
}

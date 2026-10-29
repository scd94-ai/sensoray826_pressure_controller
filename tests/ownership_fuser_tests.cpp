// Pure subprocess/audit classification tests. No SDK link or device access.
#include "OwnershipFuser.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace sensoray826_ownership;

void require(bool valid, const char* reason) {
    if (!valid) throw std::runtime_error(reason);
}

int main() {
    try {
        auto clear = runBoundedCommand({"/bin/sh", "-c", "exit 1"});
        require(verifiedClear(clear), "exit1 plus empty output establishes no holders");
        auto whitespace = runBoundedCommand({"/bin/sh", "-c", "printf ' \\n\\t'; exit 1"});
        require(verifiedClear(whitespace), "whitespace matches Python's stripped empty output");
        auto permission = runBoundedCommand({"/bin/sh", "-c", "printf 'sudo: a password is required\\n' >&2; exit 1"});
        require(!verifiedClear(permission) && permission.output.find("password") != std::string::npos,
                "sudo exit1 must never become verified clear");
        const int saved_stderr = ::dup(STDERR_FILENO);
        require(saved_stderr >= 0, "save stderr for closed-descriptor regression");
        ::close(STDERR_FILENO);
        auto closed_stderr = runBoundedCommand({"/bin/sh", "-c", "printf 'sudo permission denied\\n' >&2; exit 1"});
        ::dup2(saved_stderr, STDERR_FILENO);
        ::close(saved_stderr);
        require(!verifiedClear(closed_stderr) && closed_stderr.output.find("permission") != std::string::npos,
                "closed parent stderr must not hide audit failure and produce false clear");
        auto holder = runBoundedCommand({"/bin/sh", "-c", "printf '1234'; exit 0"});
        require(!verifiedClear(holder), "known device holders must block");
        auto empty_success = runBoundedCommand({"/bin/sh", "-c", "exit 0"});
        require(!verifiedClear(empty_success), "ambiguous exit0 must block even without PID output");
        auto unknown = runBoundedCommand({"/bin/sh", "-c", "exit 2"});
        require(!verifiedClear(unknown), "unknown exit status must block");
        auto missing = runBoundedCommand({"/nonexistent/sensoray-audit-test"});
        require(!verifiedClear(missing) && !missing.error.empty(), "missing executable must block");
        const auto before = std::chrono::steady_clock::now();
        auto timed_out = runBoundedCommand({"/bin/sh", "-c", "sleep 5"}, 100);
        require(timed_out.timed_out && !verifiedClear(timed_out), "hung audit must fail closed");
        require(std::chrono::steady_clock::now()-before < std::chrono::seconds(1),
                "audit timeout and process-group cleanup must be bounded");
        auto signaled = runBoundedCommand({"/bin/sh", "-c", "kill -TERM $$"});
        require(!verifiedClear(signaled) && !signaled.completed, "signaled audit must block");
        auto oversized = runBoundedCommand({"/usr/bin/python3", "-c", "import sys; sys.stdout.write('x'*70000); sys.exit(1)"});
        require(!verifiedClear(oversized) && !oversized.error.empty(), "oversized output must block");
        require(!verifiedClear(CommandResult{}), "unexecuted audit cannot establish clear ownership");
        std::cout << "PASS 12 bounded read-only audit subprocess/decision cases; no SDK or device\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

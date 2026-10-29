#include "BoardOwnership.h"
#include "OwnershipFuser.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <spawn.h>
#include <poll.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

extern char** environ;

namespace sensoray826_ownership {
CommandResult runBoundedCommand(const std::vector<std::string>& arguments, int timeout_ms) {
    CommandResult result;
    if (arguments.empty() || timeout_ms <= 0 || timeout_ms > 10000) {
        result.error = "Invalid audit command or timeout";
        return result;
    }
    int descriptors[2];
    if (::pipe2(descriptors, O_CLOEXEC) != 0) {
        result.error = std::strerror(errno);
        return result;
    }
    // A daemon may inherit a closed standard descriptor. Keep pipe endpoints
    // above stderr so later close actions cannot discard the captured errors.
    for (int& descriptor : descriptors) if (descriptor < 3) {
        const int duplicate = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
        if (duplicate < 0) {
            result.error = std::strerror(errno);
            ::close(descriptors[0]); ::close(descriptors[1]); return result;
        }
        ::close(descriptor);
        descriptor = duplicate;
    }
    if (::fcntl(descriptors[0], F_SETFL, O_NONBLOCK) < 0) {
        result.error = std::strerror(errno);
        ::close(descriptors[0]); ::close(descriptors[1]);
        return result;
    }
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int configured = ::posix_spawn_file_actions_init(&actions);
    if (configured != 0) {
        result.error = std::strerror(configured);
        ::close(descriptors[0]); ::close(descriptors[1]); return result;
    }
    const int attributes_initialized = ::posix_spawnattr_init(&attributes);
    if (attributes_initialized != 0) {
        result.error = std::strerror(attributes_initialized);
        ::posix_spawn_file_actions_destroy(&actions);
        ::close(descriptors[0]); ::close(descriptors[1]); return result;
    }
    auto configure = [&configured](int status) { if (!configured) configured = status; };
    configure(::posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO));
    configure(::posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDERR_FILENO));
    configure(::posix_spawn_file_actions_addclose(&actions, descriptors[0]));
    configure(::posix_spawn_file_actions_addclose(&actions, descriptors[1]));
    configure(::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP));
    configure(::posix_spawnattr_setpgroup(&attributes, 0));
    if (configured != 0) {
        result.error = std::strerror(configured);
        ::posix_spawn_file_actions_destroy(&actions);
        ::posix_spawnattr_destroy(&attributes);
        ::close(descriptors[0]); ::close(descriptors[1]); return result;
    }
    std::vector<char*> argv;
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    pid_t child = -1;
    const int spawned = ::posix_spawn(&child, argv[0], &actions, &attributes, argv.data(), environ);
    ::posix_spawn_file_actions_destroy(&actions);
    ::posix_spawnattr_destroy(&attributes);
    ::close(descriptors[1]);
    if (spawned != 0) {
        result.error = std::strerror(spawned);
        ::close(descriptors[0]);
        return result;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    bool reaped = false, eof = false;
    int status = 0;
    auto terminate = [&] {
        ::kill(-child, SIGKILL); // Also stop descendants when permissions allow.
        if (!reaped) ::kill(child, SIGKILL);
        const auto cleanup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (!reaped && std::chrono::steady_clock::now() < cleanup_deadline) {
            const pid_t waited = ::waitpid(child, &status, WNOHANG);
            if (waited == child || (waited < 0 && errno == ECHILD)) reaped = true;
            else if (waited < 0 && errno != EINTR) break;
            else ::poll(nullptr, 0, 5);
        }
        // A privileged helper can refuse an unprivileged signal. This path must
        // still return a failure within a bound, never wait forever or open SDK.
        if (!reaped) result.error += "; audit helper cleanup not confirmed";
    };
    while (true) {
        std::array<char, 4096> buffer{};
        while (!eof) {
            const ssize_t count = ::read(descriptors[0], buffer.data(), buffer.size());
            if (count > 0) {
                result.output.append(buffer.data(), static_cast<std::size_t>(count));
                if (result.output.size() > 65536) {
                    result.error = "Audit command output exceeded limit";
                    terminate(); ::close(descriptors[0]); return result;
                }
            } else if (count == 0) { eof = true; }
            else if (errno == EINTR) continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else {
                result.error = std::strerror(errno);
                terminate(); ::close(descriptors[0]); return result;
            }
        }
        if (!reaped) {
            const pid_t waited = ::waitpid(child, &status, WNOHANG);
            if (waited == child) reaped = true;
            else if (waited < 0 && errno != EINTR) {
                result.error = std::strerror(errno);
                terminate(); ::close(descriptors[0]); return result;
            }
        }
        if (reaped && eof) {
            result.completed = WIFEXITED(status);
            result.exit_code = result.completed ? WEXITSTATUS(status) : -1;
            if (!result.completed) result.error = "Audit command terminated by signal";
            ::close(descriptors[0]); return result;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.timed_out = true;
            result.error = "Read-only privileged fuser audit timed out";
            terminate(); ::close(descriptors[0]); return result;
        }
        pollfd descriptor{descriptors[0], POLLIN | POLLHUP, 0};
        ::poll(eof ? nullptr : &descriptor, eof ? 0 : 1, 10);
    }
}

bool verifiedClear(const CommandResult& result) {
    return result.completed && !result.timed_out && result.exit_code == 1 && result.error.empty() &&
        result.output.find_first_not_of(" \t\r\n\f\v") == std::string::npos;
}
}

namespace fs = std::filesystem;
namespace {
std::string readText(const fs::path& path) {
    std::ifstream stream(path);
    if (!stream) return {};
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
bool numeric(const std::string& text) {
    return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
}
}

BoardOwnership::~BoardOwnership() { release(); }

bool BoardOwnership::audit(unsigned int board) {
    const std::string init = readText("/proc/1/comm");
    if (init != "systemd\n" && init != "init\n") {
        error_ = "Host /proc namespace is not visible; refusing SDK open";
        return false;
    }
    const std::string device = "/dev/s826_" + std::to_string(board);
    struct stat expected{};
    if (::stat(device.c_str(), &expected) != 0 || !S_ISCHR(expected.st_mode)) {
        error_ = "Sensoray character device is not visible: " + device;
        return false;
    }
    std::istringstream sysfs(readText("/sys/class/s826/s826_" + std::to_string(board) + "/dev"));
    unsigned int sys_major = 0, sys_minor = 0;
    char colon = 0;
    if (!(sysfs >> sys_major >> colon >> sys_minor) || colon != ':' ||
        sys_major != major(expected.st_rdev) || sys_minor != minor(expected.st_rdev)) {
        error_ = "Sensoray device/sysfs identity mismatch";
        return false;
    }
    unsigned int limited = 0, limited_own_uid = 0;
    std::error_code iteration_error;
    fs::directory_iterator processes("/proc", iteration_error);
    if (iteration_error) {
        error_ = "Cannot enumerate host processes";
        return false;
    }
    for (const auto& process : processes) {
        const std::string pid = process.path().filename();
        if (!numeric(pid) || pid == std::to_string(::getpid())) continue;
        struct stat process_stat{};
        if (::stat(process.path().c_str(), &process_stat) != 0) continue;
        const bool own_uid = process_stat.st_uid == ::geteuid();
        bool unreadable = false;
        std::error_code fd_error;
        fs::directory_iterator descriptors(process.path() / "fd", fd_error);
        if (fd_error) unreadable = true;
        else for (const auto& descriptor : descriptors) {
            struct stat target{};
            if (::stat(descriptor.path().c_str(), &target) == 0 &&
                S_ISCHR(target.st_mode) && target.st_rdev == expected.st_rdev) {
                error_ = "Another process owns the Sensoray device: PID " + pid;
                return false;
            }
        }
        std::ifstream maps(process.path() / "maps");
        if (!maps) unreadable = true;
        else {
            std::string line;
            while (std::getline(maps, line)) {
                if (line.find("/dev/s826_") != std::string::npos ||
                    line.find("lib826_64.so") != std::string::npos) {
                    error_ = "Another process maps the Sensoray device/SDK: PID " + pid;
                    return false;
                }
            }
        }
        if (unreadable && fs::exists(process.path())) {
            if (own_uid) ++limited_own_uid;
            ++limited;
        }
    }
    if (limited) {
        // fuser checks open descriptors and mapped device accesses with host
        // privileges. Never treat sudo errors or ambiguous output as no users.
        const auto result = sensoray826_ownership::runBoundedCommand(
            {"/usr/bin/sudo", "-n", "/usr/bin/fuser", "-v", device});
        if (!sensoray826_ownership::verifiedClear(result)) {
            error_ = "Cannot verify Sensoray ownership for " + std::to_string(limited) +
                " unreadable processes (same UID: " + std::to_string(limited_own_uid) +
                "); privileged fuser did not establish no holders: " +
                (result.error.empty() ? "exit " + std::to_string(result.exit_code) : result.error);
            if (!result.output.empty()) error_ += "; " + result.output.substr(0, 512);
            return false;
        }
        limitations_ = "fd/maps inspection unavailable for " + std::to_string(limited) +
            " processes; privileged read-only fuser verified no device holders; ";
    } else limitations_.clear();
    limitations_ += "Advisory locking is cooperative, not OS device exclusivity";
    return true;
}

bool BoardOwnership::acquire(unsigned int board) {
    if (lock_fd_ >= 0) return true;
    if (!audit(board)) return false;
    const std::string path = "/tmp/sensoray826-adc-board-" + std::to_string(board) + ".lock";
    lock_fd_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock_fd_ < 0 || ::flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
        error_ = "Cannot acquire Sensoray advisory lock: " + std::string(std::strerror(errno));
        release();
        return false;
    }
    if (!audit(board)) { release(); return false; }
    return true;
}

void BoardOwnership::release() {
    if (lock_fd_ >= 0) {
        ::flock(lock_fd_, LOCK_UN);
        ::close(lock_fd_);
        lock_fd_ = -1;
    }
}

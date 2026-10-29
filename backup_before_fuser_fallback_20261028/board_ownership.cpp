#include "BoardOwnership.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>

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
    unsigned int limited = 0;
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
            if (own_uid) {
                error_ = "Cannot audit another process of this UID: PID " + pid;
                return false;
            }
            ++limited;
        }
    }
    limitations_ = limited ? "Ownership audit could not inspect " + std::to_string(limited) +
        " other-UID processes; advisory locking is cooperative, not OS device exclusivity" :
        "Advisory locking is cooperative, not OS device exclusivity";
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

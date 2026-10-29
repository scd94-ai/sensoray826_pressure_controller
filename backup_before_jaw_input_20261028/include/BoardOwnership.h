#pragma once

#include <string>

// Linux preflight plus a cooperative lock. The SDK/device itself is not exclusive.
class BoardOwnership {
public:
    BoardOwnership() = default;
    ~BoardOwnership();
    BoardOwnership(const BoardOwnership&) = delete;
    BoardOwnership& operator=(const BoardOwnership&) = delete;
    bool acquire(unsigned int board);
    void release();
    const std::string& error() const { return error_; }
    const std::string& limitations() const { return limitations_; }
private:
    bool audit(unsigned int board);
    int lock_fd_ = -1;
    std::string error_;
    std::string limitations_;
};

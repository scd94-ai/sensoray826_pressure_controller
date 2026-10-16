#pragma once
// Bindings for Jiaji's supplied soft_actuator_msgs definitions.
#include <soft_actuator_msgs/msg/pressure_command.hpp>
#include <soft_actuator_msgs/msg/pressure_state.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>

namespace sensoray826_ros {
using Command = soft_actuator_msgs::msg::PressureCommand;
using State = soft_actuator_msgs::msg::PressureState;
struct ParsedCommand {
  std::array<double,3> pressure{};
  std::string session;
  std::uint64_t sequence = 0;
  std::chrono::nanoseconds validity{};
  bool force_zero = false;
};

inline ParsedCommand parseCommand(const Command& msg) {
  ParsedCommand p;
  p.pressure = {msg.pressure_kpa[0], msg.pressure_kpa[1], msg.pressure_kpa[2]};
  p.session = msg.session_id;
  p.sequence = msg.sequence;
  // ROS Duration is a signed int32 seconds field plus uint32 nanoseconds.
  p.validity = std::chrono::seconds(msg.valid_for.sec) +
               std::chrono::nanoseconds(msg.valid_for.nanosec);
  p.force_zero = msg.force_zero;
  return p;
}

inline State makeState(
    const std::string& session,
    std::uint64_t sequence,
    const std::array<double,3>& written,
    bool write_success,
    bool ready,
    const std::string& fault) {
  State msg;
  msg.session_id = session;
  msg.command_sequence = sequence;
  msg.written_pressure_kpa = written;
  msg.write_success = write_success;
  msg.ready = ready;
  msg.fault = fault;
  return msg;
}
} // namespace sensoray826_ros

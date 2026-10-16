#pragma once

// IMPORTANT: The public soft-actuator-control-core repository does not provide
// the custom ROS message .msg definitions. These are provisional bindings.
// Check these member names against:
//   ros2 interface show soft_actuator_msgs/msg/PressureCommand
//   ros2 interface show soft_actuator_msgs/msg/PressureState
// Only this header should need adjustment to match the existing workspace.

#include <soft_actuator_msgs/msg/pressure_command.hpp>
#include <soft_actuator_msgs/msg/pressure_state.hpp>
#include <array>
#include <cstdint>
#include <string>

namespace sensoray826_ros {
using Command = soft_actuator_msgs::msg::PressureCommand;
using State = soft_actuator_msgs::msg::PressureState;
struct ParsedCommand {
  std::array<double,3> pressure{};
  std::uint64_t session=0;
  std::uint64_t sequence=0;
  std::uint64_t validity_ms=0;
  bool force_zero=false;
};

inline ParsedCommand parseCommand(const Command& msg) {
  ParsedCommand p;
  // FIELD NAMES TO VERIFY against the installed custom interfaces.
  p.pressure = {msg.pressure_kpa[0], msg.pressure_kpa[1], msg.pressure_kpa[2]};
  p.session = msg.session_id;
  p.sequence = msg.sequence_id;
  p.validity_ms = msg.validity_duration_ms;
  p.force_zero = msg.force_zero;
  return p;
}

inline State makeState(
    std::uint64_t session,
    std::uint64_t sequence,
    const std::array<double,3>& last_complete_write,
    bool ready,
    const std::string& fault) {
  State msg;
  // FIELD NAMES TO VERIFY against the installed custom interfaces.
  msg.session_id = session;
  msg.sequence_id = sequence;
  msg.pressure_kpa = last_complete_write;
  msg.ready = ready;
  msg.fault = fault;
  return msg;
}
} // namespace sensoray826_ros

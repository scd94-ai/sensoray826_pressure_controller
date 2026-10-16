#include "Festo.h"
#include "Sensoray826.h"
#include "sensoray826_ros/message_adapter.hpp"
#include "rclcpp/rclcpp.hpp"
#include "826api.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

class SensorayNode final : public rclcpp::Node {
public:
  SensorayNode()
      : Node("sensoray826_node"),
        board_(0),
        festo0_(board_, 0, 15, 0, -50., 50., 0., 5.),
        festo1_(board_, 1, 14, 1, -50., 50., 0., 5.),
        festo2_(board_, 2, 13, 2, -50., 50., 0., 5.),
        festos_{&festo0_, &festo1_, &festo2_} {
    state_pub_ = create_publisher<sensoray826_ros::State>(
        "/soft_haptics/right/sensoray_state", rclcpp::QoS(10).reliable());

    if (board_.initialize() != S826_ERR_OK)
      throw std::runtime_error("Sensoray 826 board failed to initialize");

    try {
      for (std::size_t i = 0; i < festos_.size(); ++i) {
        if (festos_[i]->initialize() != S826_ERR_OK)
          throw std::runtime_error("Festo initialization failure");
        ++initialized_;
      }
      // We do not need ADC samples to acknowledge successful DAC writes.
      ready_ = true;
      publishState();
    } catch (...) {
      forceZeroAll();
      board_.close();
      throw;
    }

    // Subscription is created only after hardware startup is complete.
    command_sub_ = create_subscription<sensoray826_ros::Command>(
        "/soft_haptics/right/pressure_command", rclcpp::QoS(10).reliable(),
        [this](sensoray826_ros::Command::ConstSharedPtr msg) {
          onCommand(*msg);
        });
    watchdog_ = create_wall_timer(20ms, [this] { onWatchdog(); });
    RCLCPP_INFO(get_logger(), "Sensoray node ready (3 FESTOs, 0..5 V command)");
  }

  ~SensorayNode() override {
    forceZeroAll(); // Best effort. ROS shutdown is not a hardware safety interlock.
    board_.close();
  }

private:
  using Clock = std::chrono::steady_clock;
  static constexpr double kMinKPa = -40.0;
  static constexpr double kMaxKPa = 50.0;
  // This adapter independently enforces the core's maximum per-write delta.
  static constexpr double kMaxDeltaKPa = 20.0;
  static constexpr auto kMaxTtl = std::chrono::seconds(1);

  void publishState(bool write_success = false) {
    // This is a host-side write acknowledgement, NOT a physical pressure reading.
    auto msg = sensoray826_ros::makeState(
        session_, sequence_, last_complete_write_, write_success, ready_, fault_);
    msg.header.stamp = get_clock()->now();
    state_pub_->publish(msg);
  }

  bool forceZeroAll() {
    bool all_ok = true;
    for (std::size_t i = 0; i < initialized_; ++i) {
      if (festos_[i]->setPressure(0.0) != S826_ERR_OK)
        all_ok = false;
    }
    if (all_ok && initialized_ == 3)
      last_complete_write_ = {0., 0., 0.};
    if (!all_ok) {
      ready_ = false;
      fault_ = "zero_write_failed";
    }
    return all_ok && initialized_ == 3;
  }

  void failSafe(const std::string& reason) {
    ready_ = false;
    fault_ = reason;
    forceZeroAll();
    publishState(false);
  }

  void onCommand(const sensoray826_ros::Command& msg) {
    // Sensoray I/O blocks in this callback; run this node with a single
    // threaded executor to serialize writes and the watchdog.
    const auto now = Clock::now();
    const auto command = sensoray826_ros::parseCommand(msg);
    if (command.session.empty() || command.sequence == 0) {
      fault_ = "invalid_command_identity";
      publishState(false);
      return;
    }
    if (have_command_ && command.session == session_ &&
        command.sequence <= sequence_) {
      return; // Reject duplicate and out-of-order commands.
    }
    // For accepted commands, the acknowledgement must echo these exact IDs,
    // including rejected writes.
    session_ = command.session;
    sequence_ = command.sequence;
    have_command_ = true;
    if (!ready_) {
      publishState(false);
      return; // Fault requires restart, not implicit re-arm.
    }
    if (command.validity <= std::chrono::nanoseconds::zero() ||
        command.validity > kMaxTtl) {
      failSafe("invalid_command_ttl");
      return;
    }

    if (!command.force_zero) {
      for (std::size_t i = 0; i < 3; ++i) {
        const double p = command.pressure[i];
        if (!std::isfinite(p) || p < kMinKPa || p > kMaxKPa) {
          failSafe("invalid_pressure_command");
          return;
        }
      }
    }

    // The arrival time is all we know; this cannot detect a message delayed
    // before receipt unless the message schema includes source timestamps.
    expires_at_ = now + command.validity;

    std::array<double,3> target =
        command.force_zero ? std::array<double,3>{0.,0.,0.} : command.pressure;

    if (!command.force_zero) {
      for (std::size_t i = 0; i < 3; ++i) {
        if (std::abs(target[i] - last_complete_write_[i]) > kMaxDeltaKPa) {
          failSafe("pressure_step_exceeds_20kpa");
          return;
        }
      }
    }

    for (std::size_t i = 0; i < 3; ++i) {
      if (festos_[i]->setPressure(target[i]) != S826_ERR_OK) {
        // Partial transaction: never report target as a complete write.
        failSafe("partial_dac_write_failed");
        return;
      }
    }
    last_complete_write_ = target;
    fault_.clear();
    publishState(true);
  }

  void onWatchdog() {
    if (have_command_ && ready_ && Clock::now() >= expires_at_)
      failSafe("pressure_command_expired");
  }

  Sensoray826 board_;
  Festo festo0_, festo1_, festo2_;
  std::array<Festo*,3> festos_;
  std::size_t initialized_ = 0;
  bool ready_ = false;
  bool have_command_ = false;
  std::string session_;
  std::uint64_t sequence_ = 0;
  std::array<double,3> last_complete_write_{0.,0.,0.};
  Clock::time_point expires_at_{};
  std::string fault_;
  rclcpp::Subscription<sensoray826_ros::Command>::SharedPtr command_sub_;
  rclcpp::Publisher<sensoray826_ros::State>::SharedPtr state_pub_;
  rclcpp::TimerBase::SharedPtr watchdog_;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<SensorayNode>();
    rclcpp::spin(node); // Single-threaded: serializes writes and watchdog.
  } catch (const std::exception& ex) {
    RCLCPP_FATAL(rclcpp::get_logger("sensoray826_node"), "%s", ex.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}

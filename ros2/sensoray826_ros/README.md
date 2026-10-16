# Sensoray 826 ROS 2 node (Humble / Jazzy)

This package wraps the existing `Sensoray826` and `Festo` classes and deliberately
does not modify the standalone executable.

## Topics

- Subscribes: `/soft_haptics/right/pressure_command` — `soft_actuator_msgs/msg/PressureCommand`
- Publishes: `/soft_haptics/right/sensoray_state` — `soft_actuator_msgs/msg/PressureState`

## Important integration step: message fields

The public `soft-actuator-control-core` repository has **no ROS message
definitions**. The existing `soft_actuator_msgs` package in Jiaji's private/local
workspace owns the actual definitions. The file
`include/sensoray826_ros/message_adapter.hpp` currently uses **provisional**
field names: `pressure_kpa`, `session_id`, `sequence_id`,
`validity_duration_ms`, `force_zero`, and for the state
`pressure_kpa`, `ready`, `fault`. **Do not expect compilation until
these are reconciled against the actual interface.**

Run:

```bash
source /opt/ros/humble/setup.bash   # Or jazzy; use one distro consistently.
cd ~/Documents/jxs1778/soft_actuator_ros2_ws
source install/setup.bash          # If already built.
ros2 interface show soft_actuator_msgs/msg/PressureCommand
ros2 interface show soft_actuator_msgs/msg/PressureState
```

Update only `message_adapter.hpp` using the actual member names and types.

## Add to Jiaji's workspace

A simple symlink preserves your standalone repo:

```bash
cd ~/Documents/jxs1778/soft_actuator_ros2_ws/src
ln -s ~/Documents/sensoray826/ros2/sensoray826_ros sensoray826_ros
cd ..
source /opt/ros/humble/setup.bash  # Or jazzy, consistently.
colcon build --packages-select sensoray826_ros --symlink-install
source install/setup.bash
ros2 run sensoray826_ros sensoray_node
```

The library location is the same Sensoray SDK used by the original CMake
project. If SDK discovery fails, pass `--cmake-args
-DS826_API_INCLUDE_DIR=/your/sdk/include
-DS826_LIBRARY=/your/sdk/middleware/lib826_64.so` to colcon.

## Operational behavior

- One Sensoray board and three FESTOs; DAC 0/1/2; AIN 15/14/13.
- Every initialized FESTO receives a zero-kPa startup command.
- No ADC measurement or empirical feedback value is used as write acknowledgment.
- Incoming nonzero pressure must be finite and within **[-40, +50] kPa**.
- The adapter currently rejects steps greater than 20 kPa relative to the
  last *complete successful triple write*. Do not apply the core output-policy
  limiter a second time here if the upstream controller already applies it
  and the contract requires a different behavior.
- Commands include bounded validity duration; an expired command attempts
  to write zero on all three channels and latches a fault until restart.
- A partial DAC failure attempts zero on all channels and latches a fault.
- The state includes only the **last fully completed triple**. A partial
  write means physical channel states can differ; `ready=false` and the
  fault string flag this condition.
- Force-zero bypasses pressure/delta checks, but still requires a valid TTL.
- A new session ID is accepted; if session authentication/lifecycle rules
  are specified elsewhere, add them before physical deployment.
- Arrival-based expiry does not account for transport delay before receipt.
  If the actual command message has a creation time, use it for expiry.
- ROS shutdown is best-effort only; a separate emergency stop and hardware
  pressure-relief design must address process crashes and power loss.
- This adapter has not been tested with ROS, the private message types, or
  live pneumatics. Bench validation is mandatory before actuation.

The provisional Festo analog feedback mapping remains unchanged in the
standalone hardware classes; it is not used to claim physical pressure.

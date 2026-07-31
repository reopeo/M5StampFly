#include <stdint.h>

#include <string>

#include <unity.h>

#include "flight_control_source.hpp"
#include "telemetry_source.hpp"

namespace {
// This is intentionally a source-level guard: the native environment cannot
// execute the ESP32 control loop. It verifies the observation gate's textual
// ordering and required zeroing operations, but it cannot prove interrupt
// timing, compiler optimization, or PWM electrical behavior. Any control-flow
// refactor should keep this test aligned with the approved observation gate.
std::string read_source(const char* source_file) {
    // __FILE__ is an absolute path in PlatformIO's native test build. Keeping
    // the lookup relative to this test prevents a machine-specific project
    // path from being embedded in the test itself.
    (void)source_file;
    return std::string(FLIGHT_CONTROL_SOURCE);
}

std::string function_body(const std::string& source, const std::string& signature) {
    const size_t signature_pos = source.find(signature);
    if (signature_pos == std::string::npos) return std::string();
    const size_t open_brace = source.find('{', signature_pos);
    if (open_brace == std::string::npos) return std::string();
    size_t depth = 0;
    for (size_t pos = open_brace; pos < source.size(); ++pos) {
        if (source[pos] == '{') {
            ++depth;
        } else if (source[pos] == '}' && --depth == 0) {
            return source.substr(open_brace + 1, pos - open_brace - 1);
        }
    }
    return std::string();
}
}  // namespace

void test_observation_loop_has_unconditional_safety_gate(void) {
    const std::string source = read_source(__FILE__);
    TEST_ASSERT_FALSE_MESSAGE(source.empty(), "cannot read flight_control.cpp");
    const std::string loop = function_body(source, "void loop_400Hz(void)");
    TEST_ASSERT_FALSE_MESSAGE(loop.empty(), "loop_400Hz body is missing");
    const size_t enforce = loop.find("observation_safety_enforce();");
    const size_t telemetry = loop.find("telemetry_capture_imu();");
    TEST_ASSERT_TRUE_MESSAGE(enforce != std::string::npos, "observation safety call is missing");
    TEST_ASSERT_TRUE_MESSAGE(telemetry != std::string::npos, "telemetry capture call is missing");
    TEST_ASSERT_TRUE_MESSAGE(enforce < telemetry, "telemetry precedes observation safety enforcement");

    const size_t return_pos = loop.find("return;", telemetry);
    TEST_ASSERT_TRUE_MESSAGE(return_pos != std::string::npos, "observation loop return is missing");
    TEST_ASSERT_TRUE_MESSAGE(telemetry < return_pos, "observation loop does not return after telemetry capture");

    // The legacy flight state machine is below this return and must remain
    // unreachable from the observation loop.
    const size_t legacy_flight = loop.find("Mode == FLIGHT_MODE", telemetry);
    TEST_ASSERT_TRUE_MESSAGE(legacy_flight == std::string::npos || return_pos < legacy_flight,
                             "legacy flight branch moved before the safety return");
}

void test_observation_safety_enforce_clears_outputs_and_state(void) {
    const std::string source = read_source(__FILE__);
    TEST_ASSERT_FALSE_MESSAGE(source.empty(), "cannot read flight_control.cpp");
    const std::string safety = function_body(source, "void observation_safety_enforce(void) {");
    TEST_ASSERT_FALSE_MESSAGE(safety.empty(), "observation safety body is missing");
    const char* required[] = {
        "Mode = PARKING_MODE;",       "Control_mode = ANGLECONTROL;", "Throttle_control_mode = 0;",
        "Thrust_command = 0.0f;",     "Thrust_command2 = 0.0f;",       "Thrust0 = 0.0f;",
        "Roll_rate_command = 0.0f;",  "Pitch_rate_command = 0.0f;",    "Yaw_rate_command = 0.0f;",
        "Roll_angle_command = 0.0f;", "Pitch_angle_command = 0.0f;",   "Yaw_angle_command = 0.0f;",
        "FrontRight_motor_duty = 0.0f;", "FrontLeft_motor_duty = 0.0f;",
        "RearRight_motor_duty = 0.0f;", "RearLeft_motor_duty = 0.0f;", "p_pid.reset();",
        "q_pid.reset();", "r_pid.reset();", "phi_pid.reset();", "theta_pid.reset();", "alt_pid.reset();",
        "z_dot_pid.reset();", "motor_stop();",
    };
    for (const char* requirement : required) {
        TEST_ASSERT_TRUE_MESSAGE(safety.find(requirement) != std::string::npos, requirement);
    }
}

void test_telemetry_timestamps_have_status_barrier_ordering(void) {
    const std::string source = std::string(TELEMETRY_SOURCE);
    const std::string queue = function_body(source, "void queue_imu_sample(const queued_imu_sample_t& sample) {");
    const std::string barrier = function_body(source, "status_barrier_t capture_status_barrier(void) {");
    const std::string status = function_body(source, "void send_status(WiFiUDP& udp) {");
    TEST_ASSERT_FALSE_MESSAGE(queue.empty(), "queue_imu_sample body is missing");
    TEST_ASSERT_FALSE_MESSAGE(barrier.empty(), "capture_status_barrier body is missing");
    TEST_ASSERT_FALSE_MESSAGE(status.empty(), "send_status body is missing");
    TEST_ASSERT_TRUE_MESSAGE(queue.find("portENTER_CRITICAL(&imu_queue_mux);") != std::string::npos,
                             "IMU queue lock is missing");
    TEST_ASSERT_TRUE_MESSAGE(queue.find("captured.acquisition_time_us = static_cast<uint64_t>(esp_timer_get_time());") !=
                                 std::string::npos,
                             "IMU timestamp is not captured under queue lock");
    TEST_ASSERT_TRUE_MESSAGE(barrier.find("portENTER_CRITICAL(&imu_queue_mux);") != std::string::npos,
                             "status barrier lock is missing");
    TEST_ASSERT_TRUE_MESSAGE(barrier.find("barrier.uptime_us = static_cast<uint64_t>(esp_timer_get_time());") !=
                                 std::string::npos,
                             "status timestamp is not captured under queue lock");
    TEST_ASSERT_TRUE_MESSAGE(status.find("capture_status_barrier();") != std::string::npos,
                             "status barrier is not used before encoding");
    TEST_ASSERT_TRUE_MESSAGE(status.find("status.uptime_us = barrier.uptime_us;") != std::string::npos,
                             "status header/payload timestamp source changed");
}

void test_telemetry_sender_drains_before_status_and_yields_when_busy(void) {
    const std::string source = std::string(TELEMETRY_SOURCE);
    const std::string drain = function_body(source, "uint8_t drain_imu_samples(WiFiUDP& udp) {");
    const std::string sender = function_body(source, "void telemetry_sender_task(void*) {");
    TEST_ASSERT_FALSE_MESSAGE(drain.empty(), "drain_imu_samples body is missing");
    TEST_ASSERT_FALSE_MESSAGE(sender.empty(), "telemetry sender body is missing");

    TEST_ASSERT_TRUE_MESSAGE(drain.find("sent < IMU_QUEUE_CAPACITY") != std::string::npos,
                             "IMU drain is not bounded");
    const size_t first_drain = sender.find("work_count = drain_imu_samples(udp);");
    const size_t status = sender.find("send_status(udp);");
    const size_t second_drain = sender.find("work_count = static_cast<uint8_t>(work_count + drain_imu_samples(udp));");
    TEST_ASSERT_TRUE_MESSAGE(first_drain != std::string::npos, "status pre-drain is missing");
    TEST_ASSERT_TRUE_MESSAGE(status != std::string::npos, "status send is missing");
    TEST_ASSERT_TRUE_MESSAGE(second_drain != std::string::npos, "normal IMU drain is missing");
    TEST_ASSERT_TRUE_MESSAGE(first_drain < status && status < second_drain,
                             "sender order is not drain then status then drain");
    TEST_ASSERT_TRUE_MESSAGE(sender.find("if (work_count == 0) vTaskDelay(1);") != std::string::npos,
                             "idle delay guard is missing");
    TEST_ASSERT_TRUE_MESSAGE(sender.find("else taskYIELD();") != std::string::npos,
                             "busy sender yield is missing");
    TEST_ASSERT_TRUE_MESSAGE(sender.find("} else {\n            vTaskDelay(1);") != std::string::npos,
                             "disconnected idle delay is missing");
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_observation_loop_has_unconditional_safety_gate);
    RUN_TEST(test_observation_safety_enforce_clears_outputs_and_state);
    RUN_TEST(test_telemetry_timestamps_have_status_barrier_ordering);
    RUN_TEST(test_telemetry_sender_drains_before_status_and_yields_when_busy);
    return UNITY_END();
}

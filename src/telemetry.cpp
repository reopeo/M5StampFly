#include "telemetry.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <math.h>

#include "flight_control.hpp"
#include "protocol.hpp"
#include "sensor.hpp"

#ifndef DRONE_WIFI_SSID
#define DRONE_WIFI_SSID ""
#endif
#ifndef DRONE_WIFI_PASSWORD
#define DRONE_WIFI_PASSWORD ""
#endif
#ifndef DRONE_PC_IP
#define DRONE_PC_IP ""
#endif
#ifndef DRONE_UDP_PORT
#define DRONE_UDP_PORT 5600
#endif

namespace {
constexpr uint8_t IMU_QUEUE_CAPACITY = 8;
constexpr float GRAVITY_MPS2 = 9.80665f;
constexpr uint32_t WIFI_BACKOFF_MAX_MS = 30000;

struct queued_imu_sample_t {
    protocol_imu_sample_t sample;
    uint64_t acquisition_time_us;
};

queued_imu_sample_t imu_queue[IMU_QUEUE_CAPACITY];
portMUX_TYPE imu_queue_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t imu_queue_head = 0;
volatile uint8_t imu_queue_count = 0;
volatile bool imu_queue_overflow = false;
volatile uint32_t imu_queue_drop_count = 0;
volatile uint32_t udp_send_failure_count = 0;
volatile uint32_t wifi_reconnect_count = 0;
volatile uint8_t wifi_state = 0;

uint64_t source_id = 0;
uint32_t session_id = 1;
uint32_t next_sequence = 0;
uint32_t last_imu_sequence = 0;
IPAddress pc_address;
bool pc_address_valid = false;

uint32_t backoff_ms = 1000;
uint32_t next_connect_attempt_ms = 0;
bool wifi_connected_once = false;

bool parse_pc_address(void) {
    return pc_address.fromString(DRONE_PC_IP);
}

void queue_imu_sample(const queued_imu_sample_t& sample) {
    portENTER_CRITICAL(&imu_queue_mux);
    if (imu_queue_count == IMU_QUEUE_CAPACITY) {
        imu_queue_head = static_cast<uint8_t>((imu_queue_head + 1) % IMU_QUEUE_CAPACITY);
        imu_queue_count--;
        imu_queue_drop_count++;
        imu_queue_overflow = true;
    }
    const uint8_t tail = static_cast<uint8_t>((imu_queue_head + imu_queue_count) % IMU_QUEUE_CAPACITY);
    imu_queue[tail] = sample;
    imu_queue_count++;
    portEXIT_CRITICAL(&imu_queue_mux);
}

bool take_imu_sample(queued_imu_sample_t* sample, bool* overflow) {
    bool available = false;
    portENTER_CRITICAL(&imu_queue_mux);
    if (imu_queue_count != 0) {
        *sample = imu_queue[imu_queue_head];
        imu_queue_head = static_cast<uint8_t>((imu_queue_head + 1) % IMU_QUEUE_CAPACITY);
        imu_queue_count--;
        *overflow = imu_queue_overflow;
        imu_queue_overflow = false;
        available = true;
    }
    portEXIT_CRITICAL(&imu_queue_mux);
    return available;
}

uint32_t queue_drop_count_snapshot(void) {
    portENTER_CRITICAL(&imu_queue_mux);
    const uint32_t result = imu_queue_drop_count;
    portEXIT_CRITICAL(&imu_queue_mux);
    return result;
}

void start_wifi_connect(void) {
    if (!pc_address_valid || DRONE_WIFI_SSID[0] == '\0') {
        wifi_state = 0;
        return;
    }
    wifi_state = 1;
    WiFi.begin(DRONE_WIFI_SSID, DRONE_WIFI_PASSWORD);
}

void maintain_wifi(uint32_t now_ms) {
    if (WiFi.status() == WL_CONNECTED) {
        if (wifi_state != 2) {
            wifi_state = 2;
            if (wifi_connected_once) wifi_reconnect_count++;
            wifi_connected_once = true;
        }
        backoff_ms = 1000;
        return;
    }

    if (wifi_state == 2) wifi_state = 0;
    if (static_cast<int32_t>(now_ms - next_connect_attempt_ms) < 0) return;

    start_wifi_connect();
    next_connect_attempt_ms = now_ms + backoff_ms;
    backoff_ms = min(backoff_ms * 2, WIFI_BACKOFF_MAX_MS);
}

bool send_datagram(WiFiUDP& udp, const uint8_t* data, size_t length) {
    if (!pc_address_valid || udp.beginPacket(pc_address, DRONE_UDP_PORT) != 1) return false;
    if (udp.write(data, length) != length) return false;
    return udp.endPacket() == 1;
}

void send_status(WiFiUDP& udp) {
    protocol_status_t status = {};
    status.uptime_us = static_cast<uint64_t>(esp_timer_get_time());
    status.last_imu_sequence = last_imu_sequence;
    status.imu_queue_drop_count = queue_drop_count_snapshot();
    status.udp_send_failure_count = udp_send_failure_count;
    status.wifi_reconnect_count = wifi_reconnect_count;
    status.wifi_state = wifi_state;
    status.health_flags = 0;
    status.rssi_dbm = static_cast<int8_t>(WiFi.RSSI());
    status.wifi_channel = static_cast<uint8_t>(WiFi.channel());
    status.last_error_code = 0;

    uint8_t datagram[PROTOCOL_STATUS_DATAGRAM_SIZE];
    const size_t length = protocol_encode_status(datagram, sizeof(datagram), source_id, session_id, next_sequence++,
                                                 status.uptime_us, status);
    if (!send_datagram(udp, datagram, length)) udp_send_failure_count++;
}

void telemetry_sender_task(void*) {
    WiFiUDP udp;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    pc_address_valid = parse_pc_address();
    uint32_t next_status_ms = 0;
    uint8_t previous_wifi_state = 0xff;

    for (;;) {
        const uint32_t now_ms = millis();
        maintain_wifi(now_ms);
        if (wifi_state == 2) {
            if (wifi_state != previous_wifi_state || static_cast<int32_t>(now_ms - next_status_ms) >= 0) {
                send_status(udp);
                next_status_ms = now_ms + 1000;
            }

            queued_imu_sample_t queued;
            bool overflow = false;
            if (take_imu_sample(&queued, &overflow)) {
                if (overflow) queued.sample.health_flags |= 0x04;
                uint8_t datagram[PROTOCOL_IMU_DATAGRAM_SIZE];
                const uint32_t sequence = next_sequence++;
                const size_t length = protocol_encode_imu(datagram, sizeof(datagram), source_id, session_id, sequence,
                                                          queued.acquisition_time_us, queued.sample);
                last_imu_sequence = sequence;
                if (!send_datagram(udp, datagram, length)) udp_send_failure_count++;
            }
        }
        previous_wifi_state = wifi_state;
        vTaskDelay(1);
    }
}
}  // namespace

void telemetry_init(void) {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    for (uint8_t byte : mac) source_id = (source_id << 8) | byte;
    session_id = esp_random();
    if (session_id == 0) session_id = 1;
    xTaskCreatePinnedToCore(telemetry_sender_task, "telemetry_sender", 4096, nullptr, 1, nullptr, 0);
}

void telemetry_capture_imu(void) {
    if (wifi_state != 2) return;

    queued_imu_sample_t queued = {};
    queued.acquisition_time_us = static_cast<uint64_t>(esp_timer_get_time());
    queued.sample.accel_x = Accel_x_raw * GRAVITY_MPS2;
    queued.sample.accel_y = -Accel_y_raw * GRAVITY_MPS2;
    queued.sample.accel_z = -Accel_z_raw * GRAVITY_MPS2;
    queued.sample.gyro_x = Roll_rate_raw;
    queued.sample.gyro_y = -Pitch_rate_raw;
    queued.sample.gyro_z = -Yaw_rate_raw;
    queued.sample.sample_period_us = static_cast<uint32_t>(Interval_time * 1000000.0f);
    queued.sample.health_flags = static_cast<uint8_t>(0x0a | (Imu_read_valid != 0 ? 0x01 : 0x00));

    if (!isfinite(queued.sample.accel_x) || !isfinite(queued.sample.accel_y) || !isfinite(queued.sample.accel_z) ||
        !isfinite(queued.sample.gyro_x) || !isfinite(queued.sample.gyro_y) || !isfinite(queued.sample.gyro_z)) {
        queued.sample.health_flags &= static_cast<uint8_t>(~0x01U);
    }
    queue_imu_sample(queued);
}

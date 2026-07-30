#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <stddef.h>
#include <stdint.h>

constexpr size_t PROTOCOL_HEADER_SIZE = 40;
constexpr size_t PROTOCOL_CRC_SIZE = 4;
constexpr size_t PROTOCOL_IMU_PAYLOAD_SIZE = 32;
constexpr size_t PROTOCOL_STATUS_PAYLOAD_SIZE = 32;
constexpr size_t PROTOCOL_IMU_DATAGRAM_SIZE = PROTOCOL_HEADER_SIZE + PROTOCOL_IMU_PAYLOAD_SIZE + PROTOCOL_CRC_SIZE;
constexpr size_t PROTOCOL_STATUS_DATAGRAM_SIZE =
    PROTOCOL_HEADER_SIZE + PROTOCOL_STATUS_PAYLOAD_SIZE + PROTOCOL_CRC_SIZE;

constexpr uint32_t PROTOCOL_MAGIC = 0x4456494fU;
constexpr uint8_t PROTOCOL_MAJOR = 1;
constexpr uint8_t PROTOCOL_MINOR = 0;
constexpr uint8_t PROTOCOL_FC_IMU_SAMPLE = 0x01;
constexpr uint8_t PROTOCOL_FC_STATUS = 0x02;
constexpr uint8_t PROTOCOL_PARKING_MODE = 3;

struct protocol_imu_sample_t {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    uint32_t sample_period_us;
    uint8_t health_flags;
};

struct protocol_status_t {
    uint64_t uptime_us;
    uint32_t last_imu_sequence;
    uint32_t imu_queue_drop_count;
    uint32_t udp_send_failure_count;
    uint32_t wifi_reconnect_count;
    uint8_t wifi_state;
    uint16_t health_flags;
    int8_t rssi_dbm;
    uint8_t wifi_channel;
    uint16_t last_error_code;
};

uint32_t protocol_crc32c(const uint8_t* data, size_t length);
size_t protocol_encode_imu(uint8_t* output, size_t capacity, uint64_t source_id, uint32_t session_id,
                           uint32_t sequence, uint64_t acquisition_time_us, const protocol_imu_sample_t& sample);
size_t protocol_encode_status(uint8_t* output, size_t capacity, uint64_t source_id, uint32_t session_id,
                              uint32_t sequence, uint64_t acquisition_time_us, const protocol_status_t& status);

#endif

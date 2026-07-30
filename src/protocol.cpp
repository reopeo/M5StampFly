#include "protocol.hpp"

#include <string.h>

namespace {
void write_u16(uint8_t* output, uint16_t value) {
    output[0] = static_cast<uint8_t>(value >> 8);
    output[1] = static_cast<uint8_t>(value);
}

void write_u32(uint8_t* output, uint32_t value) {
    output[0] = static_cast<uint8_t>(value >> 24);
    output[1] = static_cast<uint8_t>(value >> 16);
    output[2] = static_cast<uint8_t>(value >> 8);
    output[3] = static_cast<uint8_t>(value);
}

void write_u64(uint8_t* output, uint64_t value) {
    write_u32(output, static_cast<uint32_t>(value >> 32));
    write_u32(output + 4, static_cast<uint32_t>(value));
}

void write_f32(uint8_t* output, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32(output, bits);
}

void write_header(uint8_t* output, uint8_t message_type, uint16_t payload_length, uint64_t source_id,
                  uint32_t session_id, uint32_t sequence, uint64_t acquisition_time_us) {
    write_u32(output + 0, PROTOCOL_MAGIC);
    output[4] = PROTOCOL_MAJOR;
    output[5] = PROTOCOL_MINOR;
    output[6] = message_type;
    output[7] = PROTOCOL_HEADER_SIZE;
    write_u16(output + 8, payload_length);
    write_u16(output + 10, 0);
    write_u64(output + 12, source_id);
    write_u32(output + 20, session_id);
    write_u32(output + 24, sequence);
    write_u64(output + 28, acquisition_time_us);
    write_u32(output + 36, 0);
}

size_t finish_datagram(uint8_t* output, size_t length_without_crc) {
    write_u32(output + length_without_crc, protocol_crc32c(output, length_without_crc));
    return length_without_crc + PROTOCOL_CRC_SIZE;
}
}  // namespace

uint32_t protocol_crc32c(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffffU;
    while (length-- != 0U) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) != 0U ? 0x82f63b78U : 0U);
        }
    }
    return ~crc;
}

size_t protocol_encode_imu(uint8_t* output, size_t capacity, uint64_t source_id, uint32_t session_id,
                           uint32_t sequence, uint64_t acquisition_time_us, const protocol_imu_sample_t& sample) {
    if (capacity < PROTOCOL_IMU_DATAGRAM_SIZE) return 0;

    write_header(output, PROTOCOL_FC_IMU_SAMPLE, PROTOCOL_IMU_PAYLOAD_SIZE, source_id, session_id, sequence,
                 acquisition_time_us);
    uint8_t* payload = output + PROTOCOL_HEADER_SIZE;
    write_f32(payload + 0, sample.accel_x);
    write_f32(payload + 4, sample.accel_y);
    write_f32(payload + 8, sample.accel_z);
    write_f32(payload + 12, sample.gyro_x);
    write_f32(payload + 16, sample.gyro_y);
    write_f32(payload + 20, sample.gyro_z);
    write_u32(payload + 24, sample.sample_period_us);
    payload[28] = PROTOCOL_PARKING_MODE;
    payload[29] = sample.health_flags;
    write_u16(payload + 30, 0);
    return finish_datagram(output, PROTOCOL_HEADER_SIZE + PROTOCOL_IMU_PAYLOAD_SIZE);
}

size_t protocol_encode_status(uint8_t* output, size_t capacity, uint64_t source_id, uint32_t session_id,
                              uint32_t sequence, uint64_t acquisition_time_us, const protocol_status_t& status) {
    if (capacity < PROTOCOL_STATUS_DATAGRAM_SIZE) return 0;

    write_header(output, PROTOCOL_FC_STATUS, PROTOCOL_STATUS_PAYLOAD_SIZE, source_id, session_id, sequence,
                 acquisition_time_us);
    uint8_t* payload = output + PROTOCOL_HEADER_SIZE;
    write_u64(payload + 0, status.uptime_us);
    write_u32(payload + 8, status.last_imu_sequence);
    write_u32(payload + 12, status.imu_queue_drop_count);
    write_u32(payload + 16, status.udp_send_failure_count);
    write_u32(payload + 20, status.wifi_reconnect_count);
    payload[24] = PROTOCOL_PARKING_MODE;
    payload[25] = status.wifi_state;
    write_u16(payload + 26, status.health_flags);
    payload[28] = static_cast<uint8_t>(status.rssi_dbm);
    payload[29] = status.wifi_channel;
    write_u16(payload + 30, status.last_error_code);
    return finish_datagram(output, PROTOCOL_HEADER_SIZE + PROTOCOL_STATUS_PAYLOAD_SIZE);
}

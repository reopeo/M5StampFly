#include <stdint.h>

#include <string.h>

#include <unity.h>

#include "../../src/protocol.cpp"

namespace {
uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return static_cast<uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<uint8_t>(value - 'a' + 10);
    return 0xff;
}

void assert_canonical_datagram(const uint8_t* actual, size_t length, const char* fixture) {
    TEST_ASSERT_EQUAL_UINT32(length * 2, strlen(fixture));
    for (size_t i = 0; i < length; ++i) {
        const uint8_t high = hex_nibble(fixture[i * 2]);
        const uint8_t low = hex_nibble(fixture[i * 2 + 1]);
        TEST_ASSERT_NOT_EQUAL(0xff, high);
        TEST_ASSERT_NOT_EQUAL(0xff, low);
        TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((high << 4) | low), actual[i]);
    }
}
}  // namespace

void test_crc32c_known_answer(void) {
    const uint8_t input[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX32(0xe3069283U, protocol_crc32c(input, sizeof(input)));
}

void test_imu_encoding_is_fixed_big_endian(void) {
    protocol_imu_sample_t sample = {};
    sample.accel_x = 9.80665f;
    sample.accel_y = -9.80665f;
    sample.gyro_z = 1.0f;
    sample.sample_period_us = 2500;
    sample.health_flags = 0x09;
    uint8_t datagram[PROTOCOL_IMU_DATAGRAM_SIZE] = {};

    const size_t size = protocol_encode_imu(datagram, sizeof(datagram), 0x112233445566ULL, 0x12345678U,
                                            0x90abcdefU, 0x0102030405060708ULL, sample);

    TEST_ASSERT_EQUAL_UINT32(PROTOCOL_IMU_DATAGRAM_SIZE, size);
    TEST_ASSERT_EQUAL_HEX8(0x44, datagram[0]);
    TEST_ASSERT_EQUAL_HEX8(0x4f, datagram[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01, datagram[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, datagram[12]);
    TEST_ASSERT_EQUAL_HEX8(0x11, datagram[14]);
    TEST_ASSERT_EQUAL_HEX8(0x66, datagram[19]);
    TEST_ASSERT_EQUAL_HEX8(0x12, datagram[20]);
    TEST_ASSERT_EQUAL_HEX8(0x78, datagram[23]);
    TEST_ASSERT_EQUAL_HEX8(0x90, datagram[24]);
    TEST_ASSERT_EQUAL_HEX8(0xef, datagram[27]);
    TEST_ASSERT_EQUAL_HEX8(0x01, datagram[28]);
    TEST_ASSERT_EQUAL_HEX8(0x08, datagram[35]);
    TEST_ASSERT_EQUAL_HEX8(PROTOCOL_PARKING_MODE, datagram[68]);
    TEST_ASSERT_EQUAL_HEX8(0x09, datagram[69]);
    TEST_ASSERT_EQUAL_HEX32(protocol_crc32c(datagram, 72),
                            (static_cast<uint32_t>(datagram[72]) << 24) |
                                (static_cast<uint32_t>(datagram[73]) << 16) |
                                (static_cast<uint32_t>(datagram[74]) << 8) | datagram[75]);
}

void test_status_encoding_uses_parking_mode(void) {
    protocol_status_t status = {};
    status.uptime_us = 0x0102030405060708ULL;
    status.last_imu_sequence = 7;
    status.wifi_state = 2;
    status.rssi_dbm = -42;
    status.wifi_channel = 6;
    uint8_t datagram[PROTOCOL_STATUS_DATAGRAM_SIZE] = {};

    const size_t size =
        protocol_encode_status(datagram, sizeof(datagram), 1, 2, 3, status.uptime_us, status);

    TEST_ASSERT_EQUAL_UINT32(PROTOCOL_STATUS_DATAGRAM_SIZE, size);
    TEST_ASSERT_EQUAL_HEX8(PROTOCOL_FC_STATUS, datagram[6]);
    TEST_ASSERT_EQUAL_HEX8(PROTOCOL_PARKING_MODE, datagram[64]);
    TEST_ASSERT_EQUAL_HEX8(0x02, datagram[65]);
    TEST_ASSERT_EQUAL_INT8(-42, static_cast<int8_t>(datagram[68]));
    TEST_ASSERT_EQUAL_HEX8(0x06, datagram[69]);
}

void test_encoder_rejects_insufficient_capacity(void) {
    protocol_imu_sample_t imu = {};
    protocol_status_t status = {};
    uint8_t imu_datagram[PROTOCOL_IMU_DATAGRAM_SIZE] = {};
    uint8_t status_datagram[PROTOCOL_STATUS_DATAGRAM_SIZE] = {};

    TEST_ASSERT_EQUAL_UINT32(0, protocol_encode_imu(imu_datagram, sizeof(imu_datagram) - 1, 1, 2, 3, 4, imu));
    TEST_ASSERT_EQUAL_UINT32(0,
                             protocol_encode_status(status_datagram, sizeof(status_datagram) - 1, 1, 2, 3, 4,
                                                     status));
}

void test_encoder_matches_canonical_imu_fixture(void) {
    protocol_imu_sample_t sample = {};
    sample.accel_x = 9.80665f;
    sample.accel_y = -9.80665f;
    sample.gyro_z = 1.0f;
    sample.sample_period_us = 2500;
    sample.health_flags = 0x09;
    uint8_t datagram[PROTOCOL_IMU_DATAGRAM_SIZE] = {};
    const size_t size = protocol_encode_imu(datagram, sizeof(datagram), 0x112233445566ULL, 0x12345678U,
                                            0x90abcdefU, 0x0102030405060708ULL, sample);
    const char* fixture =
        "4456494f010001280020000000001122334455661234567890abcdef010203040506070800000000411ce80ac11ce80a"
        "0000000000000000000000003f800000000009c403090000d19d9446";
    TEST_ASSERT_EQUAL_UINT32(PROTOCOL_IMU_DATAGRAM_SIZE, size);
    assert_canonical_datagram(datagram, size, fixture);
}

void test_encoder_matches_canonical_status_fixture(void) {
    protocol_status_t status = {};
    status.uptime_us = 0x0102030405060708ULL;
    status.last_imu_sequence = 7;
    status.wifi_state = 2;
    status.rssi_dbm = -42;
    status.wifi_channel = 6;
    uint8_t datagram[PROTOCOL_STATUS_DATAGRAM_SIZE] = {};
    const size_t size = protocol_encode_status(datagram, sizeof(datagram), 0x112233445566ULL, 0x12345678U,
                                               0x90abcdefU, 0x0102030405060708ULL, status);
    const char* fixture =
        "4456494f010002280020000000001122334455661234567890abcdef0102030405060708000000000102030405060708"
        "0000000700000000000000000000000003020000d6060000edb4949d";
    TEST_ASSERT_EQUAL_UINT32(PROTOCOL_STATUS_DATAGRAM_SIZE, size);
    assert_canonical_datagram(datagram, size, fixture);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc32c_known_answer);
    RUN_TEST(test_imu_encoding_is_fixed_big_endian);
    RUN_TEST(test_status_encoding_uses_parking_mode);
    RUN_TEST(test_encoder_rejects_insufficient_capacity);
    RUN_TEST(test_encoder_matches_canonical_imu_fixture);
    RUN_TEST(test_encoder_matches_canonical_status_fixture);
    return UNITY_END();
}

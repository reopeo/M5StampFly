#include <stdint.h>

#include <unity.h>

#include "../../src/protocol.cpp"

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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc32c_known_answer);
    RUN_TEST(test_imu_encoding_is_fixed_big_endian);
    RUN_TEST(test_status_encoding_uses_parking_mode);
    return UNITY_END();
}

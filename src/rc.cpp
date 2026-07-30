#include "rc.hpp"

// Observation firmware has no RC or ESP-NOW transport. These inert definitions
// retain legacy control symbols while the safety profile makes them unreachable.
volatile uint16_t Connect_flag = 40;
volatile float Stick[16] = {};
volatile uint8_t Rc_err_flag = 0;
volatile uint8_t MyMacAddr[6] = {};
volatile uint8_t Recv_MAC[3] = {};

void rc_init(void) {}
void rc_demo(void) {}
void rc_end(void) {}
uint8_t rc_isconnected(void) {
    return 0;
}
uint8_t telemetry_send(uint8_t*, uint16_t) {
    return 1;
}
void send_peer_info(void) {}

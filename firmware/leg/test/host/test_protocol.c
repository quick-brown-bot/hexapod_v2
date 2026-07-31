// Host unit test for the RS485 protocol module. No hardware dependencies.
//
// Run on any machine with a C compiler:
//   gcc -I../../src ../../src/protocol.c test_protocol.c -o test_protocol && ./test_protocol
//
// (A host compiler was not available in the dev environment when this was
// written; the logic is also validated by a Python reference check and the
// firmware build compiles protocol.c on-target.)

#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static int approx(float a, float b) { return fabsf(a - b) < 0.05f; }

int main(void)
{
    // --- CRC test vectors from docs/interfaces/RS485_PROTOCOL.md ---
    CHECK(proto_crc8((const uint8_t *)">1,00,-45.5,-30.2,60.0", 22) == 0xF5, "crc pull basic = F5");
    CHECK(proto_crc8((const uint8_t *)"<1,00,1250,450,380,420", 22) == 0x71, "crc resp basic = 71");

    // --- Parse a basic pull frame ---
    {
        char line[] = ">1,00,-45.5,-30.2,60.0*F5\n";
        proto_pull_t p;
        CHECK(proto_parse_pull(line, &p), "parse basic pull");
        CHECK(p.addr == 1, "addr == 1");
        CHECK(p.flags == 0x00, "flags == 0");
        CHECK(approx(p.coxa_deg, -45.5f), "coxa == -45.5");
        CHECK(approx(p.femur_deg, -30.2f), "femur == -30.2");
        CHECK(approx(p.tibia_deg, 60.0f), "tibia == 60.0");
        CHECK(p.n_params == 0, "no params");
    }

    // --- Parse a pull with JOINT_POS flag and two config params ---
    {
        char line[] = ">1,01,-45.5,-30.2,60.0,P01=10,P02=500*3A\n";
        proto_pull_t p;
        CHECK(proto_parse_pull(line, &p), "parse pull w/ flags+params");
        CHECK(p.flags == PROTO_FLAG_JOINT_POS, "JOINT_POS set");
        CHECK(p.n_params == 2, "two params");
        CHECK(p.params[0].id == 0x01 && p.params[0].value == 10, "P01=10");
        CHECK(p.params[1].id == 0x02 && p.params[1].value == 500, "P02=500");
    }

    // --- Reject a corrupted CRC ---
    {
        char line[] = ">1,00,-45.5,-30.2,60.0*00\n";
        proto_pull_t p;
        CHECK(!proto_parse_pull(line, &p), "reject bad CRC");
    }

    // --- Build a basic response and check exact bytes ---
    {
        proto_response_t r = {0};
        r.addr = 1; r.status = 0;
        r.current_total_ma = 1250; r.current_coxa_ma = 450;
        r.current_femur_ma = 380; r.current_tibia_ma = 420;
        char buf[128];
        int n = proto_build_response(&r, buf, sizeof(buf));
        CHECK(n > 0, "build response ok");
        CHECK(strcmp(buf, "<1,00,1250,450,380,420*71\n") == 0, "response bytes match vector");
    }

    // --- Round-trip: build then parse is not applicable (different directions),
    //     but verify response with positions builds with the documented CRC ---
    {
        proto_response_t r = {0};
        r.addr = 1; r.status = 0;
        r.current_total_ma = 1250; r.current_coxa_ma = 450;
        r.current_femur_ma = 380; r.current_tibia_ma = 420;
        r.include_positions = true;
        r.pos_coxa_deg = -44.8f; r.pos_femur_deg = -29.5f; r.pos_tibia_deg = 59.2f;
        char buf[128];
        proto_build_response(&r, buf, sizeof(buf));
        CHECK(strcmp(buf, "<1,00,1250,450,380,420,-44.8,-29.5,59.2*77\n") == 0,
              "response w/ positions matches vector");
    }

    printf("\n%s (%d failures)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail);
    return g_fail ? 1 : 0;
}

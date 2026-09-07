#include "protocol.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

uint8_t proto_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

// Strip a single trailing '\n' (and '\r') if present, in place.
static void strip_eol(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
        line[--n] = '\0';
    }
}

bool proto_parse_pull(char *line, proto_pull_t *out)
{
    if (!line || !out) return false;
    if (line[0] != '>') return false;

    strip_eol(line);

    char *star = strchr(line, '*');
    if (!star) return false;

    size_t body_len = (size_t)(star - line);
    uint8_t want = proto_crc8((const uint8_t *)line, body_len);
    uint8_t got = (uint8_t)strtoul(star + 1, NULL, 16);
    if (want != got) return false;

    // Terminate the body at '*' and tokenize after the leading '>'.
    *star = '\0';
    char *body = line + 1;

    char *tok[5 + PROTO_MAX_PARAMS];
    int ntok = 0;
    for (char *s = strtok(body, ","); s && ntok < (int)(sizeof(tok) / sizeof(tok[0]));
         s = strtok(NULL, ",")) {
        tok[ntok++] = s;
    }
    if (ntok < 5) return false; // addr,flags,coxa,femur,tibia minimum

    memset(out, 0, sizeof(*out));
    out->addr      = (uint8_t)atoi(tok[0]);
    out->flags     = (uint8_t)strtoul(tok[1], NULL, 16);
    out->coxa_deg  = strtof(tok[2], NULL);
    out->femur_deg = strtof(tok[3], NULL);
    out->tibia_deg = strtof(tok[4], NULL);

    // Remaining tokens are config entries: "Pii=vvv".
    for (int i = 5; i < ntok; ++i) {
        char *t = tok[i];
        if (t[0] != 'P') return false;
        char *eq = strchr(t, '=');
        if (!eq) return false;
        *eq = '\0';
        if (out->n_params >= PROTO_MAX_PARAMS) return false;
        out->params[out->n_params].id    = (uint8_t)strtoul(t + 1, NULL, 16);
        out->params[out->n_params].value = (int32_t)atol(eq + 1);
        out->n_params++;
    }

    return true;
}

// Formats `v` to one decimal place using integer arithmetic instead of
// snprintf's %f -- this build's printf silently emits an empty string for
// float specifiers rather than the expected text (confirmed on the wire:
// the mainboard was receiving "<1,00,0,0,0,0,,,*A4", positions present as
// bare commas with nothing between them), so `%.1f` here was quietly
// dropping every position field even though include_positions was true.
static void format_1dp(float v, char *buf, size_t buf_sz)
{
    bool neg = v < 0.0f;
    float av = neg ? -v : v;
    int32_t scaled = (int32_t)(av * 10.0f + 0.5f); // round to nearest 0.1
    snprintf(buf, buf_sz, "%s%ld.%ld", neg ? "-" : "", (long)(scaled / 10), (long)(scaled % 10));
}

int proto_build_response(const proto_response_t *r, char *buf, size_t buf_sz)
{
    if (!r || !buf) return -1;

    int n;
    if (r->include_positions) {
        char pc[16], pf[16], pt[16];
        format_1dp(r->pos_coxa_deg, pc, sizeof(pc));
        format_1dp(r->pos_femur_deg, pf, sizeof(pf));
        format_1dp(r->pos_tibia_deg, pt, sizeof(pt));
        n = snprintf(buf, buf_sz, "<%u,%02X,%u,%u,%u,%u,%s,%s,%s",
                     (unsigned)r->addr, (unsigned)r->status,
                     (unsigned)r->current_total_ma, (unsigned)r->current_coxa_ma,
                     (unsigned)r->current_femur_ma, (unsigned)r->current_tibia_ma,
                     pc, pf, pt);
    } else {
        n = snprintf(buf, buf_sz, "<%u,%02X,%u,%u,%u,%u",
                     (unsigned)r->addr, (unsigned)r->status,
                     (unsigned)r->current_total_ma, (unsigned)r->current_coxa_ma,
                     (unsigned)r->current_femur_ma, (unsigned)r->current_tibia_ma);
    }
    if (n < 0 || (size_t)n >= buf_sz) return -1;

    uint8_t crc = proto_crc8((const uint8_t *)buf, (size_t)n);
    int m = snprintf(buf + n, buf_sz - n, "*%02X\n", crc);
    if (m < 0 || (size_t)(n + m) >= buf_sz) return -1;
    return n + m;
}

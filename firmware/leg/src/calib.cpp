#include "calib.h"
#include "persist.h"
#include "config.h"

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

#define CALIB_LINE_MAX 64
#define FW_IDENT "hexapod-leg fw v1"

static char   s_line[CALIB_LINE_MAX];
static size_t s_len = 0;

static void print_help(void)
{
    Serial.println(F("commands:"));
    Serial.println(F("  PING        -> PONG <fw>"));
    Serial.println(F("  ADDR?       -> ADDR=<n>"));
    Serial.println(F("  ADDR <1-6>  -> set & persist leg address"));
    Serial.println(F("  STATUS?     -> address + identity"));
    Serial.println(F("  HELP"));
}

static void handle_line(char *line)
{
    // Trim leading spaces.
    while (*line == ' ') ++line;

    if (strcmp(line, "PING") == 0) {
        Serial.println(F("PONG " FW_IDENT));
    } else if (strcmp(line, "HELP") == 0) {
        print_help();
    } else if (strcmp(line, "ADDR?") == 0) {
        Serial.print(F("ADDR="));
        Serial.println(persist_get_address());
    } else if (strncmp(line, "ADDR ", 5) == 0) {
        int a = atoi(line + 5);
        if (persist_set_address((uint8_t)a)) {
            Serial.print(F("OK ADDR="));
            Serial.println(persist_get_address());
        } else {
            Serial.println(F("ERR address out of range (1-6)"));
        }
    } else if (strcmp(line, "STATUS?") == 0) {
        Serial.print(F("fw=" FW_IDENT " addr="));
        Serial.println(persist_get_address());
    } else if (line[0] != '\0') {
        Serial.println(F("ERR unknown command (try HELP)"));
    }
}

void calib_init(void)
{
    Serial.begin(115200);
    s_len = 0;
}

void calib_poll(void)
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_len > 0) {
                s_line[s_len] = '\0';
                handle_line(s_line);
                s_len = 0;
            }
        } else if (s_len + 1 < CALIB_LINE_MAX) {
            s_line[s_len++] = c;
        } else {
            s_len = 0; // overflow: resync
        }
    }
}

#include "persist.h"
#include "config.h"

#include <EEPROM.h>
#include <string.h>

// arduino-pico EEPROM is a RAM-backed flash sector emulation; commit() writes
// the sector. A small record is plenty for the leg identity.

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t  leg_addr;
    uint8_t  pad;
} persist_record_t;

#define EEPROM_SIZE 256

static persist_record_t s_rec;

static void write_record(void)
{
    EEPROM.put(0, s_rec);
    EEPROM.commit();
}

void persist_init(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, s_rec);

    if (s_rec.magic != PERSIST_MAGIC || s_rec.version != PERSIST_VERSION) {
        memset(&s_rec, 0, sizeof(s_rec));
        s_rec.magic = PERSIST_MAGIC;
        s_rec.version = PERSIST_VERSION;
        s_rec.leg_addr = DEFAULT_LEG_ADDR;
        write_record();
    }
}

uint8_t persist_get_address(void)
{
    return s_rec.leg_addr;
}

bool persist_set_address(uint8_t addr)
{
    if (addr < 1 || addr > 6) return false;
    s_rec.leg_addr = addr;
    write_record();
    return true;
}

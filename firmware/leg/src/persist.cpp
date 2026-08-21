#include "persist.h"
#include "config.h"

#include <EEPROM.h>
#include <string.h>

// arduino-pico EEPROM is a RAM-backed flash sector emulation; commit() writes
// the whole sector regardless of which partition changed (the RP2040 has no
// finer-grained flash write here) -- the two records below are a *logical*
// partitioning for independent validation/reset, not separate physical wear.

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t  leg_addr;
    uint8_t  pad;
} persist_identity_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t pad;
    current_calib_t current_calib[NUM_CURRENT_CHANNELS];
    int32_t  filter_mode;  // CURRENT_FILTER_EMA / CURRENT_FILTER_BOXCAR
    float    ema_alpha;
    int32_t  boxcar_n;
    int32_t  pwm_neutral_us[NUM_JOINTS];  // v4+: per-joint PWM center, see persist_get_pwm_neutral_us()
    int8_t   invert[NUM_JOINTS];          // v5+: per-joint sign, see persist_get_invert()
} persist_calib_t;

#define EEPROM_SIZE            256
#define EEPROM_OFFSET_IDENTITY 0
#define EEPROM_OFFSET_CALIB    32  // well clear of persist_identity_t, small partition

static persist_identity_t s_identity;
static persist_calib_t    s_calib;

static void write_identity(void)
{
    EEPROM.put(EEPROM_OFFSET_IDENTITY, s_identity);
    EEPROM.commit();
}

static void write_calib(void)
{
    EEPROM.put(EEPROM_OFFSET_CALIB, s_calib);
    EEPROM.commit();
}

void persist_init(void)
{
    EEPROM.begin(EEPROM_SIZE);

    EEPROM.get(EEPROM_OFFSET_IDENTITY, s_identity);
    if (s_identity.magic != PERSIST_IDENTITY_MAGIC || s_identity.version != PERSIST_IDENTITY_VERSION) {
        memset(&s_identity, 0, sizeof(s_identity));
        s_identity.magic = PERSIST_IDENTITY_MAGIC;
        s_identity.version = PERSIST_IDENTITY_VERSION;
        s_identity.leg_addr = DEFAULT_LEG_ADDR; // 0 = uncalibrated
        write_identity();
    }

    EEPROM.get(EEPROM_OFFSET_CALIB, s_calib);
    if (s_calib.magic == PERSIST_CALIB_MAGIC && s_calib.version == 4) {
        // v4 -> v5: persist_calib_t only gained a trailing invert[] field:
        // every earlier field (current calib, filter, pwm_neutral_us) is
        // still at the same offset and still valid, so just default the new
        // field and bump the version in place instead of wiping calibration
        // that already exists on this board.
        for (int j = 0; j < NUM_JOINTS; ++j) {
            s_calib.invert[j] = 1;
        }
        s_calib.version = PERSIST_CALIB_VERSION;
        write_calib();
    } else if (s_calib.magic != PERSIST_CALIB_MAGIC || s_calib.version != PERSIST_CALIB_VERSION) {
        memset(&s_calib, 0, sizeof(s_calib));
        s_calib.magic = PERSIST_CALIB_MAGIC;
        s_calib.version = PERSIST_CALIB_VERSION;
        for (int i = 0; i < NUM_CURRENT_CHANNELS; ++i) {
            s_calib.current_calib[i].scale_ma_per_mv = DEFAULT_ISENSE_MA_PER_MV;
            s_calib.current_calib[i].offset_ma = DEFAULT_ISENSE_OFFSET_MA;
        }
        s_calib.filter_mode = DEFAULT_CURRENT_FILTER_MODE;
        s_calib.ema_alpha = DEFAULT_CURRENT_EMA_ALPHA;
        s_calib.boxcar_n = DEFAULT_CURRENT_BOXCAR_N;
        for (int j = 0; j < NUM_JOINTS; ++j) {
            s_calib.pwm_neutral_us[j] = DEFAULT_PWM_NEUTRAL_US;
            s_calib.invert[j] = 1;
        }
        write_calib();
    }
}

uint8_t persist_get_address(void)
{
    return s_identity.leg_addr;
}

bool persist_set_address(uint8_t addr)
{
    if (addr < 1 || addr > 6) return false;
    s_identity.leg_addr = addr;
    write_identity();
    return true;
}

current_calib_t persist_get_current_calib(int channel)
{
    if (channel < 0 || channel >= NUM_CURRENT_CHANNELS) {
        current_calib_t def = { DEFAULT_ISENSE_MA_PER_MV, DEFAULT_ISENSE_OFFSET_MA };
        return def;
    }
    return s_calib.current_calib[channel];
}

bool persist_set_current_calib(int channel, float scale_ma_per_mv, float offset_ma)
{
    if (channel < 0 || channel >= NUM_CURRENT_CHANNELS) return false;
    s_calib.current_calib[channel].scale_ma_per_mv = scale_ma_per_mv;
    s_calib.current_calib[channel].offset_ma = offset_ma;
    write_calib();
    return true;
}

int persist_get_current_filter_mode(void)
{
    return (int)s_calib.filter_mode;
}

float persist_get_current_ema_alpha(void)
{
    return s_calib.ema_alpha;
}

int persist_get_current_boxcar_n(void)
{
    return (int)s_calib.boxcar_n;
}

bool persist_set_current_filter(int mode, float ema_alpha, int boxcar_n)
{
    s_calib.filter_mode = mode;
    s_calib.ema_alpha = ema_alpha;
    s_calib.boxcar_n = boxcar_n;
    write_calib();
    return true;
}

int32_t persist_get_pwm_neutral_us(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return DEFAULT_PWM_NEUTRAL_US;
    return s_calib.pwm_neutral_us[joint];
}

bool persist_set_pwm_neutral_us(int joint, int32_t pwm_neutral_us)
{
    if (joint < 0 || joint >= NUM_JOINTS) return false;
    if (pwm_neutral_us < DEFAULT_PWM_MIN_US || pwm_neutral_us > DEFAULT_PWM_MAX_US) return false;
    s_calib.pwm_neutral_us[joint] = pwm_neutral_us;
    write_calib();
    return true;
}

int8_t persist_get_invert(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return 1;
    return s_calib.invert[joint];
}

bool persist_set_invert(int joint, int8_t invert)
{
    if (joint < 0 || joint >= NUM_JOINTS) return false;
    if (invert != 1 && invert != -1) return false;
    s_calib.invert[joint] = invert;
    write_calib();
    return true;
}

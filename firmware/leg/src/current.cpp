#include "current.h"
#include "persist.h"
#include "config.h"

#include "hardware/adc.h"

static current_reading_t s_latest;
static current_raw_t     s_latest_raw;
static current_calib_t   s_calib[NUM_CURRENT_CHANNELS];

// Maps the stable LOGICAL channel index (ADC_CH_TOTAL/COXA/FEMUR/TIBIA =
// 0/1/2/3, config.h) to the RP2040 ADC input number that must actually be
// selected for it. NOT the identity mapping: hardware/legboard/legboard_sch.py
// wires I_TOTAL_SENSE->GP26(ADC0), I_FEMUR_SENSE->GP27(ADC1),
// I_TIBIA_SENSE->GP28(ADC2), I_COXA_SENSE->GP29(ADC3) -- confirmed on real
// hardware 2026-08-02 (see config.h's current-sense comment). Keeping the
// external channel numbering (CURRAW?/CURCAL, s_channel_name in calib.cpp,
// tools/leg_configurator.py) at coxa=1/femur=2/tibia=3 regardless of pin
// order, and compensating here, avoids that numbering ever needing to
// change again if a future board revision rewires it back to plain order.
static const uint kAdcPhysicalInput[NUM_CURRENT_CHANNELS] = {
    0,  // ADC_CH_TOTAL (0) -> ADC0 / GP26 / I_TOTAL_SENSE
    3,  // ADC_CH_COXA  (1) -> ADC3 / GP29 / I_COXA_SENSE
    1,  // ADC_CH_FEMUR (2) -> ADC1 / GP27 / I_FEMUR_SENSE
    2,  // ADC_CH_TIBIA (3) -> ADC2 / GP28 / I_TIBIA_SENSE
};

static void load_calib(void)
{
    for (int ch = 0; ch < NUM_CURRENT_CHANNELS; ++ch) {
        s_calib[ch] = persist_get_current_calib(ch);
    }
}

void current_init(void)
{
    adc_init();
    adc_gpio_init(PIN_ISENSE_TOTAL);
    adc_gpio_init(PIN_ISENSE_COXA);
    adc_gpio_init(PIN_ISENSE_FEMUR);
    adc_gpio_init(PIN_ISENSE_TIBIA);
    s_latest.total_ma = s_latest.coxa_ma = s_latest.femur_ma = s_latest.tibia_ma = 0;
    load_calib();
}

void current_reload_calib(void)
{
    load_calib();
}

// Read one ADC channel, applying that channel's calibrated scale/offset.
// The RP2040 ADC is 12-bit; scale_ma_per_mv folds the INA4181 gain and shunt
// into a single per-channel constant set via CURCAL (tools/current_calibration).
static uint16_t read_channel_ma(uint ch, float *raw_mv_out, uint16_t *raw_counts_out)
{
    adc_select_input(kAdcPhysicalInput[ch]);
    uint16_t raw = adc_read(); // 0..4095
    float mv = (float)raw * (float)ADC_VREF_MV / (float)ADC_MAX_COUNT;
    if (raw_counts_out) *raw_counts_out = raw;
    if (raw_mv_out) *raw_mv_out = mv;

    float ma = mv * s_calib[ch].scale_ma_per_mv + s_calib[ch].offset_ma;
    if (ma < 0.0f) ma = 0.0f;
    if (ma > 65535.0f) ma = 65535.0f;
    return (uint16_t)ma;
}

void current_sample(void)
{
    s_latest.total_ma = read_channel_ma(ADC_CH_TOTAL, &s_latest_raw.raw_mv[ADC_CH_TOTAL], &s_latest_raw.raw_counts[ADC_CH_TOTAL]);
    s_latest.coxa_ma  = read_channel_ma(ADC_CH_COXA,  &s_latest_raw.raw_mv[ADC_CH_COXA],  &s_latest_raw.raw_counts[ADC_CH_COXA]);
    s_latest.femur_ma = read_channel_ma(ADC_CH_FEMUR, &s_latest_raw.raw_mv[ADC_CH_FEMUR], &s_latest_raw.raw_counts[ADC_CH_FEMUR]);
    s_latest.tibia_ma = read_channel_ma(ADC_CH_TIBIA, &s_latest_raw.raw_mv[ADC_CH_TIBIA], &s_latest_raw.raw_counts[ADC_CH_TIBIA]);
}

void current_get(current_reading_t *out)
{
    if (out) *out = s_latest;
}

void current_get_raw(current_raw_t *out)
{
    if (out) *out = s_latest_raw;
}

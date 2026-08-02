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

// Smoothing state for the *reported* (calibrated) current -- see the
// CURRENT_FILTER_*/DEFAULT_CURRENT_EMA_ALPHA/CURRENT_BOXCAR_MAX_N comment
// in config.h for why two strategies exist. raw_mv/raw_counts
// (current_get_raw()) stay unfiltered regardless of mode. Persisted (see
// persist_get/set_current_filter()) -- loaded in current_init(), so these
// defaults are only what's live in RAM before that first runs.
static int   s_filter_mode = DEFAULT_CURRENT_FILTER_MODE;
static float s_ema_alpha   = DEFAULT_CURRENT_EMA_ALPHA;
static int   s_boxcar_n    = DEFAULT_CURRENT_BOXCAR_N;

static float s_ema_ma[NUM_CURRENT_CHANNELS];
static bool  s_ema_seeded[NUM_CURRENT_CHANNELS];

static float s_boxcar_buf[NUM_CURRENT_CHANNELS][CURRENT_BOXCAR_MAX_N];
static float s_boxcar_sum[NUM_CURRENT_CHANNELS];
static int   s_boxcar_idx[NUM_CURRENT_CHANNELS];
static int   s_boxcar_count[NUM_CURRENT_CHANNELS];

// Switching mode or N resets all filter state -- simplest correct behavior
// (no stale samples from a differently-sized window), at the cost of a
// brief re-fill transient right after the change.
static void reset_filters(void)
{
    for (int ch = 0; ch < NUM_CURRENT_CHANNELS; ++ch) {
        s_ema_seeded[ch]   = false;
        s_boxcar_sum[ch]   = 0.0f;
        s_boxcar_idx[ch]   = 0;
        s_boxcar_count[ch] = 0;
    }
}

void current_set_filter_mode(int mode)
{
    s_filter_mode = (mode == CURRENT_FILTER_BOXCAR) ? CURRENT_FILTER_BOXCAR : CURRENT_FILTER_EMA;
    persist_set_current_filter(s_filter_mode, s_ema_alpha, s_boxcar_n);
    reset_filters();
}

void current_set_ema_alpha(float alpha)
{
    if (alpha <= 0.0f) alpha = 0.0001f; // 0 would freeze the EMA at its seed value forever
    if (alpha > 1.0f) alpha = 1.0f;
    s_ema_alpha = alpha;
    persist_set_current_filter(s_filter_mode, s_ema_alpha, s_boxcar_n);
    reset_filters();
}

void current_set_boxcar_n(int n)
{
    if (n < 1) n = 1;
    if (n > CURRENT_BOXCAR_MAX_N) n = CURRENT_BOXCAR_MAX_N;
    s_boxcar_n = n;
    persist_set_current_filter(s_filter_mode, s_ema_alpha, s_boxcar_n);
    reset_filters();
}

int   current_get_filter_mode(void) { return s_filter_mode; }
float current_get_ema_alpha(void)   { return s_ema_alpha; }
int   current_get_boxcar_n(void)    { return s_boxcar_n; }

static float boxcar_update(int ch, float ma)
{
    int n = s_boxcar_n;
    int idx = s_boxcar_idx[ch];
    if (s_boxcar_count[ch] < n) {
        s_boxcar_buf[ch][idx] = ma;
        s_boxcar_sum[ch] += ma;
        s_boxcar_count[ch]++;
    } else {
        s_boxcar_sum[ch] -= s_boxcar_buf[ch][idx];
        s_boxcar_buf[ch][idx] = ma;
        s_boxcar_sum[ch] += ma;
    }
    s_boxcar_idx[ch] = (idx + 1) % n;
    return s_boxcar_sum[ch] / (float)s_boxcar_count[ch];
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
    s_filter_mode = persist_get_current_filter_mode();
    s_ema_alpha   = persist_get_current_ema_alpha();
    s_boxcar_n    = persist_get_current_boxcar_n();
    reset_filters();
}

void current_reload_calib(void)
{
    load_calib();
}

// Read one ADC channel, applying that channel's calibrated scale/offset,
// then smooth the result per current_set_filter_mode() (see config.h's
// CURRENT_FILTER_* comment for why two strategies exist). RS485 pulls
// arrive roughly every 10 ms (100 Hz, one leg's turn in the 6-leg sweep --
// docs/interfaces/RS485_PROTOCOL.md "Timing Budget"), but current_sample()
// runs at 1 kHz, so without this the reported value would just be whichever
// single 1 kHz sample happened to land at poll time, wasting the other ~9.
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

    float filtered;
    if (s_filter_mode == CURRENT_FILTER_BOXCAR) {
        filtered = boxcar_update(ch, ma);
    } else {
        if (!s_ema_seeded[ch]) {
            s_ema_ma[ch] = ma;      // seed directly instead of ramping up from 0
            s_ema_seeded[ch] = true;
        } else {
            s_ema_ma[ch] = s_ema_alpha * ma + (1.0f - s_ema_alpha) * s_ema_ma[ch];
        }
        filtered = s_ema_ma[ch];
    }
    if (filtered < 0.0f) filtered = 0.0f;
    if (filtered > 65535.0f) filtered = 65535.0f;
    return (uint16_t)filtered;
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

#include "current.h"
#include "config.h"

#include "hardware/adc.h"

static current_reading_t s_latest;

void current_init(void)
{
    adc_init();
    adc_gpio_init(PIN_ISENSE_TOTAL);
    adc_gpio_init(PIN_ISENSE_COXA);
    adc_gpio_init(PIN_ISENSE_FEMUR);
    adc_gpio_init(PIN_ISENSE_TIBIA);
    s_latest.total_ma = s_latest.coxa_ma = s_latest.femur_ma = s_latest.tibia_ma = 0;
}

// Read one ADC channel and convert raw counts to milliamps. The RP2040 ADC is
// 12-bit; ISENSE_MA_PER_MV folds the INA4181 gain and shunt into a single
// placeholder scale to be tuned against a measured load during bring-up.
static uint16_t read_channel_ma(uint ch)
{
    adc_select_input(ch);
    uint16_t raw = adc_read(); // 0..4095
    float mv = (float)raw * (float)ADC_VREF_MV / (float)ADC_MAX_COUNT;
    float ma = mv * ISENSE_MA_PER_MV;
    if (ma < 0.0f) ma = 0.0f;
    if (ma > 65535.0f) ma = 65535.0f;
    return (uint16_t)ma;
}

void current_sample(void)
{
    s_latest.total_ma = read_channel_ma(ADC_CH_TOTAL);
    s_latest.coxa_ma  = read_channel_ma(ADC_CH_COXA);
    s_latest.femur_ma = read_channel_ma(ADC_CH_FEMUR);
    s_latest.tibia_ma = read_channel_ma(ADC_CH_TIBIA);
}

void current_get(current_reading_t *out)
{
    if (out) *out = s_latest;
}

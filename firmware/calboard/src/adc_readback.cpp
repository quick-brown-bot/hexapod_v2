#include "adc_readback.h"

#include "hardware/adc.h"

static const uint s_pin[NUM_CHANNELS] = { PIN_VSENSE_COXA, PIN_VSENSE_FEMUR, PIN_VSENSE_TIBIA };
static const uint s_adc_input[NUM_CHANNELS] = { ADC_CH_COXA, ADC_CH_FEMUR, ADC_CH_TIBIA };

void adc_readback_init(void)
{
    adc_init();
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) adc_gpio_init(s_pin[ch]);
}

float adc_readback_get_volts(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return 0.0f;
    adc_select_input(s_adc_input[channel]);
    uint16_t raw = adc_read(); // 0..4095
    float mv_at_pin = (float)raw * (float)ADC_VREF_MV / (float)ADC_MAX_COUNT;
    return (mv_at_pin / 1000.0f) / VSENSE_DIVIDER_RATIO;
}

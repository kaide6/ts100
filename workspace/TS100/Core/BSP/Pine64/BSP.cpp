// BSP mapping functions

#include "BSP.h"
#include "I2C_Wrapper.hpp"
#include "Pins.h"
#include "Setup.h"
#include "gd32vf103_timer.h"
#include "history.hpp"
#include "main.hpp"
#include "systick.h"
#include <IRQ.h>

//2 second filter (ADC is PID_TIM_HZ Hz)
history<uint16_t, PID_TIM_HZ> rawTempFilter = {{0}, 0, 0};

void resetWatchdog()
{
  //TODO
}

uint16_t getTipInstantTemperature()
{
  uint16_t sum = 0; // 12 bit readings * 8 -> 15 bits

  for (int i = 0; i < 4; i++)
  {
    sum += adc_inserted_data_read(ADC0, i);
    sum += adc_inserted_data_read(ADC1, i);
  }
  return sum; // 8x over sample
}

// Timer callbacks
// TODO
// Handle callback of the PWM modulator to enable / disable the output PWM

uint16_t getTipRawTemp(uint8_t refresh)
{
  if (refresh)
  {
    uint16_t lastSample = getTipInstantTemperature();
    rawTempFilter.update(lastSample);
    return lastSample;
  }
  else
  {
    return rawTempFilter.average();
  }
}
void unstick_I2C()
{
  // TODO
}

uint8_t getButtonA()
{
  return (gpio_input_bit_get(KEY_A_GPIO_Port, KEY_A_Pin) == RESET) ? 1 : 0;
}
uint8_t getButtonB()
{
  return (gpio_input_bit_get(KEY_B_GPIO_Port, KEY_B_Pin) == RESET) ? 1 : 0;
}

void reboot()
{
  // TODO
  for (;;)
  {
  }
}

void delay_ms(uint16_t count) { delay_1ms(count); }

uint16_t getHandleTemperature()
{
#ifdef TEMP_NTC
  //TS80P uses 100k NTC resistors instead
  //NTCG104EF104FT1X from TDK
  //For now not doing interpolation
  int32_t result = getADC(0);
  for (uint32_t i = 0; i < (sizeof(NTCHandleLookup) / (2 * sizeof(uint16_t)));
       i++)
  {
    if (result > NTCHandleLookup[(i * 2) + 0])
    {
      return NTCHandleLookup[(i * 2) + 1] * 10;
    }
  }
  return 0;
#endif
#ifdef TEMP_TMP36
  // We return the current handle temperature in X10 C
  // TMP36 in handle, 0.5V offset and then 10mV per deg C (0.75V @ 25C for
  // example) STM32 = 4096 count @ 3.3V input -> But We oversample by 32/(2^2) =
  // 8 times oversampling Therefore 32768 is the 3.3V input, so 0.1007080078125
  // mV per count So we need to subtract an offset of 0.5V to center on 0C
  // (4964.8 counts)
  //
  int32_t result = getADC(0);
  result -= 4965; // remove 0.5V offset
  // 10mV per C
  // 99.29 counts per Deg C above 0C
  result *= 100;
  result /= 993;
  return result;
#endif
}

uint16_t getInputVoltageX10(uint16_t divisor, uint8_t sample)
{
// ADC maximum is 32767 == 3.3V at input == 28.05V at VIN
// Therefore we can divide down from there
// Multiplying ADC max by 4 for additional calibration options,
// ideal term is 467
#ifdef MODEL_TS100
#define BATTFILTERDEPTH 32
#else
#define BATTFILTERDEPTH 8

#endif
  static uint8_t preFillneeded = 10;
  static uint32_t samples[BATTFILTERDEPTH];
  static uint8_t index = 0;
  if (preFillneeded)
  {
    for (uint8_t i = 0; i < BATTFILTERDEPTH; i++)
      samples[i] = getADC(1);
    preFillneeded--;
  }
  if (sample)
  {
    samples[index] = getADC(1);
    index = (index + 1) % BATTFILTERDEPTH;
  }
  uint32_t sum = 0;

  for (uint8_t i = 0; i < BATTFILTERDEPTH; i++)
    sum += samples[i];

  sum /= BATTFILTERDEPTH;
  if (divisor == 0)
  {
    divisor = 1;
  }
  return sum * 4 / divisor;
}
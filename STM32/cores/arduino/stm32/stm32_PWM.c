/*
  Copyright (c) 2017 Daniel Fekete

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "stm32_gpio.h"

TIM_HandleTypeDef *handle;

static uint32_t counter;
static uint32_t period;

extern void pinMode(uint8_t, uint8_t);

#define min(a,b) ((a)<(b)?(a):(b))

stm32_pwm_disable_callback_func stm32_pwm_disable_callback = NULL;

void (*pwm_callback_func)();

void pwm_callback();

typedef struct {
    GPIO_TypeDef *port;
    uint32_t pin_mask;
    uint16_t frequency;
    uint16_t duty_cycle;
} stm32_pwm_type;

static stm32_pwm_type pwm_config[sizeof(variant_pin_list) / sizeof(variant_pin_list[0])];

void stm32_pwm_disable(GPIO_TypeDef *port, uint32_t pin);

void analogWrite(uint8_t pin, int value) {
    static TIM_HandleTypeDef staticHandle;

    if (handle == NULL) {
        handle = &staticHandle;
        pwm_callback_func = &pwm_callback;

        stm32_pwm_disable_callback = &stm32_pwm_disable;


        #ifdef TIM2 //99% of chips have TIM2
            __HAL_RCC_TIM2_CLK_ENABLE();
            HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(TIM2_IRQn);

            handle->Instance = TIM2;
        #else
            __HAL_RCC_TIM3_CLK_ENABLE();
            HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(TIM3_IRQn);

            handle->Instance = TIM3;
        #endif

        handle->Init.Prescaler = 999;
        handle->Init.CounterMode = TIM_COUNTERMODE_UP;
        period = 256;
        handle->Init.Period = period;
        handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        HAL_TIM_Base_Init(handle);

        HAL_TIM_Base_Start_IT(handle);
    }

    for(size_t i=0; i<sizeof(pwm_config) / sizeof(pwm_config[0]); i++) {
        if (pwm_config[i].port == NULL ||
                (pwm_config[i].port == variant_pin_list[pin].port
                && pwm_config[i].pin_mask == variant_pin_list[pin].pin_mask)) {

            if (pwm_config[i].port == NULL) {
                pinMode(pin, OUTPUT);
            }

            pwm_config[i].port = variant_pin_list[pin].port;
            pwm_config[i].pin_mask = variant_pin_list[pin].pin_mask;
            pwm_config[i].frequency = 255;
            pwm_config[i].duty_cycle = value;
            break;
        }
    }
}

void stm32_pwm_disable(GPIO_TypeDef *port, uint32_t pin_mask) {
    for(size_t i=0; i<sizeof(pwm_config) / sizeof(pwm_config[0]); i++) {
        if (pwm_config[i].port == NULL) {
            return;
        }

        if (pwm_config[i].port == port && pwm_config[i].pin_mask == pin_mask) {

            for(size_t j = i + 1; j < sizeof(pwm_config) / sizeof(pwm_config[0]); j++) {
                if (pwm_config[j].port == NULL) {
                    pwm_config[i].port = pwm_config[j - 1].port;
                    pwm_config[i].pin_mask = pwm_config[j - 1].pin_mask;

                    pwm_config[j - 1].port = NULL;
                    break;
                }
            }

            break;
        }
    }
}

void pwm_callback() {
    if(__HAL_TIM_GET_FLAG(handle, TIM_FLAG_UPDATE) != RESET) {
        if(__HAL_TIM_GET_IT_SOURCE(handle, TIM_IT_UPDATE) !=RESET) {
            __HAL_TIM_CLEAR_IT(handle, TIM_IT_UPDATE);

            counter += period;
            period = 256;

            for(size_t i=0; i<sizeof(pwm_config); i++) {
                if (pwm_config[i].port != NULL) {
                    if (pwm_config[i].duty_cycle > counter % pwm_config[i].frequency) {
                        pwm_config[i].port->BSRR = pwm_config[i].pin_mask;
                        period = min(period, pwm_config[i].duty_cycle - (counter % pwm_config[i].frequency));
                    } else {
                        pwm_config[i].port->BSRR = pwm_config[i].pin_mask << 16;
                        period = min(period, 256 - counter % pwm_config[i].frequency);
                    }
                } else {
                    break;
                }
            }

            if (!period) {
                period = 256;
            }
            __HAL_TIM_SET_AUTORELOAD(handle, period);
        }
    }
}

#ifdef TIM2
  extern void TIM2_IRQHandler(void) {
#else
  extern void TIM3_IRQHandler(void) {
#endif

    if (pwm_callback_func != NULL) {
        (*pwm_callback_func)();
    }
}

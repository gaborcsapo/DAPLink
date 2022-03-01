/**
 * @file    gpio.c
 * @brief
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stm32h7xx.h"
#include "DAP_config.h"
#include "gpio.h"
#include "daplink.h"
#include "util.h"

void gpio_init_buffered_dut_pin(GPIO_TypeDef *dir_port, uint16_t dir_pin, GPIO_TypeDef *input_port, uint16_t input_pin, bool activeHigh)
{
    // Initialize GPIO signals to DUT
    //
    // These are "open drain/open collector" signals, with an external buffer (with a direction pin for each line).
    // The buffer direction is normally an input (to the MCU) so the DUT can pull high/low as needed and a butten can
    // also pull hi/low manually.
    // To "activate" the signal the MCU pin has a weak IO pullup/down so when the buffer becomes an output (from the MCU)
    // the DUT line is driven high/low as needed.

    GPIO_InitTypeDef GPIO_InitStructure;

    HAL_GPIO_WritePin(dir_port, dir_pin, GPIO_PIN_RESET);  //input to MCU

    GPIO_InitStructure.Pin = dir_pin;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(dir_port, &GPIO_InitStructure);

    //MCU pin is initialized as an input with pulldown (active low) or pullup (active high)
    GPIO_InitStructure.Pin = input_pin;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = activeHigh ? GPIO_PULLUP : GPIO_PULLDOWN;
    HAL_GPIO_Init(input_port, &GPIO_InitStructure);
    GPIO_InitStructure.Pull = GPIO_NOPULL;
}

void gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    // enable clock to ports
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    // configure LEDs
    HAL_GPIO_WritePin(RUNNING_LED_PORT, RUNNING_LED_PIN, GPIO_PIN_SET);
    GPIO_InitStructure.Pin = RUNNING_LED_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(RUNNING_LED_PORT, &GPIO_InitStructure);

    HAL_GPIO_WritePin(CONNECTED_LED_PORT, CONNECTED_LED_PIN, GPIO_PIN_SET);
    GPIO_InitStructure.Pin = CONNECTED_LED_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(CONNECTED_LED_PORT, &GPIO_InitStructure);

    HAL_GPIO_WritePin(PIN_CDC_LED_PORT, PIN_CDC_LED, GPIO_PIN_SET);
    GPIO_InitStructure.Pin = PIN_CDC_LED;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(PIN_CDC_LED_PORT, &GPIO_InitStructure);

    HAL_GPIO_WritePin(PIN_MSC_LED_PORT, PIN_MSC_LED, GPIO_PIN_SET);
    GPIO_InitStructure.Pin = PIN_MSC_LED;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(PIN_MSC_LED_PORT, &GPIO_InitStructure);

    // Setup the USB Hub to be "self powered" (very common setting, even if not strictly compliant).
    GPIO_InitStructure.Pin = USBHUB_SELFPWR_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(USBHUB_SELFPWR_PORT, &GPIO_InitStructure);
    HAL_GPIO_WritePin(USBHUB_SELFPWR_PORT, USBHUB_SELFPWR_PIN, GPIO_PIN_RESET);

    //Initialize bidir buffer control signals
    GPIO_InitStructure.Pin = SWD_BUFFER_EN_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_WritePin(SWD_BUFFER_EN_PORT, SWD_BUFFER_EN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(SWD_BUFFER_EN_PORT, &GPIO_InitStructure);

    //These are "open drain/open collector" style, with an external buffer.
    // UDC0_RST_L Reset (to DUT):
    gpio_init_buffered_dut_pin(nRESET_DIR_PIN_PORT, nRESET_DIR_PIN, nRESET_PIN_PORT, nRESET_PIN, false);
    // UDC0_BOOT_L
    gpio_init_buffered_dut_pin(UDC0_BOOT_L_DIR_PORT, UDC0_BOOT_L_DIR_PIN, UDC0_BOOT_L_PORT, UDC0_BOOT_L_PIN, false);
    // UDC0_BUTTON_L
    gpio_init_buffered_dut_pin(UDC0_BUTTON_L_DIR_PORT, UDC0_BUTTON_L_DIR_PIN, UDC0_BUTTON_L_PORT, UDC0_BUTTON_L_PIN, false);
    // UDC1_RST
    gpio_init_buffered_dut_pin(UDC1_RST_DIR_PORT, UDC1_RST_DIR_PIN, UDC1_RST_PORT, UDC1_RST_PIN, true);
    // UDC1_BOOT
    gpio_init_buffered_dut_pin(UDC1_BOOT_DIR_PORT, UDC1_BOOT_DIR_PIN, UDC1_BOOT_PORT, UDC1_BOOT_PIN, true);
    // UDC1_BUTTON
    gpio_init_buffered_dut_pin(UDC1_BUTTON_DIR_PORT, UDC1_BUTTON_DIR_PIN, UDC1_BUTTON_PORT, UDC1_BUTTON_PIN, true);

    // Turn on power to the board. When the target is unpowered
    // it holds the reset line low.
    HAL_GPIO_WritePin(POWER_EN_PIN_PORT, POWER_EN_PIN, GPIO_PIN_RESET);
    GPIO_InitStructure.Pin = POWER_EN_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(POWER_EN_PIN_PORT, &GPIO_InitStructure);

    //Initialize external relay (turned on)
    HAL_GPIO_WritePin(UDC_EXT_RELAY_PORT, UDC_EXT_RELAY_PIN, GPIO_PIN_SET);
    GPIO_InitStructure.Pin = UDC_EXT_RELAY_PIN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(UDC_EXT_RELAY_PORT, &GPIO_InitStructure);

    // Let the voltage rails stabilize.  This is especailly important
    // during software resets, since the target's 3.3v rail can take
    // 20-50ms to drain.  During this time the target could be driving
    // the reset pin low, causing the bootloader to think the reset
    // button is pressed.
    HAL_Delay(100);
}

void gpio_set_hid_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_GPIO_WritePin(PIN_HID_LED_PORT, PIN_HID_LED, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void gpio_set_cdc_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_GPIO_WritePin(PIN_CDC_LED_PORT, PIN_CDC_LED, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void gpio_set_msc_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_GPIO_WritePin(PIN_MSC_LED_PORT, PIN_MSC_LED, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/*
 * The UDC0_RST_L line in hardware build P3 would bring UDB
 * into bootloader mode when the target is connected to DUT0
 * and the target is not powered or powered from the adapter USB.
 *
 * Disable the button and let users use reset_into_swu_mode debug
 * console command to go into bootloader mode.
 */
bool reset_button_pressed(void)
{
    return false;
}

uint8_t gpio_get_reset_btn_no_fwrd(void)
{
    return (nRESET_PIN_PORT->IDR & nRESET_PIN) ? 0 : 1;
}

uint8_t gpio_get_reset_btn_fwrd(void)
{
    return 0;
}


uint8_t GPIOGetButtonState(void)
{
    return 0;
}

void target_forward_reset(bool assert_reset)
{
    // Do nothing - reset is forwarded in gpio_get_sw_reset
}

void gpio_set_board_power(bool powerEnabled)
{
}
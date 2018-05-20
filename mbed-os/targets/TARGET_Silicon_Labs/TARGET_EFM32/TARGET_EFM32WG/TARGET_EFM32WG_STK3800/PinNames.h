/***************************************************************************//**
 * @file PinNames.h
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2015 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
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
 *
 ******************************************************************************/
#ifndef MBED_PINNAMES_H
#define MBED_PINNAMES_H

#include "CommonPinNames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFM32_STANDARD_PIN_DEFINITIONS,

    /* Pursuit Alert Rev C */
		// LEDS 
    LED0 = PC13,
    LED1 = PC14,
    LED2 = PC15,
    LED3 = LED0,
    LED4 = LED1,
		PURST_LED = PA2,
		EMERG_LED = PA4,
		SPARE_LED = PA6,
		DROP_LED  = PA10,
		POWERON_LED = PB11,
		GREEN_LED = PC13,
		RED_LED = PC14,
		BLUE_LED = PC15,

    // Push Buttons 
    SW0 = PA3,
    SW1 = PA4,
    BTN0 = SW0,
    BTN1 = SW1,
		PURST_PB = PA3,
		EMERG_PB = PA5,
		SPARE_PB = PA9,
		DROP_PB = PA15,
		
		
    // Standardized button names
    BUTTON1 = BTN0,
    BUTTON2 = BTN1,

    /* Serial */
    SERIAL_TX   = PD4,
    SERIAL_RX   = PD5,
    USBTX       = PC2,
    USBRX       = PC3,
		CELL_TX 		= PE10,
		CELL_RX			= PE11,
		WIFI_TX 		= PC2,
		WIFI_RX			= PC3,
		SKYGPS_TX 	= PC0,
		SKYGPS_RX		= PC1,
		LEUART_TX		= PD4,
		LEUART_RX		= PD5, 
		
		// EEPROM
		EE_WC_L 		= PE9,
		EE_E2				= PE15,

    /* Board Controller */
    STDIO_UART_TX = USBTX,
    STDIO_UART_RX = USBRX
		
} PinName;

#ifdef __cplusplus
}
#endif

#endif

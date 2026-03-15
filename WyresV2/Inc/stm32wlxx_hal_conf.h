/*
 ******************************************************************************
 * @file    stm32wlxx_hal_conf.h
 * @brief   HAL configuration file.
 ******************************************************************************
 */

#ifndef STM32WLXX_HAL_CONF_H
#define STM32WLXX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
/*#define HAL_ADC_MODULE_ENABLED*/
/*#define HAL_COMP_MODULE_ENABLED*/
/*#define HAL_CRC_MODULE_ENABLED*/
/*#define HAL_CRYP_MODULE_ENABLED*/
/*#define HAL_DAC_MODULE_ENABLED*/
/*#define HAL_GTZC_MODULE_ENABLED*/
/*#define HAL_HSEM_MODULE_ENABLED*/
/*#define HAL_I2C_MODULE_ENABLED*/
/*#define HAL_I2S_MODULE_ENABLED*/
/*#define HAL_IPCC_MODULE_ENABLED*/
/*#define HAL_IRDA_MODULE_ENABLED*/
/*#define HAL_IWDG_MODULE_ENABLED*/
/*#define HAL_LPTIM_MODULE_ENABLED*/
/*#define HAL_PKA_MODULE_ENABLED*/
/*#define HAL_RNG_MODULE_ENABLED*/
/*#define HAL_RTC_MODULE_ENABLED*/
/*#define HAL_SMARTCARD_MODULE_ENABLED*/
/*#define HAL_SMBUS_MODULE_ENABLED*/
/*#define HAL_SPI_MODULE_ENABLED*/
#define HAL_SUBGHZ_MODULE_ENABLED
/*#define HAL_TIM_MODULE_ENABLED*/
#define HAL_UART_MODULE_ENABLED
/*#define HAL_USART_MODULE_ENABLED*/
/*#define HAL_WWDG_MODULE_ENABLED*/
#define HAL_EXTI_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED

/* ########################## Register Callbacks selection ############################## */
#define USE_HAL_ADC_REGISTER_CALLBACKS         0u
#define USE_HAL_COMP_REGISTER_CALLBACKS        0u
#define USE_HAL_CRYP_REGISTER_CALLBACKS        0u
#define USE_HAL_DAC_REGISTER_CALLBACKS         0u
#define USE_HAL_I2C_REGISTER_CALLBACKS         0u
#define USE_HAL_I2S_REGISTER_CALLBACKS         0u
#define USE_HAL_IRDA_REGISTER_CALLBACKS        0u
#define USE_HAL_LPTIM_REGISTER_CALLBACKS       0u
#define USE_HAL_PKA_REGISTER_CALLBACKS         0u
#define USE_HAL_RNG_REGISTER_CALLBACKS         0u
#define USE_HAL_RTC_REGISTER_CALLBACKS         0u
#define USE_HAL_SMARTCARD_REGISTER_CALLBACKS   0u
#define USE_HAL_SMBUS_REGISTER_CALLBACKS       0u
#define USE_HAL_SPI_REGISTER_CALLBACKS         0u
#define USE_HAL_SUBGHZ_REGISTER_CALLBACKS      0u
#define USE_HAL_TIM_REGISTER_CALLBACKS         0u
#define USE_HAL_UART_REGISTER_CALLBACKS        0u
#define USE_HAL_USART_REGISTER_CALLBACKS       0u
#define USE_HAL_WWDG_REGISTER_CALLBACKS        0u

/* ########################## Oscillator Values adaptation ####################*/
#if !defined(HSE_VALUE)
#define HSE_VALUE                           32000000UL
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT                 100UL
#endif

#if !defined(MSI_VALUE)
#define MSI_VALUE                           4000000UL
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE                           16000000UL
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE                           32000UL
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE                           32768UL
#endif

#if !defined(HSI48_VALUE)
#define HSI48_VALUE                         48000000UL
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT                 5000UL
#endif

/* ########################### System Configuration ######################### */
#define VDD_VALUE                           3300U
#define TICK_INT_PRIORITY                   ((1uL << __NVIC_PRIO_BITS) - 1uL)
#define USE_RTOS                            0U
#define PREFETCH_ENABLE                     0U
#define INSTRUCTION_CACHE_ENABLE            1U
#define DATA_CACHE_ENABLE                   1U

/* ########################## Assert Selection ############################## */
/* #define USE_FULL_ASSERT 1 */

/* ################## SPI peripheral configuration ########################## */
#define USE_SPI_CRC                         1U

/* ################## CRYP peripheral configuration ########################## */
#define USE_HAL_CRYP_SUSPEND_RESUME         1U

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "stm32wlxx.h"

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32wlxx_hal_dma.h"
#endif

#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32wlxx_hal_adc.h"
#endif

#ifdef HAL_COMP_MODULE_ENABLED
#include "stm32wlxx_hal_comp.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32wlxx_hal_cortex.h"
#endif

#ifdef HAL_CRC_MODULE_ENABLED
#include "stm32wlxx_hal_crc.h"
#endif

#ifdef HAL_CRYP_MODULE_ENABLED
#include "stm32wlxx_hal_cryp.h"
#endif

#ifdef HAL_DAC_MODULE_ENABLED
#include "stm32wlxx_hal_dac.h"
#endif

#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32wlxx_hal_exti.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32wlxx_hal_flash.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32wlxx_hal_gpio.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32wlxx_hal_pwr.h"
#endif

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32wlxx_hal_rcc.h"
#include "stm32wlxx_hal_rcc_ex.h"
#endif

#ifdef HAL_GTZC_MODULE_ENABLED
#include "stm32wlxx_hal_gtzc.h"
#endif

#ifdef HAL_HSEM_MODULE_ENABLED
#include "stm32wlxx_hal_hsem.h"
#endif

#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32wlxx_hal_i2c.h"
#endif

#ifdef HAL_I2S_MODULE_ENABLED
#include "stm32wlxx_hal_i2s.h"
#endif

#ifdef HAL_IPCC_MODULE_ENABLED
#include "stm32wlxx_hal_ipcc.h"
#endif

#ifdef HAL_IRDA_MODULE_ENABLED
#include "stm32wlxx_hal_irda.h"
#endif

#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32wlxx_hal_iwdg.h"
#endif

#ifdef HAL_LPTIM_MODULE_ENABLED
#include "stm32wlxx_hal_lptim.h"
#endif

#ifdef HAL_PKA_MODULE_ENABLED
#include "stm32wlxx_hal_pka.h"
#endif

#ifdef HAL_RNG_MODULE_ENABLED
#include "stm32wlxx_hal_rng.h"
#endif

#ifdef HAL_RTC_MODULE_ENABLED
#include "stm32wlxx_hal_rtc.h"
#endif

#ifdef HAL_SMARTCARD_MODULE_ENABLED
#include "stm32wlxx_hal_smartcard.h"
#endif

#ifdef HAL_SMBUS_MODULE_ENABLED
#include "stm32wlxx_hal_smbus.h"
#endif

#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32wlxx_hal_spi.h"
#endif

#ifdef HAL_SUBGHZ_MODULE_ENABLED
#include "stm32wlxx_hal_subghz.h"
#endif

#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32wlxx_hal_tim.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32wlxx_hal_uart.h"
#endif

#ifdef HAL_USART_MODULE_ENABLED
#include "stm32wlxx_hal_usart.h"
#endif

#ifdef HAL_WWDG_MODULE_ENABLED
#include "stm32wlxx_hal_wwdg.h"
#endif

/* Exported macro ------------------------------------------------------------*/
#ifdef USE_FULL_ASSERT
#include <assert.h>
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32WLXX_HAL_CONF_H */

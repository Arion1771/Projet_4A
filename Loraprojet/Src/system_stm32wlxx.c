/*
 ******************************************************************************
 * @file    system_stm32wlxx.c
 * @brief   CMSIS Cortex Device Peripheral Access Layer System Source File
 ******************************************************************************
 * Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 ******************************************************************************
 */

#include <stdint.h>
#include "stm32wlxx.h"

/* CMSIS headers provide *_Msk/_Pos; define legacy values if missing. */
#ifndef RCC_CFGR_SWS_MSI
#define RCC_CFGR_SWS_MSI   (0x0UL << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_HSI   (0x1UL << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_HSE   (0x2UL << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_PLL   (0x3UL << RCC_CFGR_SWS_Pos)
#endif

#ifndef RCC_PLLCFGR_PLLSRC_HSI
#define RCC_PLLCFGR_PLLSRC_MSI  (0x0UL << RCC_PLLCFGR_PLLSRC_Pos)
#define RCC_PLLCFGR_PLLSRC_HSI  (0x1UL << RCC_PLLCFGR_PLLSRC_Pos)
#define RCC_PLLCFGR_PLLSRC_HSE  (0x2UL << RCC_PLLCFGR_PLLSRC_Pos)
#endif

#if !defined (HSE_VALUE)
#define HSE_VALUE    (32000000UL)
#endif

#if !defined (MSI_VALUE)
#define MSI_VALUE    (4000000UL)
#endif

#if !defined (HSI_VALUE)
#define HSI_VALUE    (16000000UL)
#endif

#if !defined (LSI_VALUE)
#define LSI_VALUE    (32000UL)
#endif

#if !defined (LSE_VALUE)
#define LSE_VALUE    (32768UL)
#endif

/* Vector table relocation is disabled by default */
/* #define USER_VECT_TAB_ADDRESS */
#if defined(USER_VECT_TAB_ADDRESS)
#ifdef CORE_CM0PLUS
/* #define VECT_TAB_SRAM */
#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS   SRAM2_BASE
#define VECT_TAB_OFFSET         0x00008000U
#else
#define VECT_TAB_BASE_ADDRESS   FLASH_BASE
#define VECT_TAB_OFFSET         0x00020000U
#endif
#else /* CORE_CM4 */
/* #define VECT_TAB_SRAM */
#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS   SRAM1_BASE
#define VECT_TAB_OFFSET         0x00000000U
#else
#define VECT_TAB_BASE_ADDRESS   FLASH_BASE
#define VECT_TAB_OFFSET         0x00000000U
#endif
#endif
#endif

uint32_t SystemCoreClock  = 4000000UL;

const uint32_t AHBPrescTable[16UL] = {
  1UL, 3UL, 5UL, 1UL, 1UL, 6UL, 10UL, 32UL, 2UL, 4UL, 8UL, 16UL, 64UL, 128UL, 256UL, 512UL
};

const uint32_t APBPrescTable[8UL]  = {0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL};

const uint32_t MSIRangeTable[16UL] = {
  100000UL, 200000UL, 400000UL, 800000UL, 1000000UL, 2000000UL,
  4000000UL, 8000000UL, 16000000UL, 24000000UL, 32000000UL, 48000000UL, 0UL, 0UL, 0UL, 0UL
};

void SystemInit(void)
{
#if defined(USER_VECT_TAB_ADDRESS)
  SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET;
#endif

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << (10UL * 2UL)) | (3UL << (11UL * 2UL)));
#endif
}

void SystemCoreClockUpdate(void)
{
  uint32_t msirange;
  uint32_t tmp;
  uint32_t pllvco;
  uint32_t pllr;
  uint32_t pllsource;
  uint32_t pllm;

  tmp = RCC->CFGR;
  switch (tmp & RCC_CFGR_SWS_Msk)
  {
    case RCC_CFGR_SWS_MSI:
      msirange = (RCC->CR & RCC_CR_MSIRANGE) >> RCC_CR_MSIRANGE_Pos;
      SystemCoreClock = MSIRangeTable[msirange];
      break;

    case RCC_CFGR_SWS_HSI:
      SystemCoreClock = HSI_VALUE;
      break;

    case RCC_CFGR_SWS_HSE:
      SystemCoreClock = HSE_VALUE;
      break;

    case RCC_CFGR_SWS_PLL:
      pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC_Msk);
      pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1U;

      if (pllsource == RCC_PLLCFGR_PLLSRC_HSI)
      {
        pllvco = (HSI_VALUE / pllm);
      }
      else if (pllsource == RCC_PLLCFGR_PLLSRC_HSE)
      {
        pllvco = (HSE_VALUE / pllm);
      }
      else
      {
        msirange = (RCC->CR & RCC_CR_MSIRANGE) >> RCC_CR_MSIRANGE_Pos;
        pllvco = (MSIRangeTable[msirange] / pllm);
      }

      pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
      pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1U) * 2U;
      SystemCoreClock = pllvco / pllr;
      break;

    default:
      SystemCoreClock = MSI_VALUE;
      break;
  }

  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)];
  SystemCoreClock >>= tmp;
}

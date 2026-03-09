/**
 ******************************************************************************
 * @file    stm32wle5xx.h
 * @brief   CMSIS STM32WLE5xx Device Peripheral Access Layer Header File
 ******************************************************************************
 */

#ifndef __STM32WLE5XX_H
#define __STM32WLE5XX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================ Peripheral memory map ================ */
#define PERIPH_BASE           0x40000000UL
#define AHB2PERIPH_BASE       (PERIPH_BASE + 0x08000000UL)

#define RCC_BASE              (AHB2PERIPH_BASE + 0x00021000UL)
#define GPIOA_BASE            (AHB2PERIPH_BASE + 0x00020000UL)
#define GPIOB_BASE            (AHB2PERIPH_BASE + 0x00020400UL)
#define GPIOC_BASE            (AHB2PERIPH_BASE + 0x00020800UL)

#define USART1_BASE           (PERIPH_BASE + 0x00013800UL)
#define USART2_BASE           (PERIPH_BASE + 0x00004400UL)

/* ================ GPIO Structure ================ */
typedef struct
{
    volatile uint32_t MODER;       /* GPIO port mode register */
    volatile uint32_t OTYPER;      /* GPIO port output type register */
    volatile uint32_t OSPEEDR;     /* GPIO port output speed register */
    volatile uint32_t PUPDR;       /* GPIO port pull-up/pull-down register */
    volatile uint32_t IDR;         /* GPIO port input data register */
    volatile uint32_t ODR;         /* GPIO port output data register */
    volatile uint32_t BSRR;        /* GPIO port bit set/reset register */
    volatile uint32_t LCKR;        /* GPIO port configuration lock register */
    volatile uint32_t AFR[2];      /* GPIO alternate function registers */
    volatile uint32_t BRR;         /* GPIO port bit reset register */
} GPIO_TypeDef;

/* ================ RCC Structure ================ */
typedef struct
{
    volatile uint32_t CR;          /* RCC clock control register */
    volatile uint32_t ICSCR;       /* RCC internal clock sources calibration register */
    volatile uint32_t CFGR;        /* RCC clock configuration register */
    volatile uint32_t PLLCFGR;     /* RCC PLL configuration register */
    volatile uint32_t RESERVED0[2];
    volatile uint32_t CIER;        /* RCC clock interrupt enable register */
    volatile uint32_t CIFR;        /* RCC clock interrupt flag register */
    volatile uint32_t CICR;        /* RCC clock interrupt clear register */
    volatile uint32_t RESERVED1;
    volatile uint32_t AHB1RSTR;    /* RCC AHB1 peripheral reset register */
    volatile uint32_t AHB2RSTR;    /* RCC AHB2 peripheral reset register */
    volatile uint32_t AHB3RSTR;    /* RCC AHB3 peripheral reset register */
    volatile uint32_t RESERVED2;
    volatile uint32_t APB1RSTR1;   /* RCC APB1 peripheral reset register 1 */
    volatile uint32_t APB1RSTR2;   /* RCC APB1 peripheral reset register 2 */
    volatile uint32_t APB2RSTR;    /* RCC APB2 peripheral reset register */
    volatile uint32_t APB3RSTR;    /* RCC APB3 peripheral reset register */
    volatile uint32_t AHB1ENR;     /* RCC AHB1 peripheral clock enable register */
    volatile uint32_t AHB2ENR;     /* RCC AHB2 peripheral clock enable register */
    volatile uint32_t AHB3ENR;     /* RCC AHB3 peripheral clock enable register */
    volatile uint32_t RESERVED3;
    volatile uint32_t APB1ENR1;    /* RCC APB1 peripheral clock enable register 1 */
    volatile uint32_t APB1ENR2;    /* RCC APB1 peripheral clock enable register 2 */
    volatile uint32_t APB2ENR;     /* RCC APB2 peripheral clock enable register */
    volatile uint32_t APB3ENR;     /* RCC APB3 peripheral clock enable register */
} RCC_TypeDef;

/* ================ USART Structure ================ */
typedef struct
{
    volatile uint32_t CR1;         /* USART control register 1 */
    volatile uint32_t CR2;         /* USART control register 2 */
    volatile uint32_t CR3;         /* USART control register 3 */
    volatile uint32_t BRR;         /* USART baud rate register */
    volatile uint32_t GTPR;        /* USART guard time and prescaler register */
    volatile uint32_t RTOR;        /* USART receiver timeout register */
    volatile uint32_t RQR;         /* USART request register */
    volatile uint32_t ISR;         /* USART interrupt and status register */
    volatile uint32_t ICR;         /* USART interrupt flag clear register */
    volatile uint32_t RDR;         /* USART receive data register */
    volatile uint32_t TDR;         /* USART transmit data register */
    volatile uint32_t PRESC;       /* USART prescaler register */
} USART_TypeDef;

/* ================ Peripheral declarations ================ */
#define GPIOA                 ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB                 ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOC                 ((GPIO_TypeDef *) GPIOC_BASE)
#define RCC                   ((RCC_TypeDef *) RCC_BASE)
#define USART1                ((USART_TypeDef *) USART1_BASE)
#define USART2                ((USART_TypeDef *) USART2_BASE)

/* ================ RCC Bit Definitions ================ */
#define RCC_AHB2ENR_GPIOAEN   (1UL << 0)
#define RCC_AHB2ENR_GPIOBEN   (1UL << 1)
#define RCC_AHB2ENR_GPIOCEN   (1UL << 2)
#define RCC_APB2ENR_USART1EN  (1UL << 14)

/* ================ USART Bit Definitions ================ */
#define USART_CR1_UE          (1UL << 0)
#define USART_CR1_TE          (1UL << 3)
#define USART_CR1_RE          (1UL << 2)
#define USART_ISR_TXE         (1UL << 7)
#define USART_ISR_TC          (1UL << 6)

#ifdef __cplusplus
}
#endif

#endif /* __STM32WLE5XX_H */

#include <stdint.h>

uint32_t SystemCoreClock = 16000000UL;

void SystemInit(void)
{
    /* Keep reset clock configuration: HSI as SYSCLK on STM32L151. */
}

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 16000000UL;
}

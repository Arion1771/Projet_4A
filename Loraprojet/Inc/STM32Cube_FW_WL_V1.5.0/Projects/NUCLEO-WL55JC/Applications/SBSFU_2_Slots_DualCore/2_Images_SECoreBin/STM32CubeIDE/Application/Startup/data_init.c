
#include "stdint-gcc.h"

#ifndef vu32
#	define vu32 volatile uint32_t
#endif

void LoopCopyDataInit(void) {
	extern uint8_t* _sidata asm("_sidata");
	extern uint8_t* _sdata asm("_sdata");
	extern uint8_t* _edata asm("_edata");

	vu32* src = (vu32*) &_sidata;
	vu32* dst = (vu32*) &_sdata;

	 vu32 len = ((vu32)(&_edata) - (vu32)(&_sdata)) / 4;

	for(vu32 i=0; i < len; i++)
		dst[i] = src[i];
}

void LoopFillZerobss(void) {
	extern uint8_t* _sbss asm("_sbss");
	extern uint8_t* _ebss asm("_ebss");

	vu32* dst = (vu32*) &_sbss;
	vu32 len = ((vu32)(&_ebss) - (vu32)(&_sbss)) / 4;

	for(vu32 i=0; i < len; i++)
		dst[i] = 0;
}

void __gcc_data_init(void) {
	LoopFillZerobss();
	LoopCopyDataInit();
}

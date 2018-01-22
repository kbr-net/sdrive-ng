/*
 A stripped down version of gcrt1.S for use with bootloader code.
 CFLAGS needs to contain "-nostartfiles -nodefaultlibs"

 WARNING: data will not be initialized !!!

 Version 0.01  - 18 August 2005  Werner B.
 Version 0.02  - 16 Mai 2010 Werner B.  Added stack for AVR-GCC 4.x
*/

#if (__GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
#error "GCC version >= 3.3 required"
#endif

#include "macros.inc"
#include <avr/io.h>

	.section .vectors,"ax",@progbits
	.global	__vectors
	.func	__vectors
__vectors:
	clr	__zero_reg__
	out	_SFR_IO_ADDR(SREG), __zero_reg__
	ldi	r28,lo8(__stack)
#ifdef _HAVE_AVR_STACK_POINTER_HI
	ldi	r29,hi8(__stack)
	out	AVR_STACK_POINTER_HI_ADDR, r29
#endif	/* _HAVE_AVR_STACK_POINTER_HI */
	out	AVR_STACK_POINTER_LO_ADDR, r28

#ifdef __AVR_3_BYTE_PC__
//	ldi	r16, hh8(pm(__vectors))
//	out	_SFR_IO_ADDR(EIND), r16
	out	_SFR_IO_ADDR(EIND), __zero_reg__
#endif	/* __AVR_3_BYTE_PC__ */

#ifdef __AVR_HAVE_RAMPD__
	out	AVR_RAMPD_ADDR, __zero_reg__
	out	AVR_RAMPX_ADDR, __zero_reg__
	out	AVR_RAMPY_ADDR, __zero_reg__
	out	AVR_RAMPZ_ADDR, __zero_reg__
#endif
//	XJMP   main
	.endfunc

    /* The stack is initialized at the beginning of main() */
   	.weak	__stack
	.set	__stack, RAMEND

	.section .init9,"ax",@progbits
	rjmp	main


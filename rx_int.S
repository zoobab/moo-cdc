/*
 * rx_int.S - stackable interrupt serial RX routine
 * Copyright (C) 2008-2010 Stefan Szczygielski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: rx_int.S 371 2010-01-30 13:12:03Z moomean $
 */

#include <avr/interrupt.h>
#include "moo-cdc.h"

#if defined (__AVR_ATmega8__)
	.global USART_RXC_vect
	USART_RXC_vect:
#elif defined (__AVR_ATtiny2313__) || defined (__AVR_ATmega88__)
	.global USART_RX_vect
	USART_RX_vect:
#else
#error "Do I know that MCU?"
#endif

	push r1 ; (2)
#if UDR < 0x60
	in r1, _SFR_IO_ADDR(UDR) ; (1) this doesn't modify SREG, UDR int flag is now cleared
#else
	lds r1, UDR ; (2) this costs one extra cycle on ATmega88...
#endif

#	cbi _SFR_IO_ADDR(UCSRB), RXCIE	; (1) disable rx int, at 115200 bps they can stack too!
					; This was nice with Atmega8, but with Atmega88 UCSRB is
					; outside of cbi instruction range; doing that costs
					; 5 cycles (lds, ori, sts) and a register which we have to
					; push first; make that two times to restore and we need
					; 10 cycles instead of 2 :/ Not worth it, I guess... :/

#	sei ; (1) we want to re-enable interrupts as fast as we can
	
	push r0 ; (2)
	in r0, _SFR_IO_ADDR(SREG) ; (1)

	push r30 ; (2)

#	ldi r30, 0x00 ; (1)
#	add r30, r2 ; (1) r2 is a reserved register for put index in buffer
	mov r30, r2 ; (1) r2 is a reserved register for put index in buffer
	inc r2 ; (1)

	sei ; (1) interrupt latency increased by some few cycles because of rx int stack issue :/

#	sbi _SFR_IO_ADDR(UCSRB), RXCIE	; (1) re-enable rx int: we have our space in buffer reserved, pointer is ok;

					; new buffer reads are only made from non-interrupt so it's safe to fill-in
					; the value itself later
	push r31 ; (2)
	ldi r31, 0x02 ; (1)

	st Z, r1 ; (2)

	pop r31 ; (2)
	pop r30 ; (2)

	out _SFR_IO_ADDR(SREG), r0 ; (1)
	pop r0 ; (2)
	pop r1 ; (2)
	reti ; (4)

; total: 29(30) cycles, interrupts re-enabled in cycle 11(12)

# 748:	1f 92       	push	r1
# 74a:	0f 92       	push	r0
# 74c:	0f b6       	in	r0, 0x3f	; 63
# 74e:	0f 92       	push	r0
# 750:	11 24       	eor	r1, r1
# 752:	8f 93       	push	r24
# 754:	9f 93       	push	r25
# 756:	ef 93       	push	r30
# 758:	ff 93       	push	r31
#		rx_buf[rx_buf_put_ptr++] = UDR;
# 75a:	e0 e0       	ldi	r30, 0x00	; 0
# 75c:	f1 e0       	ldi	r31, 0x01	; 1
# 75e:	e2 0d       	add	r30, r2
# 760:	f1 1d       	adc	r31, r1
# 762:	9c b1       	in	r25, 0x0c	; 12
# 764:	90 83       	st	Z, r25
# 766:	23 94       	inc	r2
#		sei(); // reenable interrupts fast or die (before pops)
# 768:	78 94       	sei
# 76a:	ff 91       	pop	r31
# 76c:	ef 91       	pop	r30
# 76e:	9f 91       	pop	r25
# 770:	8f 91       	pop	r24
# 772:	0f 90       	pop	r0
# 774:	0f be       	out	0x3f, r0	; 63
# 776:	0f 90       	pop	r0
# 778:	1f 90       	pop	r1
# 77a:	18 95       	reti

; compiler: MUCH worse!


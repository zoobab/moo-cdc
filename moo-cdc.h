#define HAVE_LED 1
#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_P PC4

#ifndef UBRRL
#define UBRRL UBRR0L
#endif
#ifndef UCSRC
#define UCSRC UCSR0C	
#endif
#ifndef UPM0
#define UPM0 UPM00
#endif
#ifndef USBS
#define USBS USBS0
#endif
#ifndef UCSZ0
#define UCSZ0 UCSZ00
#endif
#ifndef UDR
#define UDR UDR0
#endif
#ifndef UCSRB
#define UCSRB UCSR0B
#endif
#ifndef UDRIE
#define UDRIE UDRIE0
#endif
#ifndef UCSRA
#define UCSRA UCSR0A
#endif
#ifndef U2X
#define U2X U2X0
#endif
#ifndef RXEN
#define RXEN RXEN0
#endif
#ifndef TXEN
#define TXEN TXEN0
#endif
#ifndef RXCIE
#define RXCIE RXCIE0
#endif

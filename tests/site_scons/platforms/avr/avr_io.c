/* This wrapper file initializes stdio to UART connection and
 * receives the list of command line arguments.
 */
#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#undef main
extern int app_main(int argc, const char **argv);

struct {
    uint8_t argc;
    char args[3][16];
} g_args;

int uart_putchar(char c, FILE *stream)
{
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
    return 0;
}

int uart_getchar(FILE *stream)
{
	loop_until_bit_is_set(UCSR0A, RXC0);
	if (UCSR0A & _BV(FE0)) return _FDEV_EOF; /* Break = EOF */
	return UDR0;
}

FILE uart_str = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);

int main(void)
{
    const char *argv[4] = {"main", g_args.args[0], g_args.args[1], g_args.args[2]};
    int status;
    
    UBRR0 = (8000000 / (16UL * 9600)) - 1; /* 9600 bps with default 8 MHz clock */
    UCSR0B = _BV(TXEN0) | _BV(RXEN0);
    
    stdout = stdin = stderr = &uart_str;
    
    fread((char*)&g_args, 1, sizeof(g_args), stdin);
    
    status = app_main(g_args.argc + 1, argv);

    DDRB = 3;
    if (status)
        PORTB = 1;
    else
        PORTB = 2;
  
    cli();
    sleep_mode();
    return status;
}


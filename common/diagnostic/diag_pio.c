

#include "stdio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/uart.h"

void print_uart_gpio_config(uart_inst_t *uart, uint tx_pin, uint rx_pin)
{
    printf("\n===== UART / GPIO CONFIG =====\n");

    // UART instance info
    printf("UART instance pointer: %p\n", uart);

    if (uart == uart0)
        printf("UART instance: uart0\n");
    else if (uart == uart1)
        printf("UART instance: uart1\n");
    else
        printf("UART instance: UNKNOWN\n");

    // Check UART enable state
    uart_hw_t *hw = uart_get_hw(uart);
    printf("UART CR register: 0x%08X\n", hw->cr);
    printf("UART enabled (UARTEN bit): %d\n", (hw->cr & UART_UARTCR_UARTEN_BITS) != 0);

    printf("\n--- GPIO %d ---\n", tx_pin);
    printf("Function select: %u\n", gpio_get_function(tx_pin));
    printf("Output enabled: %d\n", gpio_is_dir_out(tx_pin));

    printf("\n--- GPIO %d ---\n", rx_pin);
    printf("Function select: %u\n", gpio_get_function(rx_pin));
    printf("Output enabled: %d\n", gpio_is_dir_out(rx_pin));

    printf("\nGPIO FUNC_UART value: %d\n", GPIO_FUNC_UART);

    printf("================================\n\n");
}

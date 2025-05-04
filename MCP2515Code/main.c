#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "waveshareLib/DEV_Config.h" 
#include "waveshareLib/mcp2515.h"

// stdout is over usb so uart0 can be used
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define SYNC_BYTE 0xAA

uint8_t current_can = 0x00;

void send_uint16_with_header(uint16_t value) {
    uint8_t high = (value >> 8) & 0xFF;
    uint8_t low = value & 0xFF;

    uart_putc_raw(UART_ID, SYNC_BYTE); // send 0xAA as header
    uart_putc_raw(UART_ID, high); 
    uart_putc_raw(UART_ID, low);
}

// commands are send as MODE followed bt the CAN ID in hex
// e.g. MODE0C is RPM
void listen_for_commands() {
    static char buffer[16];
    static int i = 0;

    while (uart_is_readable(UART_ID)) { // check if there is data to read
        char c = uart_getc(UART_ID);
        if (c == '\n' || c == '\r') { // end of command
            buffer[i] = '\0';
            if (strncmp(buffer, "MODE", 4) == 0) { // check if its a mode command
                uint8_t id = (uint8_t)strtol(&buffer[5], NULL, 16); // get the CAN ID from the command
                current_can = id; // write it to the global variable
                printf("Received MODE command, CAN ID set to: 0x%X\n", current_can);
            }
            i = 0; // reset the buffer index after end of command
        } else if (i < sizeof(buffer) - 1) {
            buffer[i++] = c;
        }
    }
}

bool fetch_PID_response(uint8_t pid, uint16_t *out) {
    if (pid == 0x00) return false; // No query for PID 0x00
    uint8_t can_frame[8] = {0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    // print the whole frame for debugging
    printf("Sending CAN frame: ");
    for (int j = 0; j < 8; j++) {
        printf("0x%02X ", can_frame[j]);
    }
    printf("\n");
    uint8_t reply[8] = {0};
    //uint8_t reply[8] = {0x02, 0x41, pid, 0x55, 0x55, 0x00, 0x00, 0x00}; // Dummy reply for testing
    MCP2515_Send(0x7DF, can_frame, 8);
    MCP2515_Receive(0x7E8, reply);

    if (reply[1] == 0x41 && reply[2] == pid) {
        switch (pid) {
            case 0x0C: *out = ((reply[3] << 8) + reply[4]) / 4; return true; // RPM
            case 0x0D: *out = reply[3]; return true; // Vehicle Speed
            case 0x11: *out = reply[3] * 100 / 255; return true; // Throttle Position
            case 0x04: *out = reply[3] * 100 / 255; return true; // Engine load
            case 0x05: *out = reply[3] - 40; return true; // Coolant Temp
            case 0x0F: *out = reply[3] - 40; return true; // Intake Temp
            default: return false; // Unsupported PID
        }
    }
    return false; // No valid response
}


int main()
{
    stdio_init_all();
    DEV_Module_Init();
    MCP2515_Init();
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    srand(time_us_32());

    absolute_time_t last_send = get_absolute_time();

    while (true) {
        listen_for_commands();

        // query the CAN bus every 100ms
        if (absolute_time_diff_us(last_send, get_absolute_time()) > 100000) {
            uint16_t value = 0;
            // check if current can is not null and fetch the response
            if(current_can != 0x00 && fetch_PID_response((uint8_t) current_can, &value)) {
                // write into val, then send it over UART
                send_uint16_with_header(value);
                printf("Sent value: %d (mode %d)\n", value, current_can);
            } else {
                if (current_can == 0x00) {
                    printf("No valid response for CAN ID: 0x00\n");
                } else {
                    printf("No valid response for CAN ID: 0x%X\n", current_can);
                }
            }
            last_send = get_absolute_time();
        }
        
        tight_loop_contents();
    }
}
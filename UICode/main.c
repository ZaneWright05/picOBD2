#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

// UART config to communicate with the second pico
#define UART_ID uart0
#define UART_BAUD_RATE 115200
#define UART_TX_PIN 1
#define UART_RX_PIN 2

#define SYNC_BYTE 0xAA

// can ID to mean no request
uint8_t canID = 0x00;
uint16_t store = 0;

UWORD imageBuffer[OLED_1in3_C_WIDTH * OLED_1in3_C_HEIGHT / 8];

struct Mode{
    char* name; // name to display
    uint8_t id; // id to send
    char* unit; // unit to display
};

void setupUART(){
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

int receive_uint16_with_header(uint16_t* out_value) {
    *out_value = 0;
    int timeout = 50000; // 50ms
    absolute_time_t start = get_absolute_time();

    // dont wait forever to avoid blocking the UI
    while (absolute_time_diff_us(start, get_absolute_time()) < timeout) {
        if(uart_is_readable(UART_ID)){
            uint8_t byte = uart_getc(UART_ID);
            printf("Byte: %02X\n", byte);
            if (byte == SYNC_BYTE) { // msgs sent by the second pico start with 0xAA
                // read the next two bytes
                while (!uart_is_readable(UART_ID)){
                    if(absolute_time_diff_us(start, get_absolute_time()) >= timeout) {
                        return 0;
                    }
                }
                uint8_t high = uart_getc(UART_ID);

                while (!uart_is_readable(UART_ID)){
                    if(absolute_time_diff_us(start, get_absolute_time()) >= timeout) {
                        return 0;
                    }
                }
                uint8_t low = uart_getc(UART_ID);

                *out_value = ((uint16_t)high << 8) | low; // combine high and low bytes
                return 1;
        }
    }
}
    return 0;
}

// function to send the CAN ID to the second pico
void send_mode_command(uint8_t canID) {
    char msg[20];
    snprintf(msg, sizeof(msg), "MODE:%02X\n", canID);  
    uart_puts(UART_ID, msg);
    printf("Sent mode command: %s", msg);
}

int main(void)
{   
    // mode array to hold the different modes
    struct Mode mode[6] = {
        {"Eng Speed", 0x0C, "rpm"},
        {"Veh Speed", 0x0D, "km/h"},
        {"Intake temp", 0x0F, "C"},
        {"Clnt temp", 0x05, "C"},
        {"Thrttl Pos", 0x11, "%"},
        {"Engine load", 0x04, "%"}
    };
    
    DEV_Delay_ms(100);
    
    if(DEV_Module_Init()!=0){
        while(1){
            printf("END\r\n");
        }
    }
    
    stdio_init_all();
    setupUART();
    DEV_Delay_ms(100);
    OLED_1in3_C_Init();
    OLED_1in3_C_Clear();
    
    // UI setup from waveshare: https://www.waveshare.com/wiki/Pico-OLED-1.3#Overview
    UBYTE *BlackImage;
    UWORD Imagesize = ((OLED_1in3_C_WIDTH%8==0)? (OLED_1in3_C_WIDTH/8): (OLED_1in3_C_WIDTH/8+1)) * OLED_1in3_C_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        while(1){
            printf("Failed to apply for black memory...\r\n");
        }
    }
    Paint_NewImage(BlackImage, OLED_1in3_C_WIDTH, OLED_1in3_C_HEIGHT, 0, WHITE);	
    
    Paint_SelectImage(BlackImage);
    DEV_Delay_ms(500);
    Paint_Clear(BLACK);

    OLED_1in3_C_Display(BlackImage);

    // varaible to check if keys are pressed
    int key1act = 0; 
    int key0act = 0;
    
    DEV_GPIO_Mode(KEY0, 0);
    DEV_GPIO_Mode(KEY1, 0);

    // index in mode array
    int current = 0;

    // varaible to check if the screen is toggled on/off
    int visible = true;
    // varaible to check if the a mode is selected
    bool selected = false;

    // varibale to hold the response
    char valStr[10];
    absolute_time_t start = get_absolute_time();
    while(1){
        Paint_SelectImage(BlackImage);
        Paint_Clear(BLACK);

        if (visible && selected == false) { // show name and unit and instructions
            Paint_DrawString_EN(0, 0, mode[current].name, &Font16, WHITE, BLACK);
            Paint_DrawString_EN(OLED_1in3_C_WIDTH - (strlen(mode[current].unit) * Font16.Width), 17, mode[current].unit, &Font16, WHITE, BLACK);
            Paint_DrawLine(0, 35, OLED_1in3_C_WIDTH, 35, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
            Paint_DrawString_EN(0, 37, "0 for mode" , &Font12, WHITE, BLACK);
            Paint_DrawString_EN(0, 49, "1 to select", &Font12, WHITE, BLACK); 
        } else{
            if(selected){ // toggle disabled
                Paint_DrawString_EN(0, 0, mode[current].name, &Font16, WHITE, BLACK); // show name
                if(receive_uint16_with_header(&store)){ //  get new data, if true its updated
                    printf("Received: %x\r\n", store);
                    snprintf(valStr, sizeof(valStr), "%u", store); // convert to string
                    printf("valStr: %s\r\n", valStr);
                    Paint_DrawString_EN(0, 17, valStr, &Font16, WHITE, BLACK); // show value
                } else{
                    printf("No data received: %s\r\n", valStr);
                    Paint_DrawString_EN(0, 17, valStr, &Font16, WHITE, BLACK); // show old value to make it look consistent
                }
                // show unit
                Paint_DrawString_EN(OLED_1in3_C_WIDTH - (strlen(mode[current].unit) * Font16.Width), 17, mode[current].unit, &Font16, WHITE, BLACK);
            }
            // show line and instructions
            Paint_DrawLine(0, 35, OLED_1in3_C_WIDTH, 35, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
            Paint_DrawString_EN(0, 37, "0 for mode" , &Font12, WHITE, BLACK);
            Paint_DrawString_EN(0, 49, "1 to select", &Font12, WHITE, BLACK);
        }

        OLED_1in3_C_Display(BlackImage);

        // timer for toggling the screen
        absolute_time_t now = get_absolute_time();
        if(absolute_time_diff_us(start, now) >= 500000){
            start = now;
            if (visible) {
                visible = false;
            } else {
                visible = true;
            }
        }

        // button presses
        if(DEV_Digital_Read(KEY1) == 0){
            if(key1act == 0){
                printf("key1 pressed\r\n");
                // toggle the flashing
                selected = !selected; 
                if(selected){
                    if(canID != mode[current].id){
                        canID = mode[current].id;
                        send_mode_command(canID);
                        printf("CAN ID set to: %x\r\n", canID);
                    }
                }
                //  else{ // needs to be tested -> will stop 2nd pico the old request
                //     send_mode_command(0x00); // send 0 to stop the request
                //     canID = 0x00;
                //     printf("CAN ID set to: %x\r\n", canID);
                key1act = 1;  
                sleep_ms(50); // debounce
            }
        }else {
            // if the key is released, set the action to 0
            if(key1act == 1){
                key1act = 0;
            }
            
        }
            
        if(DEV_Digital_Read(KEY0) == 0){
            if(key0act == 0 && !selected){
                printf("key0 pressed\r\n");
                // move to the next mode
                current = (current + 1) % 6;
                key0act = 1;    
                sleep_ms(50);
            }
        }else {
            if(key0act == 1){
                key0act = 0;
            }
        }
    }
    return 0;
}
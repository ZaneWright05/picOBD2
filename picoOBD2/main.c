#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "CAN/CAN_DEV_Config.h"
#include "CAN/MCP2515.h"

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

UWORD imageBuffer[OLED_1in3_C_WIDTH * OLED_1in3_C_HEIGHT / 8];
// uint8_t currPID = 0x00; // current PID

struct Mode{
    char* name; // name to display
    uint8_t id; // id to send
    char* unit; // unit to display
};

int query_CAN(uint8_t PID) { // needs to take the can ID (not 7df but the val stored in mode and the out frame >> 
    int out_value = 0;
    int timeout = 50000; // 50ms
    absolute_time_t start = get_absolute_time();
    uint32_t canID = 0x7DF; // default canID
    uint8_t frame[8] = {0x02, 0x01, PID, 0x00, 0x00, 0x00, 0x00, 0x00}; // default frame
    MCP2515_Send(canID, frame, 8);
    uint8_t reply[8] = {0};
    MCP2515_Receive(0x7E8, reply); // receive the response
    
    if (reply[1] == 0x41 && reply[2] == PID) {
        switch (PID) {
            case 0x0C: return ((reply[3] << 8) + reply[4]) / 4; // RPM
            case 0x0D: return reply[3]; // Vehicle Speed
            case 0x11: return reply[3] * 100 / 255; // Throttle Position
            case 0x04: return reply[3] * 100 / 255; // Engine load
            case 0x05: return reply[3] - 40; // Coolant Temp
            case 0x0F: return reply[3] - 40; // Intake Temp
            // case 0x5C: return reply[3] - 40; // Oil Temp not supportedby rav 4
            case 0x0B: return reply[3]; // Manifold Air Pressure
            case 0x33: return reply[3]; // Absolute Pressure
            default: return 0; // Unsupported PID
        }
    }
    return false; // No valid response

    printf("Timeout: %d\r\n", out_value);
     // store the first byte
    return out_value;
}

int main(void)
{   
    // mode array to hold the different modes
    struct Mode mode[7] = {
        {"Eng Speed", 0x0C, "rpm"},
        // {"Veh Speed", 0x0D, "km/h"},
        {"Intake temp", 0x0F, "C"},
        {"Clnt temp", 0x05, "C"},
        {"Thrttl Pos", 0x11, "%"},
        {"Engine load", 0x04, "%"},
        // {"Oil temp", 0x5C, "C"},
        {"Manif air", 0x0B, "kPa"},
        {"Abs air pre", 0x33, "kPa"}
    };
    
    DEV_Delay_ms(100);
    
    if(DEV_Module_Init()!=0){
        while(1){
            printf("END\r\n");
        }
    }
    CAN_DEV_Module_Init();
    MCP2515_Init();
    stdio_init_all();

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
                int res = query_CAN(mode[current].id); // get the value from the second pico
                if(res || (mode[current].id )){ //  get new data, if true its updated
                    printf("Received: %x\r\n", res);
                    snprintf(valStr, sizeof(valStr), "%u", res); // convert to string
                    printf("valStr: %s\r\n", valStr);
                    Paint_DrawString_EN(0, 17, valStr, &Font16, WHITE, BLACK); // show value
                } else{
                    // printf("No data received: %s\r\n", valStr);
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
                // if(selected){
                //     // if(currPID != mode[current].id){
                //     //     currPID = mode[current].id;
                //     //     printf("CAN ID set to: %x\r\n", canID);
                //     // }
                // }
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
                current = (current + 1) % 7; // loop through the modes
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
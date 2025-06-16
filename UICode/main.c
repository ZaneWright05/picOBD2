#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

// can ID to mean no request
uint8_t canID = 0x00;
uint16_t store = 0;

UWORD imageBuffer[OLED_1in3_C_WIDTH * OLED_1in3_C_HEIGHT / 8];

struct Mode{
    char* name; // name to display
    uint8_t id; // id to send
    char* unit; // unit to display
};

volatile bool key0Pressed = false;
volatile bool key1Pressed = false;

// these are used to debounce the keys
absolute_time_t lastKey0Time;
absolute_time_t lastKey1Time;

const uint64_t debounceDelayUs = 200000; // 200ms

void handleKey0Press() {
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(lastKey0Time, now) < debounceDelayUs) {
        return;
    }
    lastKey0Time = now;
    key0Pressed = true;
}

void handleKey1Press() {
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(lastKey1Time, now) < debounceDelayUs) {
        return;
    }
    lastKey1Time = now;
    key1Pressed = true;
}

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == KEY0) {
        handleKey0Press();
    } else if (gpio == KEY1) {
        handleKey1Press();
    }
}

int menuScreen(UBYTE *image){
    int selected = 0;
    int numMenuItems = 4;
    char **menuItems = (char **)malloc(numMenuItems * sizeof(char *));
    if (menuItems == NULL) {
        printf("Failed to allocate menuItems\n");
        exit(1);
    }
    menuItems[0] = strdup("Real-time data");
    menuItems[1] = strdup("Settings");
    menuItems[2] = strdup("Add car");
    menuItems[3] = strdup("CAR 1");
    while (1){
        Paint_SelectImage(image);
        Paint_Clear(BLACK);
        Paint_DrawString_EN(0, 0, "Main Menu", &Font16, WHITE, BLACK);
        Paint_DrawString_EN(0, 20, menuItems[selected], &Font16, WHITE, BLACK);
        OLED_1in3_C_Display(image);
        if (key0Pressed) {
            key0Pressed = false;
            selected = (selected + 1) % numMenuItems;
        }
        if  (key1Pressed) {
            key1Pressed = false;
            break;
        }
    }
    for (int i = 0; i < numMenuItems; i++) {
        free(menuItems[i]);
    }
    free(menuItems);
    return selected;
}

void realTimeDataScreen(UBYTE *BlackImage) {
    
    // mode array to hold the different modes
    struct Mode mode[6] = {
        {"Eng Speed", 0x0C, "rpm"},
        {"Veh Speed", 0x0D, "km/h"},
        {"Intake temp", 0x0F, "C"},
        {"Clnt temp", 0x05, "C"},
        {"Thrttl Pos", 0x11, "%"},
        {"Engine load", 0x04, "%"}
    };
    int current = 0; // current mode index
    bool visible = true; // toggle visibility of the name and unit
    bool selected = false; // if the mode is selected
    int key0act = 0; // key0 action state
    int key1act = 0; // key1 action state
    char valStr[10] = "0"; // value string to display
    uint8_t canID = 0x00; // CAN ID to send, 0x00 means no request
    absolute_time_t start = get_absolute_time(); // timer for toggling the screen
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
                // if(receive_uint16_with_header(&store)){ //  get new data, if true its updated
                //     printf("Received: %x\r\n", store);
                //     snprintf(valStr, sizeof(valStr), "%u", store); // convert to string
                //     printf("valStr: %s\r\n", valStr);
                //     Paint_DrawString_EN(0, 17, valStr, &Font16, WHITE, BLACK); // show value
                // } else{
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
                        // send_mode_command(canID);
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
}

int main(void)
{   
    DEV_Delay_ms(100);
    
    if(DEV_Module_Init()!=0){
        while(1){
            printf("END\r\n");
        }
    }
    
    stdio_init_all();
    // setupUART();
    DEV_Delay_ms(100);
    OLED_1in3_C_Init();
    OLED_1in3_C_Clear();

    // while(!stdio_usb_connected()) {
    //     sleep_ms(100); // wait for USB to be connected
    // }
    
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
    
    DEV_KEY_Config(KEY0);
    gpio_set_irq_enabled_with_callback(KEY0, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    DEV_KEY_Config(KEY1);
    gpio_set_irq_enabled(KEY1, GPIO_IRQ_EDGE_FALL, true); 

    absolute_time_t start = get_absolute_time();
    int mode = 0;
    while(mode < 2) {
        absolute_time_t now = get_absolute_time();
        uint64_t elapsed_time = absolute_time_diff_us(start, now);
        // printf("Elapsed time: %d us\n", elapsed_time);
        Paint_SelectImage(BlackImage);
        Paint_Clear(BLACK);
        if(mode == 0){
            Paint_DrawString_EN(0, 0, "picOBD2", &Font24, WHITE, BLACK);
        }
        else {
            Paint_DrawString_EN(0, 0, "Welcome:", &Font16, WHITE, BLACK);
            Paint_DrawString_EN(10, 30, "<user>", &Font12, WHITE, BLACK);
        } 

        OLED_1in3_C_Display(BlackImage);
        if (elapsed_time >= 2000000 || key0Pressed || key1Pressed){
            start = now;
            mode++;
            key0Pressed = false;
            key1Pressed = false;
        }
    }
    printf("Entering menu screen...\n");
    int choice = menuScreen(BlackImage); // enter the menu screen loop
    switch (choice)
    {
    case 0: // rt data
        printf("Entering real-time data screen...\n");
        realTimeDataScreen(BlackImage);
        break;
    case 1: // settings
        printf("Entering settings screen...\n");
        break;
    case 2: // add car
        printf("Entering add car screen...\n");
        break;
    default:
        printf("Entering car screen...\n");
        // based on the choice, can determine which car to show
        break;
    }
    return 0;
}
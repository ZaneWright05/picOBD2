﻿#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "config_writer.h"
#include "ui.h"
#include "pid.h"

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

UWORD imageBuffer[OLED_1in3_C_WIDTH * OLED_1in3_C_HEIGHT / 8];

// helper function, takes a PID and the base 0x01, 0x21, ..., 
bool check_Single_PID(uint8_t response[4], uint8_t pid, uint8_t basePid){
    if(pid < basePid || pid > basePid + 31) {
        printf("PID %02X is out of range for the response frame.\n", pid);
        return false;
    }
    uint8_t index = pid - basePid; // position out of th 32 pids
    uint8_t byte = index / 8; // which byte holds the PID
    uint8_t bit = 7 - (index % 8); // which bit in the byte holds the PID
    return (response[byte] >> bit) & 0x01; // check if the bit is set
}

int scanForPIDs() {
    uint8_t frames[7][8] = {
        // {0x06, 0x41, 0x00, 0x98, 0x3B, 0x80, 0x13, 0xAA}, // 1 - 20
        {0x06, 0x41, 0x00, 0x12, 0x34, 0x56, 0x78, 0xaa} ,
        {0x06, 0x41, 0x20, 0xA0, 0x19, 0xA0, 0x01, 0xAA}, // 21 - 40
        {0x06, 0x41, 0x40, 0x40, 0xDE, 0x00, 0x00, 0xAA}, // 41 - 60 // should stop here
        {0x06, 0x41, 0x60, 0x12, 0x34, 0x56, 0x78, 0xAA}, // 61 - 80
        {0x06, 0x41, 0x80, 0x12, 0x34, 0x56, 0x78, 0xAA}, // A1 - C1
        {0x06, 0x41, 0xa1, 0xc2, 0xd3, 0xe4, 0xf5, 0xaa}, // C2 - E2
        {0x06, 0x41, 0x00, 0x12, 0x34, 0x56, 0x78, 0xaa} // E3 - F3
    };
    int frameIndex = 0; // current frame index
    int supportedPIDs = 0; // count of supported PIDs
    uint8_t basePid = 0x01;
    uint8_t dataFromResponse[4] = {frames[frameIndex][3], frames[frameIndex][4], frames[frameIndex][5], frames[frameIndex][6]}; // to hold the response data
    for (int i = 0; i < dirSize; i++){
        PIDEntry entry = pid_Dir[i];
        uint8_t pid = entry.pid;
        if (pid > basePid + 31) {
            // printf("Moving to frame %d for PIDs %02X to %02X\n", frameIndex + 1, basePid, basePid + 31);
            basePid += 32; // move to the next set of 32 PIDs
            if (!check_Single_PID(dataFromResponse, basePid - 1, basePid - 32)) {
                printf("Next set of PIDs not supported\n");
                break;
            } else {
                printf("Next set of PIDs supported\n");
            }
            frameIndex++; // move to the next frame
            dataFromResponse[0] = frames[frameIndex][3];
            dataFromResponse[1] = frames[frameIndex][4];
            dataFromResponse[2] = frames[frameIndex][5];
            dataFromResponse[3] = frames[frameIndex][6];
        }
        bool result = check_Single_PID(dataFromResponse, pid, basePid);
        if (result) {
            supportedPIDs++;
        }
        pid_Dir[i].supported = result;
    }
    printf("Total supported PIDs: %d\n", supportedPIDs);
    return supportedPIDs;
    // for(int i = 0; i < dirSize; i++){
    //     PIDEntry entry = pid_Dir[i];
    //     printf("PID %02X (%s), supported? %d\n", entry.pid, entry.name, entry.supported);
    // }
}

int main(void)
{   
    DEV_Delay_ms(100);
    
    if(DEV_Module_Init()!=0){
        while(1){
            printf("END\r\n");
        }
    }

    DEV_KEY_Config(KEY0);
    // gpio_set_irq_enabled_with_callback(KEY0, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    DEV_KEY_Config(KEY1);
    stdio_init_all();
    while(!stdio_usb_connected()) {
        sleep_ms(100); // wait for USB to be connected
    }

    DEV_Delay_ms(100);
    OLED_1in3_C_Init();
    OLED_1in3_C_Clear();

    //  UI setup from waveshare: https://www.waveshare.com/wiki/Pico-OLED-1.3#Overview
    UBYTE *BlackImage;
    UWORD Imagesize = ((OLED_1in3_C_WIDTH%8==0)? (OLED_1in3_C_WIDTH/8): (OLED_1in3_C_WIDTH/8+1)) * OLED_1in3_C_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        while(1){
            printf("Failed to apply for black memory...\r\n");
        }
    }
    Paint_NewImage(BlackImage, OLED_1in3_C_WIDTH, OLED_1in3_C_HEIGHT, 0, WHITE);	
    
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    OLED_1in3_C_Display(BlackImage);

    char userName[8] = "<user>"; // default username
    if(!find_in_config("username=", userName, sizeof(userName))){
        strcpy(userName, "<user>"); // default username if not found
    }

    load_screen(BlackImage, userName);
    printf("Entering menu screen...\n");
    int numOfScreens = 4; // number of screens in the menu
    char** screens = (char**)malloc(numOfScreens * sizeof(char*)); // allocate memory for the screens
    screens[0] = "QUICK       QUERY"; // spacing to use new line
    screens[1] = "DTC CHECK";
    screens[2] = "SCAN AND    LOG";
    screens[3] = "QUIT";
    while (1) {
        int choice = menu_Screen(BlackImage, numOfScreens, screens); // enter the menu screen loop
        switch (choice){
        case 0: // rt data
            printf("Entering real-time data screen...\n");
            realTimeDataScreen(BlackImage);
            break;

        case 1: // add car
            // print_config();
            printf("Entering dtc screen...\n");
            // print_config();
            break;

        case 2: // scan and log
            printf("Entering scan and log screen...\n");
            int numToLog = chooseNumber(BlackImage); // choose number of PIDs to log
            printf("Number of PIDs to log: %d\n", numToLog);
            int supportedPIDs = scanForPIDs();
            choosePIDs(BlackImage, pid_Dir, supportedPIDs, dirSize, numToLog);
            break;

        case 3: // quit
            printf("Exiting...\n");
            for (int i = 0; i < numOfScreens; i++) {
                free(screens[i]); // free the screen names
            }
            free(screens); // free the screens memory
            OLED_1in3_C_Clear();
            OLED_1in3_C_Display(BlackImage);
            DEV_Module_Exit();
            free(BlackImage);
            return 0; // exit the program

        default:
            printf("Entering car screen...\n");
            // based on the choice, can determine which car to show
            break;
        }
    }
    return 0;
}
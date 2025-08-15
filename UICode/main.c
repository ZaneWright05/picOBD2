#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "config_writer.h"
#include "ui.h"
#include "pid.h"
#include "lib/CAN/CAN_DEV_Config.h"
#include "lib/CAN/MCP2515.h"


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
    uint8_t searchFrame[8] = {0x02, 0x01, 0x00, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}; // frame to send, mode 1 and PID 0x00
    uint8_t response[8] = {0}; // buffer to hold the response
    MCP2515_Send(0x7DF, searchFrame, 8); // send the frame
    MCP2515_Receive(0x7E8, response); // receive the response
    int frameIndex = 0; // current frame index
    int supportedPIDs = 0; // count of supported PIDs
    uint8_t basePid = 0x01;
    uint8_t dataFromResponse[4] = {response[3], response[4], response[5], response[6]};
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
            searchFrame[2] = basePid - 1;
            for(int j = 0; j < 8; j++) {
                printf("%02X ", searchFrame[j]);
            }
            printf("\n");
            MCP2515_Send(0x7DF, searchFrame, 8);
            MCP2515_Receive(0x7E8, response);

            dataFromResponse[0] = response[3];
            dataFromResponse[1] = response[4];
            dataFromResponse[2] = response[5];
            dataFromResponse[3] = response[6];
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

int query_CAN(uint8_t PID) {
    uint8_t frame[8] = {0x02, 0x01, PID, 0x00, 0x00, 0x00, 0x00, 0x00};// frame to send, 2 bytes, mode 1 and the PID
    uint32_t canID = 0x7DF; // broadcast ID
    uint8_t reply[8] = {0};
    MCP2515_Send(canID, frame, 8); // send the frame
    MCP2515_Receive(0x7E8, reply); // receive the response
    printf("Received reply: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X ", reply[i]);
    }
    printf("\n");
    return 0;
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

    CAN_DEV_Module_Init();
    MCP2515_Init();

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
            PIDEntry* chosenPIDS = choosePIDs(BlackImage, pid_Dir, supportedPIDs, dirSize, numToLog);
            char * fileName = name_File(BlackImage); // get the file name
            fileName = realloc(fileName, strlen(fileName) + 5);
            char * csvHeader = malloc(512); // allocate memory for the CSV header
            char * strBuffer = malloc(256);
            if (!csvHeader || !strBuffer || !fileName) {
                printf("Memory allocation failed for csvHeader.\n");
                free(chosenPIDS);
                free(screens);
                free(BlackImage);
                DEV_Module_Exit();
                return -1; // exit with error
            }
            strcat(fileName,".csv");
            strcpy(csvHeader, "Timestamp,");
            for(int i = 0; i < numToLog; i++){
                PIDEntry entry = chosenPIDS[i];
                    if (i == numToLog - 1) {
                        snprintf(strBuffer, 256, "%s", entry.name);
                    } else {
                        snprintf(strBuffer, 256, "%s,", entry.name);
                    }
                    strcat(csvHeader, strBuffer); // append the PID to the header
            }
            printf("CSV Header: %s\n", csvHeader);
            free(strBuffer);
            // now we can create the CSV file and begin logging
            // have access to, array of PID entries, number of PIDS, file name, and csv header
            int result = create_csv_file(fileName, csvHeader); // create the CSV file with the header
            while(result == -2){
                printf("File %s already exists. Please choose a different name.\n", fileName);
                fileName = name_File(BlackImage); // get a new file name
                snprintf(fileName, sizeof(fileName)+5, "%s.csv", fileName);
                result = create_csv_file(fileName, csvHeader); // try to create the CSV file again
            }
            if (result < 0) {
                printf("Failed to create CSV file.\n");
                free(chosenPIDS);
                free(screens);
                free(BlackImage);
                DEV_Module_Exit();
                return -1; // exit with error
            }
            free(csvHeader);
            printf("CSV file created: %s\n", fileName);

            absolute_time_t start = get_absolute_time();
            int time = 0; // time in seconds
            strBuffer = calloc(256, sizeof(char));
            FIL *csvFile = open_File(fileName); 
            if (!csvFile) {
                printf("Failed to open CSV file for logging.\n");
                free(chosenPIDS);
                free(BlackImage);
                DEV_Module_Exit();
                return -1; // exit with error
            }
            while (1) {
                // need to try opening the file and keeping it open
                absolute_time_t now = get_absolute_time();
                uint64_t elapsed_time = absolute_time_diff_us(start, now);
                if (elapsed_time >= 1000000) { // log every second
                    start = now;
                    time++;
                    snprintf(strBuffer, 256, "%d,1,2,3\n", time); // line to write
                    log_record(strBuffer, csvFile);
                    printf("Time = %d.", time);
                    if(time == 60){
                        break;
                    }
                }
            }
            close_File(csvFile);
            printf("Logging completed. Data saved to %s\n", fileName);
            free(chosenPIDS);
            free(strBuffer);
            free(fileName);
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
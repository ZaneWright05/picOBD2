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
    // uint8_t frames[7][8] = {
    //     {0x06, 0x41, 0x00, 0x98, 0x3B, 0x79, 0x13, 0xAA}, // 1 - 20
    //     {0x06, 0x41, 0x20, 0xA0, 0x19, 0xA0, 0x01, 0xAA}, // 21 - 40
    //     {0x06, 0x41, 0x40, 0x40, 0xDE, 0x00, 0x00, 0xAA}, // 41 - 60 // should stop here
    //     {0x06, 0x41, 0x60, 0x12, 0x34, 0x56, 0x78, 0xAA}, // 61 - 80
    //     {0x06, 0x41, 0x80, 0x12, 0x34, 0x56, 0x78, 0xAA}, // A1 - C1
    //     {0x06, 0x41, 0xa1, 0xc2, 0xd3, 0xe4, 0xf5, 0xaa}, // C2 - E2
    //     {0x06, 0x41, 0xc1, 0xd2, 0xe3, 0xf4, 0xa5, 0xaa} // E3 - F3
    // };
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
            // MCP2515_Send(0x7DF, searchFrame, 8);
            // MCP2515_Receive(0x7E8, response);

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

int query_CAN(PIDEntry entry) {
    uint8_t frame[8] = {0x02, 0x01, entry.pid, 0x00, 0x00, 0x00, 0x00, 0x00};// frame to send, 2 bytes, mode 1 and the PID
    uint32_t canID = 0x7DF; // broadcast ID
    uint8_t reply[8] = {0};
    MCP2515_Send(canID, frame, 8); // send the frame
    MCP2515_Receive(0x7E8, reply); // receive the response
    // printf("Received reply: ");
    // for (int i = 0; i < 8; i++) {
    //     printf("%02X ", reply[i]);
    // }
    // printf("\n");
    int value, a, b;
    float divisor;
    if (reply[0] > 0x03){ // data len is > 3 so we have mode = 1, pid = 2, a = 3, b = 3
        a = reply[3];
        b = reply[4];
        divisor = entry.divisor;
        value = entry.form(a, b, divisor); 
    } else{
        a = reply[3];
        b = 0; // no data here
        divisor = entry.divisor;
        value = entry.form(a, b, divisor);
    }
    return value;
}

void printPIDs(PIDEntry* pids, int max) {
    for (int i = 0; i < max; i++) {
        PIDEntry entry = pids[i];
        printf("%d. %s (%02X)\n", i + 1, entry.name, entry.pid);
    }
}

int main(void)
{   
    stdio_init_all();


    while(!stdio_usb_connected()) {
        sleep_ms(100); // wait for USB to be connected
    }
    
    // stdio_init_all();
    //  absolute_time_t test = get_absolute_time();
    CAN_DEV_Module_Init();
    MCP2515_Init();
    // absolute_time_t test2 = get_absolute_time();
    // printf("MCP2515 init time: %lld us\n", absolute_time_diff_us(test, test2));

    // DEV_Delay_ms(100);
    // OLED_1in3_C_Init();
    // OLED_1in3_C_Clear();

    char userName[8] = "<user>"; // default username
    // selectSD();
    if(!find_in_config("username=", userName, sizeof(userName))){ // not working right now
        strcpy(userName, "<user>"); // default username if not found
    }
    printf("Username: %s\n", userName);

    int supportedPIDs = scanForPIDs();
    PIDEntry supportedPIDArray[supportedPIDs];
    int current = 0;
    for (int i = 0; i < dirSize; i++) {
        if (pid_Dir[i].supported) {
            supportedPIDArray[current] = pid_Dir[i];
            current++;
        }
    }

    int maxToLog = supportedPIDs < 6 ? supportedPIDs : 6;
    printf("Please choose a number of PIDs to log (max %d): ", maxToLog);
    fflush(stdout);
    sleep_ms(100);
    int numToLog = 0;
    char buf[32];

    if (fgets(buf, sizeof(buf), stdin)) {
        sscanf(buf, "%d", &numToLog);
    }
    while (numToLog < 1 || numToLog > maxToLog) {
        printf("Invalid number. Please choose a number between 1 and %d: ", maxToLog);
        scanf("%d", &numToLog);
    }

    PIDEntry chosenPIDS[numToLog];
    int count = 0;
    while (count < numToLog) {
        printPIDs(supportedPIDArray, current);
        printf("Remaining PIDs to choose: %d/%d\n", count, numToLog);
        int inp = 0;
        scanf("%d", &inp);
        if (inp > 0 && inp < dirSize){
            chosenPIDS[count] = supportedPIDArray[inp - 1];
            count++;
        }
    }
    char header[256] = "Timestamp";
    char * buffer = malloc(64);
    for (int i = 0; i < numToLog; i++){
        PIDEntry entry = chosenPIDS[i];
        printf("Chosen PID %02X (%s)\n", entry.pid, entry.name);
        snprintf(buffer, 64, ",%s", entry.name);
        strcat(header, buffer);
    }

    printf("CSV Header: %s\n", header);
    free(buffer);

    printf("Enter a name for the log file (without extension):\n");
    char fileName[32];
    scanf("%31s", fileName);
    strcat(fileName,".csv");
    int result = create_csv_file(fileName, header);
    while(result == -2){
                printf("File %s already exists. Please choose a different name.\n", fileName);
                scanf("%31s", fileName);                
                strcat(fileName,".csv");
                result = create_csv_file(fileName, header); // try to create the CSV file again
            }
    if (result < 0) {
        printf("Failed to create CSV file.\n");
        return -1;
    }
    printf("Log file will be: %s\n", fileName);

    int time = 0;
    FIL* csvFile = open_File(fileName);
    if (!csvFile) {
        printf("Failed to open CSV file for logging.\n");
        return -1; // exit with error
    }

    printf("Ready to begin logging, press enter to begin...");
    getchar();

   
    absolute_time_t start = get_absolute_time();
    int currentPID = 0;
    //         int mainVal = 0;
            // bool sdUsed = false;
    char * recordBuffer = calloc(256, sizeof(char));
    while (1) {
        absolute_time_t now = get_absolute_time();   
        uint64_t elapsed_time = absolute_time_diff_us(start, now);
        if (currentPID < numToLog) {
            PIDEntry entry = chosenPIDS[currentPID];
            if (entry.supported) {
                char valueBuffer[32];
                int val = query_CAN(entry);
                // int val = 100;
                snprintf(valueBuffer, 32, ",%d", val);
                strncat(recordBuffer, valueBuffer, 256 - strlen(recordBuffer) - 1);
                currentPID++;
            }
        }
        if (elapsed_time >= 1000000) { // log every second
            start = now;
            time++;
            char record[256];
            if (currentPID < numToLog) {
                for (int i = currentPID; i < numToLog; i++) {
                    strncat(recordBuffer, ",", 256 - strlen(recordBuffer) - 1); // fill with empty values
                }
            }
            snprintf(record, sizeof(record), "%d", time);
            strncat(record, recordBuffer, 256 - strlen(recordBuffer) - 1);
            strncat(record, "\n", 256 - strlen(record) - 1);
            printf("%s", record);
            log_record(record, csvFile);
            memset(recordBuffer, 0, 256);
            currentPID = 0;
            if(time == 15){
                break;
            }
        }
    }
    close_File(csvFile);
    printf("Logging completed. Data saved to %s\n", fileName);
    //         free(chosenPIDS);
    //         free(strBuffer);
    //         free(fileName);
    //         break;

        // case 3: // quit
        //     printf("Exiting...\n");
        //     for (int i = 0; i < numOfScreens; i++) {
        //         free(screens[i]); // free the screen names
        //     }
        //     free(screens); // free the screens memory
        //     OLED_1in3_C_Clear();
        //     OLED_1in3_C_Display(BlackImage);
        //     DEV_Module_Exit();
        //     free(BlackImage);
        //     return 0; // exit the program

        // default:
        //     printf("Entering car screen...\n");
        //     // based on the choice, can determine which car to show
        //     break;
        // }
    // }
    return 0;
}
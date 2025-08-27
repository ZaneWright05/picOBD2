#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// #include "OLED_1in3_c.h"
// #include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "config_writer.h"
// #include "ui.h"
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
    uint8_t response[8] = {0x06, 0x41, 0x40, 0x40, 0xDE, 0x00, 0x00, 0xAA}; // buffer to hold the response
    // MCP2515_Send(0x7DF, searchFrame, 8); // send the frame
    // MCP2515_Receive(0x7E8, response); // receive the response
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

char inputBuffer[32];
int inputPos = 0;
int nonBlockRead(){
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) return -1;
    if (ch == '\n' || ch == '\r'){
        inputBuffer[inputPos] = '\0';
        inputPos = 0;
        return 1;
    } else if (inputPos < sizeof(inputBuffer) - 1){
        inputBuffer[inputPos++] = (char)ch;
    }
   return 0;
}

int main(void)
{   
    stdio_init_all();

    while(!stdio_usb_connected()) {
        sleep_ms(100); // wait for USB to be connected
    }
    
    CAN_DEV_Module_Init();
    MCP2515_Init();

    char userName[8] = "<user>"; // default username
    if(!find_in_config("username=", userName, sizeof(userName))){ // this is more a check that the SD card is working now
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
    printf("Please choose a number of PIDs to log (max %d):\n", maxToLog);
    fflush(stdout);
    sleep_ms(100);

    // enter a wait for input state
    int numToLog = 0;
    do {
    int lineReady = 0;
    while(!lineReady){
        if(nonBlockRead() == 1){ // this means an entire line has been read
            lineReady = 1;
            numToLog = atoi(inputBuffer); // attempt to convert to int
        }
        sleep_ms(10);
    }
    if (numToLog < 1 || numToLog > maxToLog) { // catch any wrong numbers or char converts
            printf("Invalid number. Please choose a number between 1 and %d: \n", maxToLog);
    }
    } while (numToLog < 1 || numToLog > maxToLog);


    PIDEntry chosenPIDS[numToLog];
    int count = 0;
    
    while(count< numToLog){
        printPIDs(supportedPIDArray, current);
        printf("Remaining PIDs to choose: %d/%d\n", count, numToLog);
        fflush(stdout);
        int inp = 0;
        int lineReady = 0;
        while (!lineReady) {
            if(nonBlockRead() == 1){
                lineReady = 1;
                inp = atoi(inputBuffer); // attempt to convert to int
                }
                sleep_ms(10);
            }
            if (inp < 1 || inp > supportedPIDs){
                printf("Please select a valid PID.\n");
            } else{
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

    printf("Enter a name for the log file (without extension, max 32 chars):\n");
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

    printf("Ready to begin logging, press any key to begin...");
    getchar();
    printf("Logging started, you may now disconnect without affecting logging (if power is not through USB).\n");

    absolute_time_t start = get_absolute_time();
    int currentPID = 0;
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
    return 0;
}
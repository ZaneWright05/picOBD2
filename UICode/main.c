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

#define GPIO_IRQ_LEVEL_LOW     0x1
#define GPIO_IRQ_LEVEL_HIGH    0x2
#define GPIO_IRQ_EDGE_FALL     0x4
#define GPIO_IRQ_EDGE_RISE     0x8

#define RED_PIN 16 // error
#define GREEN_PIN 17 // terminal
#define BLUE_PIN 18 // pico
#define YELLOW_PIN 19 // sd

typedef enum State {IDLE, TERMINAL, PICO, SD, LOGGING, ERROR} State;
typedef enum Lighting {ON, OFF, FLASH} Lighting;

typedef struct Input {
    State inputSource;
    bool active;
    int ledPin;
} Input;

State state = IDLE;

#define MAX_LOG_PIDS 6
#define MAX_FILENAME_LEN 64

typedef struct {
    char fileName[MAX_FILENAME_LEN];
    int numToLog;
    PIDEntry chosenPIDs[MAX_LOG_PIDS]; 
} InputConfig;

Input leds[4] = {
    {TERMINAL, false, GREEN_PIN}, 
    {PICO, false, BLUE_PIN}, 
    {SD, false, YELLOW_PIN},
    {ERROR, true, RED_PIN}
};
// int validInputs = 0;
int selectedIndex = -1;

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

absolute_time_t lastFlash;
bool ledOn = false;
// const uint32_t flashInterval = 500000;
// int flashingLED = RED_PIN;

void handle_leds(){
    const uint32_t flashInterval = 500000;
    // static bool ledOn = false;
    absolute_time_t now = get_absolute_time();

    if(selectedIndex >= 0){
        if(absolute_time_diff_us(lastFlash, now) > flashInterval) {
            lastFlash = now;
            ledOn = !ledOn;
            gpio_put(leds[selectedIndex].ledPin, ledOn);
        }
    }

    for (int i = 0; i < 4; i ++){
        if(i == selectedIndex) continue; 
        gpio_put(leds[i].ledPin, leds[i].active);
    }
}

bool check_inputs(){
    for(int i = 0; i < 3; i++){
        if(leds[i].active){
            return true;
        }
    }
    return false;
}

void select_next(){
    if (!check_inputs()) return; // need an input to be able to select
    int start = selectedIndex;
    // if(selectedIndex >= 0){
    //     // leds[selectedIndex].selected = false;
    //     // leds[selectedIndex].isLit = false; // the case where its not on when the btn is pressed
    // }
    for(int i = 1; i <= 3; i++){
        int idx = (start + i) % 3;
        if(leds[idx].active){
            // leds[idx].selected = true;
            selectedIndex = idx;

            // get flashing working immediately
            ledOn = false;
            gpio_put(leds[idx].ledPin, 0);
            lastFlash = get_absolute_time();
            return;
        }
    }
}

absolute_time_t lastPress;
bool INTERRUPTED = false;
void print(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    if(absolute_time_diff_us(lastPress, now) > 200000){
        // printf("interrupt detected\n");
        lastPress = now;
        INTERRUPTED = true;
        select_next();
    }
}

absolute_time_t last_event;
absolute_time_t press_start;
bool short_press = false;
bool long_press = false;

void button_event(uint gpio, uint32_t event){
    absolute_time_t now = get_absolute_time();

    if (absolute_time_diff_us(last_event, now) < 50000){
        return;
    }
    last_event = now;

    if(event & GPIO_IRQ_EDGE_FALL){
        // printf("Button pressed\n");
        press_start = now;
    }

    if(event & GPIO_IRQ_EDGE_RISE){
        // printf("Button released\n");
        uint64_t time_held = absolute_time_diff_us(press_start, now);
        if(time_held < 500000){
            short_press = true;
        }
        else{
            long_press = true;
        }
    }
}

int get_terminal_input(InputConfig *IC){
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
    // sleep_ms(100);

    // enter a wait for input state
    // int numToLog = 0;
    do {
    int lineReady = 0;
    while(!lineReady){
        if(nonBlockRead() == 1){ // this means an entire line has been read
            lineReady = 1;
            IC->numToLog = atoi(inputBuffer); // attempt to convert to int
        }
        sleep_ms(10);
    }
    if (IC->numToLog < 1 || IC->numToLog > maxToLog) { // catch any wrong numbers or char converts
            printf("Invalid number. Please choose a number between 1 and %d: \n", maxToLog);
    }
    } while (IC->numToLog < 1 || IC->numToLog > maxToLog);

    // chosenPIDS = malloc(numToLog * sizeof(PIDEntry));
    // PIDEntry chosenPIDS[numToLog];
    int count = 0;

    while(count< IC->numToLog){
        printPIDs(supportedPIDArray, current);
        printf("Remaining PIDs to choose: %d/%d\n", count, IC->numToLog);
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
                IC->chosenPIDs[count] = supportedPIDArray[inp - 1];
                count++;
        }
    }

    char header[256] = "Timestamp";
    char buffer[64];
    for (int i = 0; i < IC->numToLog; i++){
        PIDEntry entry = IC->chosenPIDs[i];
        printf("Chosen PID %02X (%s)\n", entry.pid, entry.name);
        snprintf(buffer, sizeof(buffer), ",%s", entry.name);
        strncat(header, buffer, sizeof(header) - strlen(header) - 1);
    }

    printf("CSV Header: %s\n", header);
    // free(buffer);

    printf("Enter a name for the log file (without extension, max 32 chars):\n");

    int lineReady = 0;
    while (!lineReady) {
        if(nonBlockRead() == 1){
            strncpy(IC->fileName, inputBuffer, MAX_FILENAME_LEN - 5);
            IC->fileName[MAX_FILENAME_LEN - 5] = '\0'; // safety
            lineReady = 1;
        }
        sleep_ms(10);
    }

    strcat(IC->fileName,".csv");

    int result = create_csv_file(IC->fileName, header);
    while(result == -2){
        printf("File %s already exists. Please choose a different name.\n", IC->fileName);
        lineReady = 0;
        while (!lineReady) {
            if(nonBlockRead() == 1){
                strncpy(IC->fileName, inputBuffer, MAX_FILENAME_LEN - 5);
                IC->fileName[MAX_FILENAME_LEN - 5] = '\0'; // safety
                lineReady = 1;
            }
            sleep_ms(10);
        }
        strcat(IC->fileName,".csv");
        result = create_csv_file(IC->fileName, header); // try to create the CSV file again
    }
    if (result < 0) {
        printf("Failed to create CSV file.\n");
        return -1;
    }
    printf("Log file will be: %s\n", IC->fileName);
    return 0;
}

int log_loop(char* filename, int numToLog, PIDEntry* chosenPIDS){
    FIL * csvFile = open_File(filename);
    if (!csvFile) {
        printf("Failed to open CSV file for logging.\n");
        return -1; // exit with error
    }
    absolute_time_t start = get_absolute_time();
    int currentPID = 0;
    char * recordBuffer = calloc(256, sizeof(char));
    uint64_t time = 0;
    uint64_t freqRate = 1000000; // default to 1 second
    if(numToLog < 3){
        freqRate = 500000; // if more than 3 PIDs, log every 500ms
    }
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
        if (elapsed_time >= freqRate) { // log every second
            start = now;
            char record[256];
            if (currentPID < numToLog) {
                for (int i = currentPID; i < numToLog; i++) {
                    strncat(recordBuffer, ",", 256 - strlen(recordBuffer) - 1); // fill with empty values
                }
            }
            snprintf(record, sizeof(record), "%llu", time);
            strncat(record, recordBuffer, 256 - strlen(recordBuffer) - 1);
            strncat(record, "\n", 256 - strlen(record) - 1);
            printf("%s", record);
            log_record(record, csvFile);
            memset(recordBuffer, 0, 256);
            currentPID = 0;
            if(short_press || long_press){ // wait till next log to stop
                short_press = false;
                long_press = false;
                break;
            }
            time += freqRate; // convert microseconds to seconds
        }
    }
    close_File(csvFile);
    printf("Logging completed.\n");
    return 0;
}

void init_all(){
    stdio_init_all();

    gpio_init(15);
    gpio_set_dir(15, GPIO_IN);
    gpio_pull_up(15);
    gpio_set_irq_enabled_with_callback(15, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &button_event);

    gpio_init(RED_PIN);
    gpio_set_dir(RED_PIN, GPIO_OUT);
    gpio_init(GREEN_PIN);
    gpio_set_dir(GREEN_PIN, GPIO_OUT);
    gpio_init(BLUE_PIN);
    gpio_set_dir(BLUE_PIN, GPIO_OUT);
    gpio_init(YELLOW_PIN);
    gpio_set_dir(YELLOW_PIN, GPIO_OUT);
   
    CAN_DEV_Module_Init();
    MCP2515_Init();
}

int main(void) {
    init_all();
    lastFlash = get_absolute_time();
    absolute_time_t SD_checked = get_absolute_time();
    bool SD_available = false;
    // char* fileName;
    // int* numToLog;
    // PIDEntry* chosenPIDS;
    InputConfig IC;

    while(1){
        handle_leds();
        
        switch (state)
        {
        case IDLE:
            if(absolute_time_diff_us(SD_checked, get_absolute_time()) > 50000){
                SD_checked = get_absolute_time();
                SD_available = is_valid_FS(); // every 500ms check if the SD card is available, without this no logging can occur
            }

            if(!SD_available) {
                for(int i = 0; i < 4; i++){
                    leds[i].active = false;
                    // leds[i].selected = false;
                    // leds[i].isLit = false;
                }
                // selectedIndex = -1;
                leds[3].active = true; // error if there is no SD
                break; // don't bother checking others
            }

            if(short_press){ // check for a button press
                select_next();
                short_press = false;
            }  

            if(long_press){
                long_press = false;
                if(leds[selectedIndex].inputSource == TERMINAL){
                    printf("Selected terminal output");
                    state = TERMINAL;
                } else if(leds[selectedIndex].inputSource == SD){
                    printf("Selected SD card output");
                    state = SD;
                } else if(leds[selectedIndex].inputSource == PICO){
                    printf("Selected USB output");
                    state = PICO;
                }
                selectedIndex = -1;
                break;
            }

            if(stdio_usb_connected()){
                if(!leds[0].active){
                    printf("USB connected\n");
                    // gpio_put(leds[0].ledPin, 1);
                    leds[0].active = true;
                    leds[3].active = !check_inputs();
                    if(selectedIndex == -1) selectedIndex = 0;
                    // if(!any_selected()){
                    //     // leds[0].selected = true;
                    //     selectedIndex = 0;
                    // }
                    // validInputs++;
                }
            } else{
                if(leds[0].active){ // move off of this option
                    leds[0].active = false;
                    select_next();
                }
                leds[0].active = false;
                leds[3].active = !check_inputs();
            }
            if(SD_available){ // config check will occur later, currently tie to SD availability
                if(!leds[2].active){
                    leds[2].active = true;
                    leds[3].active = !check_inputs();
                    if(selectedIndex == -1) selectedIndex = 2;
                }
            }
            else{
                leds[2].active = false;
                leds[3].active = !check_inputs();
            }
            break;

        case TERMINAL:
            for(int i = 0; i < 4; i++){ // clears leds and enables TERMINAL LED
                if(leds[i].inputSource == TERMINAL){
                    gpio_put(leds[i].ledPin, 1);
                    leds[i].active = true;
                }else {
                    gpio_put(leds[i].ledPin, 0);
                    leds[i].active = false;
                }
            }
            //extract from terminal
            int result = get_terminal_input(&IC);
            printf("Filename: %s\n", IC.fileName); // need to fix
            printf("Num to log: %d\n", IC.numToLog);
            for (int i = 0; i < IC.numToLog; i++) {
                printf("PID %d: %02X (%s)\n", i, IC.chosenPIDs[i].pid, IC.chosenPIDs[i].name);
            }
            if(result == -1){
                state = ERROR;
                break;
            }
            state = LOGGING;
            break;

        case SD:
            for(int i = 0; i < 4; i++){ // clears leds and enables SD LED
                if(leds[i].inputSource == SD){
                    gpio_put(leds[i].ledPin, 1);
                    leds[i].active = true;
                }else {
                    gpio_put(leds[i].ledPin, 0);
                    leds[i].active = false;
                }
            }
            // extract from config
            break;

        case PICO:
            break;

        case LOGGING:
            int log_result = log_loop(IC.fileName, IC.numToLog, IC.chosenPIDs);
            if (log_result == -1) {
                state = ERROR;
            }else{
                state = IDLE;
            }
            break;

        case ERROR:
            printf("An error occurred.\n");
            gpio_put(leds[3].ledPin, 1);
            break;
        default:
            break;
        }
    }
}

//     int supportedPIDs = scanForPIDs();
//     PIDEntry supportedPIDArray[supportedPIDs];
//     int current = 0;
//     for (int i = 0; i < dirSize; i++) {
//         if (pid_Dir[i].supported) {
//             supportedPIDArray[current] = pid_Dir[i];
//             printf("Supported PID: %02X (%s)\n", pid_Dir[i].pid, pid_Dir[i].name);
//             current++;
//         }
//     }

//     char logBuffer[8];
//     if(!find_in_config("count=", logBuffer, sizeof(logBuffer))){
//         printf("Failed to find count in config\n");
//         return -1;
//     }

//     int numToLog = atoi(logBuffer);
//     if(numToLog < 1 || numToLog > supportedPIDs){
//         printf("Invalid count in config\n");
//         return -1;
//     }

//     PIDEntry chosenPIDS[numToLog];
//     int count = 0;
    
//     char pidBuffer[128];
//     if(!find_in_config("PIDs=", pidBuffer, sizeof(pidBuffer))){
//         printf("Failed to find PIDs in config\n");
//         return -1;
//     }
//     char * token = strtok(pidBuffer, ",");
//     while (token){
//         uint8_t pid = (uint8_t)strtol(token, NULL, 16);
//         printf("Looking for PID %02X\n", pid);
//         bool flag = false;
//         for (int i = 0; i< dirSize; i++){
//             if (pid_Dir[i].pid == pid && pid_Dir[i].supported){
//                 flag = true;
//                 printf("%s,%02X\n", pid_Dir[i].name, pid_Dir[i].pid);
//                 chosenPIDS[count] = pid_Dir[i];
//                 count++;
//                 break;
//             }
//         }
//         if(!flag){
//             printf("Not a supported PID: %02X\n", pid);
//             return -1;
//         }
//         token = strtok(NULL, ",");
//     }

//     if(count > numToLog){
//         printf("Found more PIDs than expected\n");
//         return -1;
//     }

//     char header[256] = "Timestamp";
//     char * buffer = malloc(64);
//     for (int i = 0; i < numToLog; i++){
//         PIDEntry entry = chosenPIDS[i];
//         printf("Chosen PID %02X (%s)\n", entry.pid, entry.name);
//         snprintf(buffer, 64, ",%s", entry.name);
//         strcat(header, buffer);
//     }

//     printf("CSV Header: %s\n", header);
//     free(buffer);


//     char fileName[32];
//     if(!find_in_config("filename=", fileName, sizeof(fileName))){
//         printf("Failed to find filename in config\n");
//         return -1;
//     }

//     // strcat(fileName,".csv");
//     int result = create_csv_file(fileName, header);
//     if(result == -2){
//         printf("File %s already exists. Please choose a different name.\n", fileName);
//         return -1;
//     }
//     if (result < 0) {
//         printf("Failed to create CSV file.\n");
//         return -1;
//     }
//     printf("Log file will be: %s\n", fileName);

//     FIL* csvFile = open_File(fileName);
//     if (!csvFile) {
//         printf("Failed to open CSV file for logging.\n");
//         return -1; // exit with error
//     }

//     printf("Ready to begin logging, press any key to begin...");
//     getchar();
//     printf("Logging started, you may now disconnect without affecting logging (if power is not through USB).\n");

//     log_loop(csvFile, numToLog, chosenPIDS);
//     return 0;
// }




//     FIL* csvFile = open_File(fileName);
//     if (!csvFile) {
//         printf("Failed to open CSV file for logging.\n");
//         return -1; // exit with error
//     }

//     printf("Ready to begin logging, press any key to begin...");
//     getchar();
//     printf("Logging started, you may now disconnect without affecting logging (if power is not through USB).\n");

//     absolute_time_t start = get_absolute_time();
//     int currentPID = 0;
//     char * recordBuffer = calloc(256, sizeof(char));
//     uint64_t time = 0;
//     uint64_t freqRate = 1000000; // default to 1 second
//     if(numToLog < 3){
//         freqRate = 500000; // if more than 3 PIDs, log every 500ms
//     }
//     INTERRUPTED = false;
//     while (1) {
//         absolute_time_t now = get_absolute_time();   
//         uint64_t elapsed_time = absolute_time_diff_us(start, now);
//         if (currentPID < numToLog) {
//             PIDEntry entry = chosenPIDS[currentPID];
//             if (entry.supported) {
//                 char valueBuffer[32];
//                 int val = query_CAN(entry);
//                 // int val = 100;
//                 snprintf(valueBuffer, 32, ",%d", val);
//                 strncat(recordBuffer, valueBuffer, 256 - strlen(recordBuffer) - 1);
//                 currentPID++;
//             }
//         }
//         if (elapsed_time >= freqRate) { // log every second
//             start = now;
//             char record[256];
//             if (currentPID < numToLog) {
//                 for (int i = currentPID; i < numToLog; i++) {
//                     strncat(recordBuffer, ",", 256 - strlen(recordBuffer) - 1); // fill with empty values
//                 }
//             }
//             snprintf(record, sizeof(record), "%llu", time);
//             strncat(record, recordBuffer, 256 - strlen(recordBuffer) - 1);
//             strncat(record, "\n", 256 - strlen(record) - 1);
//             printf("%s", record);
//             log_record(record, csvFile);
//             memset(recordBuffer, 0, 256);
//             currentPID = 0;
//             if(INTERRUPTED){ // wait till next log to stop
//                 break;
//             }
//             time += freqRate; // convert microseconds to seconds
//         }
//     }
//     close_File(csvFile);
//     printf("Logging completed. Data saved to %s\n", fileName);
//     return 0;
// }
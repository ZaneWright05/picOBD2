#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

bool PIDarr[200];

    // for(int j = 0; j < 4; j++){
    //     if(!fill_in_PIDs(PIDarr, frames[j], init_PID + (j * 8))){
    //         return false; // if any byte fails, return false
    //     }
    // }
// A B C D + addit for cert func
typedef int (*PIDForm) (int, int, float);

typedef struct PIDEntry {
    uint8_t pid;
    char *name;
    char *unit;
    PIDForm form;
    int divisor;
} PIDEntry;

int tempForm(int a, int b, float c) { return a - 40; }

int justA(int a, int b, float c) { return a; }

int tripleA(int a, int b, float c) { return 3 * a; }

int toPercentage(int a, int b, float c) { return (int) ((a * 100.0) / 255.0); }

int stdShift(int a, int b, float c) { return (a << 8) + b; }

int stdShiftDiv(int a, int b, float divisor) { return stdShift(a, b, 1) / divisor; }

const int dirSize = 30;

const PIDEntry pid_Dir[] = {
    {0x00, "PID check 01-20","N/A", NULL, 1.0f},
    {0x04, "Engine Load", "%", toPercentage, 1.0f},
    {0x05, "Coolant Temp", "C", tempForm, 1.0f},
    {0x0A, "Fuel Pressure", "kPA", tripleA, 1.0f},
    {0x0B, "Int manif Press", "kPA", justA, 1.0f},
    {0x0C, "Eng Speed", "RPM", stdShiftDiv, 4.0f},
    {0x0D, "Veh Speed", "km/h", justA, 1.0f},
    {0x0F, "Int Temp", "C", tempForm, 1.0f},
    {0x10, "MAF Air Flow", "g/s", stdShiftDiv, 100.0f},
    {0x11, "Throttle Pos", "%", toPercentage, 1.0f},
    {0x1C, "OBD2 std", "", NULL, 1.0f},
    {0x1F, "R.T. since strt", "s", stdShift, 1.0f},
    {0x20, "PID check 21-40","N/A", NULL, 1.0f},
    {0x21, "Dist with MIL", "km", stdShift, 1.0f},
    {0x22, "Fuel Press", "kPA", stdShiftDiv, 12.658f},
    {0x23, "Fuel Press", "kPA", stdShiftDiv, 0.1f},
    {0x30, "Warm ups since clear", "times", justA, 1.0f},
    {0x31, "Dist since clear", "km", stdShift, 1.0f},
    {0x33, "Absolute baro press", "kPA", stdShift, 1.0f},
    {0x40, "PID check 41-60", "N/A", NULL, 1.0f},
    {0x45, "Rel thrttle pos", "%", toPercentage, 1.0f},
    {0x46, "Ambient air temp", "C", tempForm, 1.0f},
    {0x47, "Absol thrrl pos B", "%", toPercentage, 1.0f},
    {0x48, "Absol thrrl pos C", "%", toPercentage, 1.0f},
    {0x49, "Absol thrrl pos D", "%", toPercentage, 1.0f},
    {0x4A, "Absol thrrl pos E", "%", toPercentage, 1.0f},
    {0x4B, "Absol thrrl pos F", "%", toPercentage, 1.0f},
    {0x4D, "Time w/ MIL on", "mins", stdShift, 1.0f},
    {0x4E, "Time since trbl clear", "min", stdShift, 1.0f},
    {0x51, "Fuel type", NULL, NULL, 1.0f},
    {0x5C, "Eng Oil temp", "C", tempForm, 1.0f}
};

bool fill_in_PIDs(bool PIDarr[200], uint8_t frames[4], int init_PID, int recursePid){
    for (int j = 0; j < 4; j++) {
        int bit = 0;
        for (int i = 7; i >= 0; i--) {
            bit = (frames[j] >> i) & 1;
            if (bit) {
                char PID[3];
                sprintf(PID, "%02X", init_PID);
                PIDarr[init_PID] = true; // Mark PID as supported
                printf("%02X ", init_PID);
            }
            else {
                PIDarr[init_PID] = false; // Mark PID as not supported
            }
            init_PID++;
        }
    }
    if (PIDarr[recursePid]) {
        return true;
    }
    return false;
}

bool get_Supported_PIDs(uint8_t *frame, int startPid, int endPid) {

    if(frame[0] != 0x06){
        printf("Incorrect frame length for PID request: %02X\n", frame[0]);
        return;
    }
    if(frame[1] != 0x41){ // correct mode
        printf("Incorrect mode for PID request: %02X\n", frame[1]);
        return;
    }
    // if(frame[2] != 0x00){ // correct PID
    //     printf("Incorrect PID for request: %02X\n", frame[2]);
    //     return;
    // }
    uint8_t dataByte1 = frame[3];
    uint8_t dataByte2 = frame[4]; 
    uint8_t dataByte3 = frame[5];
    uint8_t dataByte4 = frame[6];
    printf("Supported PIDs: ");
    uint8_t frames_arr[4] = {dataByte1, dataByte2, dataByte3, dataByte4};
    bool res = fill_in_PIDs(PIDarr, frames_arr, startPid,endPid);

    // fill_in_PIDs(PIDarr, dataByte2, 9);
    // fill_in_PIDs(PIDarr, dataByte3, 17);
    // fill_in_PIDs(PIDarr, dataByte4, 25);
    printf("\nfor this frame, ");

    if(res){
        printf("recurse expected.\n");
    }
    else{
        printf("recurse not expected.\n");
    }
    return res;
}
// structure for using fill in PIDS
// while (fill_in_PIDs(PIDarr, frames, init_PID)) {
//     init pid moved to the next set of 32 pids
//     excract the pid, for the first step 20
//     perform the query to get the next frame, using PID
//   }


void print_byte_to_PID(uint8_t byte, int init_PID) {
    for (int i = 7; i >= 0; i--) {
        // printf("%d", (byte >> i) & 1);
        int bit = (byte >> i) & 1;
        if (bit) {
            printf("%02X ", init_PID);
        }
        init_PID++;
    }
    printf(" "); // optional spacing
}


void main(){
    int numOfFrames = 7;
    uint8_t frames[7][8] = {
        {0x06, 0x41, 0x00, 0x98, 0x3B, 0x80, 0x13, 0xAA}, // 1 - 20
        {0x06, 0x41, 0x20, 0xA0, 0x19, 0xA0, 0x01, 0xAA}, // 21 - 40
        {0x06, 0x41, 0x40, 0x40, 0xDE, 0x00, 0x00, 0xAA}, // 41 - 60 // should stop here
        {0x06, 0x41, 0x60, 0x12, 0x34, 0x56, 0x78, 0xAA}, // 61 - 80
        {0x06, 0x41, 0x80, 0x12, 0x34, 0x56, 0x78, 0xAA}, // A1 - C1
        {0x06, 0x41, 0xa1, 0xc2, 0xd3, 0xe4, 0xf5, 0xaa}, // C2 - E2
        {0x06, 0x41, 0xc1, 0xd2, 0xe3, 0xf4, 0xa5, 0xaa} // E3 - F3
    };
    int sumOfPids = 0;
    int init_PID = 0x01; // starting PID
    int end_PID = 0x20; // ending PID
    for (int i = 0; i < numOfFrames; i++) {
        printf("Frame %d: ", i + 1);
        if(get_Supported_PIDs(frames[i], init_PID, end_PID)){
            init_PID += 32;
            end_PID += 32;
            for (int j = 0; j < 8; j++) {
                if (PIDarr[j]){
                    sumOfPids += init_PID + j;
                }
            }
        } else{
            break;
        }
    }
    printf("\nEncoded PID as a sum: %d\n", sumOfPids);
    for (int i = 0; i < 200; i++) {
        printf("PID %02X: %d\n", i, PIDarr[i]);
    }
    for (int i = 0; i < dirSize; i++){
        if(PIDarr[pid_Dir[i].pid]) {
            PIDEntry current = pid_Dir[i];
            printf("%s is supported, its code is %02X, and is unit %s\n", current.name, current.pid, current.unit);
            if(current.form == NULL){continue;}
            printf("Example calc with A = 10, B = 20: %d\n", current.form(10, 20, current.divisor));
        }
    }
}

void query_PID(int pid){
    printf("Querying PID: %02X\n", pid);
}

// void test_log_loop() {
//     absolute_time_t start = get_absolute_time();
//     int currentPid = 0;
//     int pids[3] = {0x01, 0x02, 0x03};
//     int numPids = 3;
//     while (true){
//         absolute_time_t now = getAbsolute_time();
//         if (absolute_time_t_diff_us(start, now) >= 500000) { // 500ms
//             start = now;
//             printf("Writing to log file...\n"); // need to work out how long it takes
//             currentPid = 0;
//         }
//         if (currentPid < numPids) {
//             query_PID(pids[currentPid]); // need to work out how long it takes
//             currentPid++;
//         } 
//         // if not we wait for the next iteration
//     }
// }

// AI version
// void test_log_loop() {
//     absolute_time_t start = get_absolute_time();
//     int pids[3] = {0x01, 0x02, 0x03};
//     const int numPids = 3;

//     while (true) {
//         absolute_time_t now = get_absolute_time();
//         if (absolute_time_diff_us(start, now) >= 500000) {
//             start = delayed_by_us(start, 500000);  // consistent interval

//             // Measure PID query time
//             absolute_time_t t_query_start = get_absolute_time();
//             for (int i = 0; i < numPids; i++) {
//                 query_PID(pids[i]);
//             }
//             absolute_time_t t_query_end = get_absolute_time();
//             printf("PID query time: %dus\n", absolute_time_diff_us(t_query_start, t_query_end));

//             // Measure log write time
//             absolute_time_t t_log_start = get_absolute_time();
//             log_to_sd();
//             absolute_time_t t_log_end = get_absolute_time();
//             printf("Log write time: %dus\n", absolute_time_diff_us(t_log_start, t_log_end));
//         }
//     }
// }

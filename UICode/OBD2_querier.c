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

bool fill_in_PIDs(bool PIDarr[200], uint8_t frames[4], int init_PID){
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
        if(j ==3 && bit){
            return true; // last byte is true means recurse for the next frame
        }
    }
    return false;
}

void get_Supported_PIDs(uint8_t *frame){

    if(frame[0] != 0x06){
        printf("Incorrect frame length for PID request: %02X\n", frame[0]);
        return;
    }
    if(frame[1] != 0x41){ // correct mode
        printf("Incorrect mode for PID request: %02X\n", frame[1]);
        return;
    }
    if(frame[2] != 0x00){ // correct PID
        printf("Incorrect PID for request: %02X\n", frame[2]);
        return;
    }
    uint8_t dataByte1 = frame[3];
    uint8_t dataByte2 = frame[4]; 
    uint8_t dataByte3 = frame[5];
    uint8_t dataByte4 = frame[6];
    printf("Supported PIDs: ");
    uint8_t frames_arr[4] = {dataByte1, dataByte2, dataByte3, dataByte4};
    bool res = fill_in_PIDs(PIDarr, frames_arr, 1);

    // fill_in_PIDs(PIDarr, dataByte2, 9);
    // fill_in_PIDs(PIDarr, dataByte3, 17);
    // fill_in_PIDs(PIDarr, dataByte4, 25);
    printf("\n");
    if(res){
        printf("for this frame, recurse expected.\n");
    }
    for (int i = 0; i < 200; i++) {
        // if (PIDarr[0][i] != NULL) {
            printf("PID %02X: %d\n", i, PIDarr[i]);
        // } 
        // else {
        //     printf("PID %02X: Not supported\n", i);
        // }
    }
}



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
    uint8_t frame[] = {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xA8, 0x13, 0xFF}; // Example frame
    get_Supported_PIDs(frame);
}


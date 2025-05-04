#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "waveshareLib/DEV_Config.h" 
#include "waveshareLib/mcp2515.h"

int run_RPM() {
    uint8_t RPM_CAN[8] = {0x02,0x01,0x0C,0x00,0x00,0x00,0x00,0x00};
    uint32_t SEND_ID = 0x7DF; // send this to the ECU
    uint32_t RESP_ID = 0x7E8; // look for CAN frames with this ID
    printf("Sending OBD-II PID 0x0C...\n");
    uint8_t CAN_RESP_BUFF[8] = {0}; // store the response from the CAN
    
    MCP2515_Send(SEND_ID, RPM_CAN, 8); // send the request to the ECU
    MCP2515_Receive(RESP_ID, CAN_RESP_BUFF); // wait for the response from the ECU, write into buffer
    while(true){
        if(CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x0C) { // [1] is response code for mode 4, [2] is PID for RPM
            uint16_t RPM = ((CAN_RESP_BUFF[3] << 8) + CAN_RESP_BUFF[4]) / 4; // calculate the RPM from the response
            // fst shift by 8 + snd / 4
            printf("RPM: %d\n", RPM); // print the RPM to the console - only if serial monitor is connected
        }
        sleep_ms(100); // wait for next request - safety (some say can to 20, but 100 is fast enough)
        memset(CAN_RESP_BUFF, 0, sizeof(CAN_RESP_BUFF)); // clear the buffer
        MCP2515_Send(SEND_ID, RPM_CAN, 8);
        MCP2515_Receive(RESP_ID, CAN_RESP_BUFF); 
    }
    return 0;
}

bool fetch_rpm_once(uint16_t* can_out) {
    uint8_t RPM_CAN[8] = {0x02, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t CAN_RESP_BUFF[8] = {0};
    MCP2515_Send(0x7DF, RPM_CAN, 8);
    MCP2515_Receive(0x7E8, CAN_RESP_BUFF);

    if (CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x0C) {
        *can_out = ((CAN_RESP_BUFF[3] << 8) + CAN_RESP_BUFF[4]) / 4;
        return true;
    }
    return false;
}

bool fetch_speed_once(uint16_t* can_out) {
    uint8_t SPD_CAN[8] = {0x02, 0x01, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t CAN_RESP_BUFF[8] = {0};
    MCP2515_Send(0x7DF, SPD_CAN, 8);
    MCP2515_Receive(0x7E8, CAN_RESP_BUFF);

    if (CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x0D) {
        *can_out = CAN_RESP_BUFF[3];
        return true;
    }
    return false;
}

bool fetch_thttle_pos_once(uint16_t* can_out) {
    uint8_t THRTL_CAN[8] = {0x02, 0x01, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t CAN_RESP_BUFF[8] = {0};
    MCP2515_Send(0x7DF, THRTL_CAN, 8);
    MCP2515_Receive(0x7E8, CAN_RESP_BUFF);

    if (CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x11) {
        *can_out = CAN_RESP_BUFF[3] * 100 / 255;
        return true;
    }
    return false;
}

bool fetch_engine_load_once(uint16_t* can_out) {
    uint8_t ENG_LD_CAN[8] = {0x02, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t CAN_RESP_BUFF[8] = {0};
    MCP2515_Send(0x7DF, ENG_LD_CAN, 8);
    MCP2515_Receive(0x7E8, CAN_RESP_BUFF);

    if (CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x04) {
        *can_out = CAN_RESP_BUFF[3] * 100 / 255;
        return true;
    }
    return false;
}

bool fetch_int_temp_once(uint16_t* can_out) {
    uint8_t INT_TEMP_CAN[8] = {0x02, 0x01, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t CAN_RESP_BUFF[8] = {0};
    MCP2515_Send(0x7DF, INT_TEMP_CAN, 8);
    MCP2515_Receive(0x7E8, CAN_RESP_BUFF);

    if (CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x0F) {
        *can_out = CAN_RESP_BUFF[3] - 40;
        return true;
    }
    return false;
}

bool fetch_clnt_temp_once(uint16_t* can_out) {
    uint8_t CLNT_TEMP_CAN[8] = {0x02, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t CAN_RESP_BUFF[8] = {0};
    MCP2515_Send(0x7DF, CLNT_TEMP_CAN, 8);
    MCP2515_Receive(0x7E8, CAN_RESP_BUFF);

    if (CAN_RESP_BUFF[1] == 0x41 && CAN_RESP_BUFF[2] == 0x05) {
        *can_out = CAN_RESP_BUFF[3] - 40;
        return true;
    }
    return false;
}

bool check_Stop(){
    if (stdio_usb_connected() && getchar_timeout_us(0) == 's') {
        char buffer[5];
        scanf("%4s", buffer);
        if (strcmp(buffer, "top") == 0) {
            printf("Stopping RPM stream.\n");
            return true;
        }
    }
    return false;
}



int main()
{
    stdio_init_all();
    while (!stdio_usb_connected()) { // wait for serial monitor, so prints aren't missed
        sleep_ms(100);
    }

    // https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979

    DEV_Module_Init();

    MCP2515_Init();

    

    while (true)
    {
        char input[32];
        printf("Enter a command (or 'exit' to quit): ");
        scanf("%31s", input);
        printf("You entered: %s\n", input);
        if (strcmp(input, "exit") == 0) {
            printf("Exiting...\n");
            sleep_ms(1000);
            return 0; 

        } else if (strcmp(input, "RPM") == 0) {
            printf("Streaming RPM. Type 'stop' to return.\n");
            while (true) {
                uint16_t rpm;
                if (fetch_rpm_once(&rpm)) {
                    printf("RPM: %u\n", rpm);
                } else {
                    printf("Failed to fetch RPM.\n");
                }
                sleep_ms(100);

                if(check_Stop()) break;
            }
        } else if (strcmp(input, "SPD") == 0){
            printf("Streaming Speed. Type 'stop' to return.\n");
            while (true) {
                uint16_t speed;
                if (fetch_speed_once(&speed)) {
                    printf("Speed: %u\n", speed);
                } else {
                    printf("Failed to fetch Speed.\n");
                }
                sleep_ms(100);

                if(check_Stop()) break;
            }
        } else if (strcmp(input, "INTAKE") == 0){
            printf("Streaming Intake Temp. Type 'stop' to return.\n");
            while (true) {
                uint16_t temp;
                if (fetch_int_temp_once(&temp)) {
                    printf("Temp: %u\n", temp);
                } else {
                    printf("Failed to fetch Intake Temp.\n");
                }
                sleep_ms(100);

                if(check_Stop()) break;
            }
        } else if (strcmp(input, "COOLANT") == 0){
            printf("Streaming Coolant Temp. Type 'stop' to return.\n");
            while (true) {
                uint16_t temp;
                if (fetch_clnt_temp_once(&temp)) {
                    printf("Temp: %u\n", temp);
                } else {
                    printf("Failed to fetch Intake Temp.\n");
                }
                sleep_ms(100);

                if(check_Stop()) break;
            }
        } else if (strcmp(input, "THRTL") == 0){
            printf("Streaming Throttle Position. Type 'stop' to return.\n");
            while (true) {
                uint16_t throttle_pos;
                if (fetch_thttle_pos_once(&throttle_pos)) {
                    printf("Throttle Position: %u%%\n", throttle_pos);
                } else {
                    printf("Failed to fetch Throttle Position.\n");
                }
                sleep_ms(100);
        
                if(check_Stop()) break;
            }
        } else if (strcmp(input, "ENGLOAD") == 0){
            printf("Streaming Engine Load. Type 'stop' to return.\n");
            while (true) {
                uint16_t load;
                if (fetch_engine_load_once(&load)) {
                    printf("Engine Load: %u%%\n", load);
                } else {
                    printf("Failed to fetch Engine Load.\n");
                }
                sleep_ms(100);
        
                if(check_Stop()) break;
            }
        }else {
            printf("Unknown command: %s\n", input);
        }
    }
    return 0;
}

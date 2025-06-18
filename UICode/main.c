#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "sd_card.h"
#include "ff.h"

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

UWORD imageBuffer[OLED_1in3_C_WIDTH * OLED_1in3_C_HEIGHT / 8];

struct Mode{
    char* name; // name to display
    uint8_t id; // id to send
    char* unit; // unit to display
};

// volatile
bool key0Pressed = false;
// volatile 
bool key1Pressed = false;

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

void pollButtons(){
    if(DEV_Digital_Read(KEY0) == 0) {
        handleKey0Press();
    } else {
        key0Pressed = false; 
    }
    if(DEV_Digital_Read(KEY1) == 0) {
        handleKey1Press();
    } else {
        key1Pressed = false;
    }
}

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == KEY0) {
        handleKey0Press();
    } else if (gpio == KEY1) {
        handleKey1Press();
    }
}

int menuScreen(UBYTE *image, int numScreens, char** screens) {
    int selected = 0;
    while (1){
        Paint_SelectImage(image);
        Paint_Clear(BLACK);
        Paint_DrawString_EN(0, 0, "Main Menu", &Font16, WHITE, BLACK);
        Paint_DrawString_EN(0, 20, screens[selected], &Font16, WHITE, BLACK);
        OLED_1in3_C_Display(image);
        pollButtons();
        if (key0Pressed) {
            key0Pressed = false;
            selected = (selected + 1) % numScreens;
        }
        if  (key1Pressed) {
            key1Pressed = false;
            break;
        }
    }
    return selected;
}

void realTimeDataScreen(UBYTE *BlackImage) {
    
    // mode array to hold the different modes
    struct Mode mode[7] = {
        {"Eng Speed", 0x0C, "rpm"},
        {"Veh Speed", 0x0D, "km/h"},
        {"Intake temp", 0x0F, "C"},
        {"Clnt temp", 0x05, "C"},
        {"Thrttl Pos", 0x11, "%"},
        {"Engine load", 0x04, "%"},
        {"BACK", 0x00, ""}
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
                if(mode[current].id == 0x00){
                    return;
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
                current = (current + 1) % 7;
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

char convertToChar(int value) {
    if (value == -1) {
        return ' '; // return space for unselected characters
    }
    if (value >= 0 && value <= 25){
        return 'A' + value; // convert 0-25 to A-Z
    } else if (value >= 26 && value <= 35) {
        return '0' + (value - 26); // convert 26-35 to 0-9
    } else {
        return '?'; // return '?' for invalid values
    }
}

char* name_Car(UBYTE *BlackImage){
    int lineWidth = (OLED_1in3_C_WIDTH - 20)/ 8; // width of the line
    absolute_time_t start = get_absolute_time();
    bool visible = true; // toggle for the selected 
    int selected = 0; // selected character index
    int characters[8] = {-1,-1,-1,-1,-1,-1,-1,-1}; // array to hold the characters
    while (1) {
        absolute_time_t now = get_absolute_time();
        uint64_t elapsed_time = absolute_time_diff_us(start, now);
        if (elapsed_time >= 500000) { // 500ms
            start = now;
            visible = !visible; // toggle visibility
        }
        Paint_SelectImage(BlackImage);
        Paint_Clear(BLACK);
        Paint_DrawString_EN(0, 0, "Enter Name:", &Font16, WHITE, BLACK);
        int y = 40;
        for (int i = 0; i < 8; i++) {
            int x1 = 2 + i * 16;
            int x2 = x1 + 12;
            char charStr[2] = {convertToChar(characters[i]), '\0'};
            Paint_DrawString_EN(x1, y - 15, charStr, &Font16, WHITE, BLACK);
            if(i == selected && !visible){
                continue;
            }
            Paint_DrawLine(x1, y, x2, y, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
        }  
        Paint_DrawString_EN(0, y + 5, "0 for next", &Font8, WHITE, BLACK);
        Paint_DrawString_EN(0, y + 13, "1 to change/select", &Font8, WHITE, BLACK);
        if(selected == 8){
            if (visible) {
                // Paint_DrawRectangle(OLED_1in3_C_WIDTH - 28 , y + 5, OLED_1in3_C_WIDTH - 2, y + 15, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
                Paint_DrawString_EN(OLED_1in3_C_WIDTH - 28 , y + 10, "Done?", &Font8, WHITE, BLACK);
            }
        } else {
            Paint_DrawString_EN(OLED_1in3_C_WIDTH - 28 , y + 10, "Done?", &Font8, WHITE, BLACK);
        }
        Paint_DrawRectangle(OLED_1in3_C_WIDTH - 30 , y + 8, OLED_1in3_C_WIDTH - 2, y + 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        
        pollButtons();
        if(key0Pressed) {
            key0Pressed = false;
            selected = (selected + 1) % 9; // move to the next character
        }
        if(key1Pressed) {
            key1Pressed = false;
            bool isDone = false;
            if (selected == 8) { // if the last character is selected, exit
                for (int i = 0; i < 8; i++) {
                    if (characters[i] != -1) { // if any character is selected
                        isDone = true;
                        break;
                    }
                }
                if (isDone){
                    break;
                }
            }
            else {
                int currChar = characters[selected];
                if(currChar == 35){
                    currChar = -1; // if the character is 36, set it to -1 (space)
                } else if(currChar == -1) {
                    currChar = 0; // if the character is -1, set it to 0 (A)
                } else {
                    currChar++;
                }
                characters[selected] = currChar; // 0-25 for A-Z, 26-35 for 0-9
            }
        }
        OLED_1in3_C_Display(BlackImage);
    }
    char *name = malloc(9);
    if (!name) return NULL;
    for (int i = 0; i < 8; i++) {
        if (characters[i] == -1) {
            name[i] = ' ';
        } else {
            name[i] = convertToChar(characters[i]);
            // printf("%c", convertToChar(characters[i]));
        }
    }
    name[8] = '\0'; // null-terminate the string
    return name;
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
    // gpio_set_irq_enabled(KEY1, GPIO_IRQ_EDGE_FALL, true); 
    // gpio_set_irq_enabled_with_callback(KEY1, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    stdio_init_all();
    // while(!stdio_usb_connected()) {
    //     sleep_ms(100); // wait for USB to be connected
    // }


    FRESULT fr;
    FATFS fs;
    FIL fil;
    DWORD nclst;
    int ret;
    char buf[100];


    bool sd_card_found = true;
    char userName[8] = "<user>"; // default user name
    // bool sdInit = sd_init_driver();
    // printf("SD card driver initialized: %s\r\n", sdInit ? "true" : "false");
     printf("Initializing SD card...\n");
    if (!sd_init_driver()) {
        printf("ERROR: Could not initialize SD card");
        sd_card_found = false;
    }
    // printf("SD card initialized successfully.\n");


    DEV_Delay_ms(100);
    OLED_1in3_C_Init();
    OLED_1in3_C_Clear();

    
    // // UI setup from waveshare: https://www.waveshare.com/wiki/Pico-OLED-1.3#Overview
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
    
    printf("SD card initialized successfully.\r\n");
    if(sd_card_found){
    fr = f_mount(&fs, "0:", 1);
        if (fr != FR_OK) {
            printf("ERROR: Could not mount filesystem (%d)\r\n", fr);
            sd_card_found = false;
        }
        if(sd_card_found){
            fr = f_open(&fil, "config.txt", FA_READ);
            if (fr != FR_OK) {
                sd_card_found = false;
            }
            else{
                while (f_gets(buf, sizeof(buf), &fil)) {
                    if (strncmp(buf, "username=", 9) == 0){
                        char *name = buf + 9; // skip "username="
                        name[strcspn(name, "\r\n")] = '\0'; // remove newline characters
                        if (strlen(name) > 0 && strlen(name) < 8) {
                            strncpy(userName, name, sizeof(userName) - 1);
                            userName[sizeof(userName) - 1] = '\0'; // ensure null termination
                        } else {
                            printf("ERROR: Invalid username in config.txt\r\n");
                        }
                        break;
                    }
                }       
            }
    }
    }

    absolute_time_t start = get_absolute_time();
    int mode = 0;
    
    while(mode < 2) {
        absolute_time_t now = get_absolute_time();
        uint64_t elapsed_time = absolute_time_diff_us(start, now);
        Paint_SelectImage(BlackImage);
        Paint_Clear(BLACK);
        if(mode == 0){
            Paint_DrawString_EN(0, 0, "picOBD2", &Font24, WHITE, BLACK);
        }
        else {
            Paint_DrawString_EN(0, 0, "Welcome:", &Font16, WHITE, BLACK);
            Paint_DrawString_EN(10, 30, userName, &Font12, WHITE, BLACK);
        } 
        pollButtons();

        OLED_1in3_C_Display(BlackImage);
        if (elapsed_time >= 2000000 || key0Pressed || key1Pressed){
            start = now;
            mode++;
            key0Pressed = false;
            key1Pressed = false;
        }
    }
    printf("Entering menu screen...\n");
    int numOfScreens = 3; // number of screens in the menu
    char** screens = (char**)malloc(numOfScreens * sizeof(char*)); // allocate memory for the screens
    screens[0] = "QUICK       QUERY"; // spacing to use new line
    screens[1] = "SETTINGS";
    screens[2] = "ADD CAR";
    while (1) {
        int choice = menuScreen(BlackImage, numOfScreens, screens); // enter the menu screen loop
        switch (choice){
        case 0: // rt data
            printf("Entering real-time data screen...\n");
            realTimeDataScreen(BlackImage);
            break;
        case 1: // settings
            printf("Entering settings screen...\n");
            break;
        case 2: // add car
            printf("Entering add car screen...\n");
            char *carName = name_Car(BlackImage); // enter the name car screen
            printf("Car name entered: %s\n", carName);
    
            char** temp = (char**)realloc(screens, (numOfScreens + 1) * sizeof(char*)); // reallocate memory for the screens
            if(temp == NULL || carName == NULL) {
                screens[numOfScreens - 1] = "ERR";
            }
            else {
                screens = temp;
                screens[numOfScreens] = strdup(carName);
                numOfScreens++;
            }
            free(carName);
            break;
        default:
            printf("Entering car screen...\n");
            // based on the choice, can determine which car to show
            break;
        }
    }
    for (int i = 0; i < numOfScreens; i++) {
        if (screens[i] != NULL) {
            free(screens[i]);
        }
    }
    free(screens); // free the screens memory
    free(BlackImage); // free the black image memory
    return 0;
}
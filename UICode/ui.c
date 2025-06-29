#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

struct Mode{
    char* name; // name to display
    uint8_t id; // id to send
    char* unit; // unit to display
};

// volatile
bool key0Pressed = false;
// volatile 
bool key1Pressed = false;

absolute_time_t lastKey0Time;
absolute_time_t lastKey1Time;
const uint64_t debounceDelayUs = 300000; // 300ms

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

void load_screen(UBYTE *BlackImage, char *userName) {
    int mode = 0;
    absolute_time_t start = get_absolute_time();
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
}

int menu_Screen(UBYTE *image, int numScreens, char** screens) {
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

char convertToChar(int value) {
    if (value == -1) {
        return ' '; // return space for unselected characters
    }
    char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if(value < 0 || value >= sizeof(alphabet) - 1) {
        return '?'; // return '?' for invalid values
    }
    return alphabet[value];
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
    int nameStart = 0;
    while (nameStart < 8 && characters[nameStart] == -1) {
        nameStart++; // find the first non-space character
    }
    int nameEnd = 7;
    while (nameEnd >= 0 && characters[nameEnd] == -1) {
        nameEnd--; // find the last non-space character
    }

    int len = nameEnd - nameStart + 1; // length of the name
    if (len <= 0) {
        printf("No valid characters entered for car name.\n");
        return NULL;
    }
    char *name = malloc(len + 1); 
    if (!name) return NULL;
    for (int i = 0; i < len; i++) {
        int val = characters[nameStart + i];
        char c = convertToChar(val);
        name[i] = c;
    }
    name[len] = '\0';
    return name;
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
        pollButtons();
        // button presses
        if(key0Pressed) {
            key0Pressed = false;
            if (selected) { // if selected, change mode
                canID = 0x00; // reset CAN ID
                selected = false; // unselect
                key0act = 0; // reset action state
            } else {
                current = (current + 1) % 7; // change mode
                canID = mode[current].id; // set CAN ID
                key0act = 1; // set action state to change mode
            }
        }
        if(key1Pressed) {
            key1Pressed = false;
            if (current == 6) { // if BACK is selected, exit
                return;
            }
            if (selected) { // if selected, send request
                selected = false; // unselect
                key1act = 0; // reset action state
            } else {
                selected = true; // select mode
                key1act = 1; // set action state to select
            }
        }
    }
}

int car_Screen(UBYTE *image, int numScreens, char screens[][9]) {
    int selected = 0;
    while (1){
        Paint_SelectImage(image);
        Paint_Clear(BLACK);
        Paint_DrawString_EN(0, 0, "Choose car:", &Font16, WHITE, BLACK);
        Paint_DrawString_EN(0, 20, screens[selected], &Font16, WHITE, BLACK);
        OLED_1in3_C_Display(image);
        pollButtons();
        if (key0Pressed) {
            key0Pressed = false;
            selected = (selected + 1) % numScreens;
        }
        if (key1Pressed) {
            key1Pressed = false;
           if(strcmp(screens[selected], "BACK") == 0) {
                printf("Exiting car selection\r\n");
                return -1;
            }
            break;
        }
    }
    return selected;
}
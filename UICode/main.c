#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "OLED_1in3_c.h"
#include "GUI_Paint.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "config_writer.h"
#include "ui.h"

#define KEY0 15 // GPIO key used for the screen btns
#define KEY1 17

UWORD imageBuffer[OLED_1in3_C_WIDTH * OLED_1in3_C_HEIGHT / 8];

// for the config file, updating 
// config file updated -> this will be used as a fixed truth 
// i.e when ever a car is added or removed we take from config so that the code state and the cofig always match)

int maxCars = 0;
int currentCars = 0;

// void gpio_callback(uint gpio, uint32_t events) {
//     if (gpio == KEY0) {
//         handleKey0Press();
//     } else if (gpio == KEY1) {
//         handleKey1Press();
//     }
// }

int populate_car_array(char carArray[][9] , int currentCars, int maxCars){
    for (int i = 0; i < maxCars + 1; i++) {
        strcpy(carArray[i], "");
    }

    int strLen = 10*currentCars; // 8 chars, delimiter, null terminator
    char *carStr = malloc(strLen);
    if(carStr == NULL){
        printf("Failed to allocate memory for car string\n");
        return 0;
    }
    find_in_config("cars=", carStr, strLen);
    if (strlen(carStr) == 0) {
        free(carStr);
        return 0;
    }
    char *token = strtok(carStr, ",");
    int index = 0;
    while (token != NULL) {
        strcpy(carArray[index], token);
        index++;
        token = strtok(NULL, ",");
    }
    strcpy(carArray[currentCars], "BACK"); // add BACK option 
    free(carStr);
    return index;
}

bool increment_current_cars(){
    char temp[16];
    if(find_in_config("currentCars=", temp, sizeof(temp))){
        currentCars = atoi(temp);
    } else{
        currentCars = 0;
    }
    currentCars++;
    char currentCarsStr[16];
    snprintf(currentCarsStr, sizeof(currentCarsStr), "%d", currentCars);
    if(!update_config("currentCars=", currentCarsStr)){
        printf("Failed to update current cars in config file.\n");
        return false; // exit if failed
    }
    return true;
}

bool decrement_current_cars(){
    char temp[16];
    if(find_in_config("currentCars=", temp, sizeof(temp))){
        currentCars = atoi(temp);
    } else{
        currentCars = 0;
    }
    if(currentCars <= 0) {
        printf("Current cars is already 0, cannot decrement.\n");
        return false; // exit if failed
    }
    currentCars--;
    char currentCarsStr[16];
    snprintf(currentCarsStr, sizeof(currentCarsStr), "%d", currentCars);
    if(!update_config("currentCars=", currentCarsStr)){
        printf("Failed to update current cars in config file.\n");
        return false; // exit if failed
    }
    return true;
}

int get_current_cars(){
    char temp[16];
    if(find_in_config("currentCars=", temp, sizeof(temp))){
        currentCars = atoi(temp);
    } else{
        currentCars = 0;
    }
    return currentCars;
}

bool add_Car_to_config(const char *carName){
    if (carName == NULL  || strlen(carName) == 0) {
        printf("Invalid car name\n");
        return false;
    }
    char msg[128] = {0};
    find_in_config("cars=", msg, sizeof(msg)); // get the current cars from the config
    int currentCars = get_current_cars();
    if (!currentCars){
        strcpy(msg, ""); // if no cars, start with empty string
        strcat(msg, carName); // add the new car name
    }
    else{
        strcat(msg, ","); // add a comma to the end
        strcat(msg, carName); // add the new car name
    }
    printf("Adding car to config: %s\n", msg);
    if(!update_config("cars=", msg)){
        printf("Failed to update config file with new car.\n");
        return false; // exit if failed
    }
    return true;
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

    // // // UI setup from waveshare: https://www.waveshare.com/wiki/Pico-OLED-1.3#Overview
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
    
    char temp[16];
    if(find_in_config("maxCars=", temp, sizeof(temp))){
        maxCars = atoi(temp);
    } else{
        maxCars = 0;
    }
    printf("Max cars: %d\n", maxCars);

    currentCars = get_current_cars();
    printf("Current cars: %d\n", currentCars);
    // when new cars are added need to move back + 1 // back option for the menu

    char carArray[maxCars][9]; // 0 to maxCars (includes BACK), 9 char (name + null)
    int foundCars = populate_car_array(carArray, currentCars ,maxCars);
    if(!foundCars){
        printf("Failed to populate car array\n");
        return -1; // exit if failed
    }
    if(currentCars != foundCars) {
        printf("Current car, does not match found cars, recommend resetting config file.\n");
        return -1;
    }

    
    load_screen(BlackImage, userName);
    printf("Entering menu screen...\n");
    int numOfScreens = 5; // number of screens in the menu
    char** screens = (char**)malloc(numOfScreens * sizeof(char*)); // allocate memory for the screens
    screens[0] = "QUICK       QUERY"; // spacing to use new line
    screens[1] = "SETTINGS";
    screens[2] = "ADD CAR";
    screens[3] = "VIEW CARS";
    screens[4] = "QUIT";
    while (1) {
        int choice = menu_Screen(BlackImage, numOfScreens, screens); // enter the menu screen loop
        switch (choice){
        case 0: // rt data
            printf("Entering real-time data screen...\n");
            realTimeDataScreen(BlackImage);
            break;
        case 1: // settings
            printf("Entering settings screen...\n");
            break;
        case 2: // add car
            // print_config();
            printf("Entering add car screen...\n");
            currentCars = get_current_cars();
            if(currentCars >= maxCars && maxCars > 0) {
                printf("Max cars reached, cannot add more.\n");
                continue; // skip to the next iteration
            }
            char *carName = name_Car(BlackImage); // enter the name car screen
            printf("Car name entered: %s\n", carName);
            increment_current_cars();

            if(!add_Car_to_config(carName)){
                printf("Failed to add car to config file.\n");
                free(carName); // free the car name memory
                continue; // skip to the next iteration
            }
            printf("Car added: %s\n", carName);
            free(carName);
            // print_config();
            break;
        case 3: // view entered cars
            printf("Entering view cars screen...\n");
            currentCars = get_current_cars();
            if(currentCars <= 0) {
                printf("No cars entered yet.\n");
                continue; // skip to the next iteration
            }
            populate_car_array(carArray, currentCars, maxCars);
            int selectedCar = car_Screen(BlackImage, currentCars + 1, carArray); // number of cars + 1 for BACK option
            // temporary reduction of current cars, curr any breakout will be a deleted car
            if(selectedCar != -1){ // not BACK
                print_config();
                printf("Selected car: %s\n", carArray[selectedCar]);
                decrement_current_cars();
                currentCars = get_current_cars();
                char temp[maxCars * 10];
                bool first = true;
                for(int i =0; i <= currentCars; i++){
                    if(i == selectedCar) continue; // skip the selected car
                    if(!first){
                        strcat(temp, ",");
                    }
                    strcat(temp, carArray[i]);
                    first = false;
                    printf("i = %d, temp = %s\n", i, temp);
                }
                printf("Updating config with cars: %s\n", temp);
                if(!update_config("cars=", temp)){
                    printf("Failed to update config file with cars.\n");
                    continue; // skip to the next iteration
                }
                print_config();
                // // todo:
                // // update the car array
                // // update the currentCars variable
                // // poss create a function to get the config data, so i dont have to rewrite the code in init
            }
            break;
        case 4: // quit
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
    for (int i = 0; i < numOfScreens; i++) {
        if (screens[i] != NULL) {
            free(screens[i]);
        }
    }
}
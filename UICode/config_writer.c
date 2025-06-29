#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>

bool write_to_config(const char * msg){
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    FIL fil;
    const char* filename = "config.txt";
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }
    if (f_printf(&fil, msg) < 0) {
        printf("f_printf failed\n");
    }

    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    f_unmount("");
}

bool find_in_config(const char* key, char* value, size_t value_size) {
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    FIL fil;
    const char* const filename = "config.txt";
    fr = f_open(&fil, filename, FA_READ);
    if (FR_OK != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    char buf[128];
    while (f_gets(buf, sizeof(buf), &fil)) { // get each line, written into buf
        if (strncmp(buf, key, strlen(key)) == 0) { // look for the key
            strncpy(value, buf + strlen(key), value_size - 1);  // copy the value part
            value[value_size - 1] = '\0'; // ensure null termination
            f_close(&fil);
            size_t len = strlen(value);
            while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) { // remove trailing newlines
                value[--len] = '\0';
            }
            return true;
        }
    }

    f_close(&fil);
    return false;
}

bool update_config(const char* key, const char* new_value) {
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    FIL fil;
    const char* filename = "config.txt";
    fr = f_open(&fil, filename, FA_READ);
    if (FR_OK != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    char lines[32][128]; // store up to 32 lines
    // formatter allows up to 16 cars so 32 lines should be enough
    int line_count = 0;
    char buf[128];
    bool found = false;

    // we look through the file looking for the key
    // if we find it, we replace the line with the new value
        // this is done by ignoring the value of the key
    // otherwise we just copy the line as is
    while (f_gets(buf, sizeof(buf), &fil) && line_count < 32) {
        if (strncmp(buf, key, strlen(key)) == 0) {
            snprintf(lines[line_count], sizeof(lines[line_count]), "%s%s\n", key, new_value);
            found = true;
            printf("Updated line: %s", lines[line_count]);
        }
        else{
            strncpy(lines[line_count], buf, sizeof(lines[line_count]));
            lines[line_count][sizeof(lines[line_count]) - 1] = '\0';
        }
        line_count++;
    }

    f_close(&fil);

    // now we write the lines back to the file
    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (FR_OK != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    for (int i = 0; i < line_count; i++) {
        f_puts(lines[i], &fil);
    }

    f_close(&fil);
    f_unmount("");

    // return true if we found the key and updated it
    // return false if we didn't find the key
    return found;
}

bool delete_from_config(const char* key) {
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    FIL fil;
    const char* filename = "config.txt";
    fr = f_open(&fil, filename, FA_READ);
    if (FR_OK != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    char lines[32][128]; // store up to 32 lines
    int line_count = 0;
    char buf[128];
    bool found = false;

    while (f_gets(buf, sizeof(buf), &fil) && line_count < 32) {
        if (strncmp(buf, key, strlen(key)) == 0) {
            found = true; // we found the key
            continue; // skip this line
        }
        strncpy(lines[line_count], buf, sizeof(lines[line_count]));
        lines[line_count][sizeof(lines[line_count]) - 1] = '\0';
        line_count++;
    }

    f_close(&fil);

    // now we write the lines back to the file
    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (FR_OK != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    for (int i = 0; i < line_count; i++) {
        f_puts(lines[i], &fil);
    }

    f_close(&fil);
    f_unmount("");

    return found; // return true if we found and deleted the key
}

void print_config(){
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    FIL fil;
    const char* filename = "config.txt";
    fr = f_open(&fil, filename, FA_READ);
    if (FR_OK != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }
    char buf[128];
    printf("Config file contents:\n");
    while (f_gets(buf, sizeof(buf), &fil)) { // get each line, written into buf
        printf("%s", buf); // print the line
    }
    f_close(&fil);
    f_unmount("");
    printf("End of config file contents.\n");
}
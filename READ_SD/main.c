#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"

int main(){
    stdio_init_all();

    while(1){
        printf("Hello, World!\n");
    }
    return 0;
}
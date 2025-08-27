#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <stdbool.h>

typedef int (*PIDForm) (int, int, float);

typedef struct PIDEntry {
    uint8_t pid;
    char *name;
    char *unit;
    PIDForm form;
    float divisor;
    bool supported;
} PIDEntry;

extern PIDEntry pid_Dir[];
extern const int dirSize;

#endif
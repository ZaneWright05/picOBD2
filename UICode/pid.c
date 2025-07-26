#include "pid.h"
#include <stddef.h>

int tempForm(int a, int b, float c) { return a - 40; }
int justA(int a, int b, float c) { return a; }
int tripleA(int a, int b, float c) { return 3 * a; }
int toPercentage(int a, int b, float c) { return (int) ((a * 100.0) / 255.0); }
int stdShift(int a, int b, float c) { return (a << 8) + b; }
int stdShiftDiv(int a, int b, float divisor) { return stdShift(a, b, 1) / divisor; }

const int dirSize = 28;

PIDEntry pid_Dir[] = {
    {0x04, "Engine Load", "%", toPercentage, 1.0f, false},
    {0x05, "Coolant Temp", "C", tempForm, 1.0f, false},
    {0x0A, "Fuel Pressure", "kPA", tripleA, 1.0f, false},
    {0x0B, "Int manif Press", "kPA", justA, 1.0f, false},
    {0x0C, "Eng Speed", "RPM", stdShiftDiv, 4.0f, false},
    {0x0D, "Veh Speed", "km/h", justA, 1.0f, false},
    {0x0F, "Int Temp", "C", tempForm, 1.0f, false},
    {0x10, "MAF Air Flow", "g/s", stdShiftDiv, 100.0f, false},
    {0x11, "Throttle Pos", "%", toPercentage, 1.0f, false},
    {0x1C, "OBD2 std", "", NULL, 1.0f, false},
    {0x1F, "R.T. since strt", "s", stdShift, 1.0f, false},
    {0x21, "Dist with MIL", "km", stdShift, 1.0f, false},
    {0x22, "Fuel Press", "kPA", stdShiftDiv, 12.658f, false},
    {0x23, "Fuel Press", "kPA", stdShiftDiv, 0.1f, false},
    {0x30, "Warm ups since clear", "times", justA, 1.0f, false},
    {0x31, "Dist since clear", "km", stdShift, 1.0f, false},
    {0x33, "Absolute baro press", "kPA", stdShift, 1.0f, false},
    {0x45, "Rel thrttle pos", "%", toPercentage, 1.0f, false},
    {0x46, "Ambient air temp", "C", tempForm, 1.0f, false},
    {0x47, "Absol thrrl pos B", "%", toPercentage, 1.0f, false},
    {0x48, "Absol thrrl pos C", "%", toPercentage, 1.0f, false},
    {0x49, "Absol thrrl pos D", "%", toPercentage, 1.0f, false},
    {0x4A, "Absol thrrl pos E", "%", toPercentage, 1.0f, false},
    {0x4B, "Absol thrrl pos F", "%", toPercentage, 1.0f, false},
    {0x4D, "Time w/ MIL on", "mins", stdShift, 1.0f, false},
    {0x4E, "Time since trbl clear", "min", stdShift, 1.0f, false},
    {0x51, "Fuel type", NULL, NULL, 1.0f, false},
    {0x5C, "Eng Oil temp", "C", tempForm, 1.0f, false}
};
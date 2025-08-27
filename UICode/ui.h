# include "pid.h"

void load_screen(UBYTE *BlackImage, char *userName);
int menu_Screen(UBYTE *BlackImage, int numScreens, char** screens);
char* name_File(UBYTE *BlackImage);
void realTimeDataScreen(UBYTE *BlackImage);
int car_Screen(UBYTE *BlackImage, int numScreens, char carArray[][9]);
PIDEntry* choosePIDs(UBYTE *BlackImage, PIDEntry *pid_Dir, int pidCount, int dirSize, int numToLog);
int chooseNumber(UBYTE *BlackImage);
int draw_Number(UBYTE *BlackImage, int number);
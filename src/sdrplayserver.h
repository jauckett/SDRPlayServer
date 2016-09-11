#ifndef __SDRPLAY
#define __SDRPLAY

#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))
int initSDRPlay();
void sighandler(int signum);
int initRequired(double freqCurrent, double freqNew);

double frequencyAllocations[7] = {12.0, 30.0, 60.0, 120.0, 250.0, 420.0, 1000.0};

char *mir_error_strings[] = {
  "Success",
  "Fail",
  "InvalidParam",
  "OutOfRange",
  "GainUpdateError",
  "RfUpdateError",
  "FsUpdateError",
  "HwError",
  "AliasingError",
  "AlreadyInitialised",
  "NotInitialised"};

#endif

#ifndef APP_H
#define APP_H

#define VERSION 1

enum TriageStatus {
    NB = 0,
    P1 = 1,
    P2 = 2,
    P3 = 3,

    FAULT = 0x80,
    UNUSED = 0x81,
    
    UNKNOWN = 0xFF,
};

extern int32_t whole_celsius;
extern uint8_t fraction_celsius;

#endif

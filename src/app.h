#ifndef APP_H
#define APP_H

#define VERSION 1

enum TriageStatus {
    P3 = 0,
    P2 = 1,
    P1 = 2,
    NB = 3,
    DEAD = 4,
    S1 = 5,
    S2 = 6,

    FAULT = 0x80,
    UNUSED = 0x81,
    UNKNOWN = 0xFF,
};

extern int32_t whole_celsius;
extern uint8_t fraction_celsius;

#endif

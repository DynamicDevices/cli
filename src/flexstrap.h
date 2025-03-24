#ifndef FLEXSTRAP_H_
#define FLEXSTRAP_H_

// Includes

#include <stdio.h>
#include <stdint.h>

// Defines

// Enumerations

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
typedef enum TriageStatus EnumTriageStatus;

// Functions
bool flexStrapInit(void);

#endif


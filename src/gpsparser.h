#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include "minmea.h"

typedef void (*RmcHandler)(bool valid, float latitude, float longitude, float speed);

void set_callback_rmc(RmcHandler handler);

typedef void (*GgaHandler)(float elevation);

void set_callback_gga(GgaHandler handler);

#endif
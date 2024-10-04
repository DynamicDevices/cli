#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include "minmea.h"

typedef void (*RmcHandler)(int fix_type, float latitude, float longitude, float altitude);

void set_callback_rmc(RmcHandler handler);

int gpsparser_getfixtype();
float gpsparser_getlatitude();
float gpsparser_getlongitude();
float gpsparser_getaltitude();
char gpsparser_getaltitudeunits();
float gpsparser_getheight();
char gpsparser_getheightunits();

#endif
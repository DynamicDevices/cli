#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include "minmea.h"

typedef void (*RmcHandler)(bool valid, float latitude, float longitude, float speed);

void set_callback_rmc(RmcHandler handler);

typedef void (*GgaHandler)(float elevation);

void set_callback_gga(GgaHandler handler);

int gpsparser_getfixtype();
float gpsparser_getlatitude();
float gpsparser_getlongitude();
float gpsparser_getaltitude();
char gpsparser_getaltitudeunits();
float gpsparser_getheight();
char gpsparser_getheightunits();

#endif
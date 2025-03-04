#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include "minmea.h"

typedef void (*RmcHandler)(int fix_type, float latitude, float longitude, float altitude);

void set_callback_rmc(RmcHandler handler);

int gpsparser_fixtype();
float gpsparser_latitude();
float gpsparser_longitude();
float gpsparser_altitude();
float gpsparser_rms_deviation();

#endif
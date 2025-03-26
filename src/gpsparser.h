#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include "minmea.h"

typedef void (*RmcHandler)(int fix_type, float latitude, float longitude, float altitude);

void set_callback_rmc(RmcHandler handler);

/* 
    Returns a copy of the last GNSS GGA sentence we received
*/
bool get_last_gnss_gga(const struct minmea_sentence_gga *ptr_minmea_sentence_gga);

/* 
    Returns a copy of the last GNSS GST sentence we received
*/
bool get_last_gnss_gst(const struct minmea_sentence_gst *ptr_minmea_sentence_gst);

/* 
    Returns a copy of the last GNSS RMC sentence we received
*/
bool get_last_gnss_rmc(const struct minmea_sentence_rmc *ptr_minmea_sentence_rmc);

#endif
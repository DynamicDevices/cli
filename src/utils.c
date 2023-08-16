#include "utils.h"


#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <openthread/platform/logging.h>
#include "openthread/instance.h"
#include "openthread/thread.h"

int8_t datahex(char* string, uint8_t *data, int8_t len) 
{
    if(string == NULL) 
       return -1;

    // Count colons
    int colons = 0;
    size_t index = 0;
    for (index = 0, colons=0; string[index] > 0; index++)
        if(string[index] == ':')
          colons++;

    size_t slength = strlen(string);

    if( ((slength-colons) % 2) != 0) // must be even
       return -1;

    if( (slength - colons)/2 > len)
      return -1;

    memset(data, 0, len);

    index = 0;
    size_t dindex = 0;
    while (index < slength) {
        char c = string[index];
        int value = 0;
        if(c >= '0' && c <= '9')
          value = (c - '0');
        else if (c >= 'A' && c <= 'F') 
          value = (10 + (c - 'A'));
        else if (c >= 'a' && c <= 'f')
          value = (10 + (c - 'a'));
        else if (c == ':') {
            index++;
            continue;
        }
        else {
          return -1;
        }

        data[(dindex/2)] += value << (((dindex + 1) % 2) * 4);

        index++;
        dindex++;
    }

    return 1+dindex;
}

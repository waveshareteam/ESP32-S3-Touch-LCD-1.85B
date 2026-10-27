#include "weather_body.h"

weather_body_result_t weather_body_read(const weather_body_reader_t *reader,char *buffer,size_t capacity,size_t *length)
{
    if(!reader||!reader->read||!reader->complete||!reader->expired||!buffer||!capacity||!length) return WEATHER_BODY_IO;
    *length=0;buffer[0]=0;
    for(;;){
        if(reader->expired(reader->context)) return WEATHER_BODY_TIMEOUT;
        size_t remaining=capacity-1-*length;
        char extra;
        /* Always drain read(), even when complete() is already true: ESP-IDF may have
         * parsed the entire body into its internal cache while fetching headers. */
        int n=reader->read(reader->context,remaining?buffer+*length:&extra,remaining?(remaining>1024?1024:remaining):1);
        if(n<0) return WEATHER_BODY_IO;
        if(n==0) return reader->complete(reader->context)?WEATHER_BODY_OK:WEATHER_BODY_INCOMPLETE;
        if((size_t)n>remaining) return WEATHER_BODY_TOO_LARGE;
        *length+=(size_t)n;buffer[*length]=0;
    }
}

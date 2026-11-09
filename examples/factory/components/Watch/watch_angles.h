#pragma once

/* Clockwise degrees from twelve o'clock. Fractional seconds also advance the
 * minute and hour hands, including across noon and calendar rollovers. */
typedef struct { double hour, minute, second; } watch_angles_t;
static inline watch_angles_t watch_angles(int hour,int minute,int second,long microsecond)
{
    const double seconds=second+microsecond/1000000.0;
    const double minutes=minute+seconds/60.0;
    const watch_angles_t angles={(hour%12+minutes/60.0)*30.0,minutes*6.0,seconds*6.0};
    return angles;
}

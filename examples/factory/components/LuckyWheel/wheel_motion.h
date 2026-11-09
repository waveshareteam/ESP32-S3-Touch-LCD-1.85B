/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 *
 * Spin target and cubic deceleration adapted from M5StopWatch-UserDemo.
 * Angles use tenths of a degree; sector zero starts below the top pointer.
 */
#pragma once
#include <stdint.h>

#define WHEEL_FULL_TURN 3600
#define WHEEL_MIN_CHOICES 2
#define WHEEL_MAX_CHOICES 18

static inline int32_t wheel_normalize(int32_t angle)
{
    int32_t result=angle%WHEEL_FULL_TURN;
    return result<0?result+WHEEL_FULL_TURN:result;
}

static inline int32_t wheel_sector_center(int slot,int count)
{
    return (slot*WHEEL_FULL_TURN+count/2)/count;
}

static inline int32_t wheel_spin_target(int32_t current,int count,int slot,
                                      int turns,int clockwise,int offset)
{
    int32_t target=wheel_normalize(-wheel_sector_center(slot,count)+offset);
    int32_t delta=clockwise?wheel_normalize(target-wheel_normalize(current)):
                            wheel_normalize(wheel_normalize(current)-target);
    return current+(clockwise?1:-1)*(turns*WHEEL_FULL_TURN+delta);
}

static inline int wheel_result(int32_t rotation,int count)
{
    return ((wheel_normalize(-rotation)*count+WHEEL_FULL_TURN/2)/WHEEL_FULL_TURN)%count;
}

static inline int32_t wheel_ease_out(int32_t start,int32_t target,
                                   uint32_t elapsed,uint32_t duration)
{
    if (!duration || elapsed>=duration) return target;
    int64_t remaining=duration-elapsed;
    int64_t denominator=(int64_t)duration*duration*duration;
    return target-(int64_t)(target-start)*remaining*remaining*remaining/denominator;
}

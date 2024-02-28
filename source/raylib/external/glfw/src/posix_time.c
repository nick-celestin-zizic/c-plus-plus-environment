//========================================================================
// GLFW 3.4 POSIX - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2017 Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
// It is fine to use C99 in this file because it will not be built with VS
//========================================================================

#include "internal.h"

#include <unistd.h>
#include <sys/time.h>
#define CLOCK_REALTIME			0
#define CLOCK_MONOTONIC			1
#define CLOCK_PROCESS_CPUTIME_ID	2
#define CLOCK_THREAD_CPUTIME_ID		3
#define CLOCK_MONOTONIC_RAW		4
#define CLOCK_REALTIME_COARSE		5
#define CLOCK_MONOTONIC_COARSE		6
#define CLOCK_BOOTTIME			7
#define CLOCK_REALTIME_ALARM		8
#define CLOCK_BOOTTIME_ALARM		9


//////////////////////////////////////////////////////////////////////////
//////                       GLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _glfwPlatformInitTimer(void)
{
    _glfw.timer.posix.clock = CLOCK_REALTIME;
    _glfw.timer.posix.frequency = 1000000000;

#if defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        _glfw.timer.posix.clock = CLOCK_MONOTONIC;
#endif
}

uint64_t _glfwPlatformGetTimerValue(void)
{
    struct timespec ts;
    clock_gettime(_glfw.timer.posix.clock, &ts);
    return (uint64_t) ts.tv_sec * _glfw.timer.posix.frequency + (uint64_t) ts.tv_nsec;
}

uint64_t _glfwPlatformGetTimerFrequency(void)
{
    return _glfw.timer.posix.frequency;
}


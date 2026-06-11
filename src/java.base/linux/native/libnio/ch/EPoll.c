/*
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

 #include <dlfcn.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/epoll.h>
 #include <time.h>

#include "jni.h"
#include "jni_util.h"
#include "jvm.h"
#include "jlong.h"
#include "nio.h"
#include "nio_util.h"

#include "sun_nio_ch_EPoll.h"

#include "ut_jcl_nio.h"

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPoll_eventSize(JNIEnv* env, jclass clazz)
{
    return sizeof(struct epoll_event);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPoll_eventsOffset(JNIEnv* env, jclass clazz)
{
    return offsetof(struct epoll_event, events);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPoll_dataOffset(JNIEnv* env, jclass clazz)
{
    return offsetof(struct epoll_event, data);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPoll_create(JNIEnv *env, jclass clazz) {
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    Trc_sun_nio_ch_EPoll_create(epfd);
    if (epfd < 0) {
        JNU_ThrowIOExceptionWithLastError(env, "epoll_create1 failed");
    }
    return epfd;
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPoll_ctl(JNIEnv *env, jclass clazz, jint epfd,
                          jint opcode, jint fd, jint events)
{
    struct epoll_event event;
    int res;

    event.events = events;
    event.data.fd = fd;

    res = epoll_ctl(epfd, (int)opcode, (int)fd, &event);
    return (res == 0) ? 0 : errno;
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_EPoll_wait(JNIEnv *env, jclass clazz, jint epfd,
                           jlong address, jint numfds, jint timeout)
{
    struct epoll_event *events = jlong_to_ptr(address);
    struct timespec starttime;
    uint32_t orgevents = events->events;
    clock_gettime(CLOCK_MONOTONIC, &starttime);
    int res = epoll_wait(epfd, events, numfds, timeout);
//    struct timespec sleept;
//    sleept.tv_sec = 0;
//    sleept.tv_nsec = 1000000 * 75;
//    nanosleep(&sleept, NULL);
//    errno = EINVAL;
//    timeout = 200;
    if (res < 0) {
       if (errno == EINTR) {
            return IOS_INTERRUPTED;
        } else {
            if (errno = EINVAL) {
				time_t elapsedsec = 0;
				long elapsedmillis = 0;
                jint timeoutmillis = -1;
                if (-1 != timeout) {
                    struct timespec endtime;
                    clock_gettime(CLOCK_MONOTONIC, &endtime);
                    elapsedsec = (endtime.tv_sec - starttime.tv_sec);
                    elapsedmillis = (endtime.tv_nsec - starttime.tv_nsec) / 1000000;
                    if (elapsedmillis < 0) {
                        elapsedmillis += 1000;
                        elapsedsec -= 1;
                    }
                    jint timeoutsec = (timeout / 1000) - elapsedsec;
                    timeoutmillis = (timeout - (timeoutsec * 1000)) - elapsedmillis;
                    if (timeoutsec > 0) {
                        timeoutmillis += timeoutsec * 1000;
                    }
                    if (timeoutmillis < 0) {
						timeoutmillis = 0;
					}
                }
                fprintf(stderr, "sun_nio_ch_EPoll_wait(fd=%d, timeout=%d) EINVAL orgevents %u events %u remaining timeout %d elapsed sec %ld millis %ld\n",
                    epfd, timeout, orgevents, events->events, timeoutmillis, elapsedsec, elapsedmillis);
                res = epoll_wait(epfd, events, numfds, timeoutmillis);
                fprintf(stderr, "sun_nio_ch_EPoll_wait(fd=%d, res %d, errno %d events %u)\n", epfd, res, errno, events->events);
                if (res < 0) {
                    if (errno == EINTR) {
                        return IOS_INTERRUPTED;
                    } else {
                        JNU_ThrowIOExceptionWithLastError(env, "epoll_wait failed");
                        return IOS_THROWN;
                    }
                } else {
                    return res;
                }
            }
            JNU_ThrowIOExceptionWithLastError(env, "epoll_wait failed");
            return IOS_THROWN;
        }
    }
    return res;
}

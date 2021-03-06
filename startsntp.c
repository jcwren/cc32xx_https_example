/*
 * Copyright (c) 2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <unistd.h>
#include <ti/net/sntp/sntp.h>
#include "uart_term.h"

//
//
//
#define TIME_BASEDIFF        ((((uint32_t)70 * 365 + 17) * 24 * 3600))
#define TIME_NTP_TO_LOCAL(t) ((t) - TIME_BASEDIFF)

#define NTP_SERVERS           1
#define NTP_SERVER_PORT     123
#define NTP_REPLY_WAIT_TIME   5
#define NTP_POLL_TIME        15

/*
 *  ======== startSNTP ========
 */
void startSNTP (void)
{
  uint64_t ntpTimeStamp;
  uint32_t currentTime;
  int32_t ret;
  time_t ts;
  SlNetSock_Timeval_t timeval;
  struct timespec tspec;

  /* Set timeout value for NTP server reply */
  timeval.tv_sec = NTP_REPLY_WAIT_TIME;
  timeval.tv_usec = 0;

  do {
    /* Get the time use the built in NTP server list: */
    if ((ret = SNTP_getTime (NULL, 0, &timeval, &ntpTimeStamp)))
    {
      UART_PRINT ("startSNTP: couldn't get time (%d), will retry in %d secs ...", ret, NTP_POLL_TIME);
      sleep (NTP_POLL_TIME);
      UART_PRINT ("startSNTP: retrying ...");
    }

    currentTime = ntpTimeStamp >> 32;
    currentTime = TIME_NTP_TO_LOCAL (currentTime);
  }
  while (ret < 0);

  tspec.tv_nsec = 0;
  tspec.tv_sec = currentTime;

  if (clock_settime (CLOCK_REALTIME, &tspec) != 0)
  {
    UART_PRINT ("startSNTP: Failed to set current time\n");

    while (1)
      ;
  }

  ts = time (NULL);

  UART_PRINT ("startSNTP: Current time: %s\n", ctime (&ts));
}

/* File: wsn-sniffer-cli.c
   Time-stamp: <2013-03-06 19:51:55 gawen>

   Copyright (C) 2013 David Hauweele <david@hauweele.net>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <err.h>

#include "mac.h"
#include "uart.h"
#include "pcap.h"
#include "dump.h"
#include "help.h"

#define PACKAGE "wsn-sniffer-cli"

#ifndef VERSION
# define VERSION "unk"
#endif /* VERSION */

#if !(defined COMMIT && defined PARTIAL_COMMIT)
# define PACKAGE_VERSION "v" VERSION
#else
# define PACKAGE_VERSION "v" VERSION " (commit: " PARTIAL_COMMIT ")"
#endif /* COMMIT */

enum event { EV_FRAME = 0xff,
             EV_INFO  = 0xfe };

static bool payload;
static unsigned int mac_info;
/* static unsigned int payload_info; */

static void event(const unsigned char *data, unsigned int size)
{
  struct mac_frame frame;

  switch(*data) {
  case(EV_FRAME):
    /* We except a raw frame so we don't need to renormalize anything. */
    if(mac_decode(&frame, data + 1, size - 1) < 0) {
#ifndef NDEBUG
      hex_dump(data + 1, size);
#endif /* NDEBUG */
      warnx("cannot decode frame");
    }

    /* Display the frame live. */
    mac_display(&frame, mac_info);

    /* Append the frame to the PCAP file. */
    append_frame(data + 1, size);

    /* For now we do not try decode payload.
       Instead we just dump the packet. */
    if(payload && frame.payload) {
      printf("Payload:\n");
      hex_dump(frame.payload, frame.size);
    }

    break;
  case(EV_INFO):
    write(STDOUT_FILENO, data + 1, size - 1);
    break;
  default:
    warnx("invalid event ignored");
#ifndef NDEBUG
    hex_dump(data, size);
#endif /* NDEBUG */
    break;
  }

  putchar('\n');
}

static speed_t baud(const char *arg)
{
  const struct {
    int     intval;
    speed_t baud;
  } *b, bauds[] = {
    { 230400, B230400 },
    { 115200, B115200 },
    { 57600, B57600 },
    { 38400, B38400 },
    { 19200, B19200 },
    { 9600, B9600 },
    { 4800, B4800 },
    { 2400, B2400 },
    { 1800, B1800 },
    { 1200, B1200 },
    { 300, B300 },
    { 200, B200 },
    { 150, B150 },
    { 134, B134 },
    { 110, B110 },
    { 75, B75 },
    { 50, B50 },
    { 0,  B0 }};

  int arg_val = atoi(arg);
  for(b = bauds; b->intval ; b++)
    if(b->intval == arg_val)
      return b->baud;

  errx(EXIT_FAILURE, "unrecognized speed");
}

static void cleanup(void)
{
  /* Ensure that the PCAP file is closed properly to flush buffers. */
  destroy_pcap();
}

int main(int argc, char *argv[])
{
  const char *name;
  const char *tty  = NULL;
  const char *pcap = NULL;
  speed_t speed = B0;

  int exit_status = EXIT_FAILURE;

  name = (const char *)strrchr(argv[0], '/');
  name = name ? (name + 1) : argv[0];

  struct opt_help helps[] = {
    { 'h', "help", "Show this help message" },
    { 'V', "version", "Print version information" },
#ifdef COMMIT
    { 'C', "commit", "Display commit information" },
#endif /* COMMIT */
    { 'b', "baud", "Specify the baud rate"},
    { 'p', "pcap", "Save packets in the specified PCAP file" },
    { 'c', "show-control", "Display frame control information" },
    { 's', "show-seqno", "Display sequence number" },
    { 'a', "show-addr", "Display addresses fields" },
    { 'S', "show-security", "Display security auxiliary field" },
    { 'M', "show-mac", "Display all informations about MAC frames" },
    { 'P', "show-payload", "Try to decode and display the payload" },
    { 'A', "show-all", "Display all informations" },
    { 0, NULL, NULL }
  };

  struct option opts[] = {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'V' },
#ifdef COMMIT
    { "commit", no_argument, NULL, 'C' },
#endif /* COMMIT */
    { "baud", required_argument, NULL, 'b' },
    { "pcap", required_argument, NULL, 'p' },
    { "show-control", no_argument, NULL, 'c' },
    { "show-seqno", no_argument, NULL, 's' },
    { "show-addr", no_argument, NULL, 'a' },
    { "show-security", no_argument, NULL, 'S' },
    { "show-mac", no_argument, NULL, 'M' },
    { "show-payload", no_argument, NULL, 'P' },
    { "show-all", no_argument, NULL, 'A' },
    { NULL, 0, NULL, 0 }
  };

  while(1) {
#ifndef COMMIT
    int c = getopt_long(argc, argv, "hVp:cb:saSMPA", opts, NULL);
#else
    int c = getopt_long(argc, argv, "hCVp:cb:saSMPA", opts, NULL);
#endif /* COMMIT */

    if(c == -1)
      break;

    switch(c) {
    case('p'):
      pcap = optarg;
      break;
    case('b'):
      speed = baud(optarg);
      break;
    case('c'):
      mac_info |= MI_CONTROL;
      break;
    case('s'):
      mac_info |= MI_SEQNO;
      break;
    case('a'):
      mac_info |= MI_ADDR;
      break;
    case('S'):
      mac_info |= MI_SECURITY;
      break;
    case('M'):
      mac_info = MI_ALL;
      break;
    case('P'):
      payload = true;
      break;
    case('A'):
      mac_info = MI_ALL;
      /* TODO: payload_info = PI_ALL */
      break;
    case('V'):
      printf(PACKAGE " " PACKAGE_VERSION "\n");
      exit_status = EXIT_SUCCESS;
      goto EXIT;
    case('h'):
      exit_status = EXIT_SUCCESS;
    default:
      help(name, "[OPTIONS] ... BAUD-RATE TTY", helps);
      goto EXIT;
    }
  }

  if((argc - optind) != 1)
    errx(EXIT_FAILURE, "except tty device");

  tty = argv[optind];

  if(!pcap && !mac_info /* && !payload_info */)
    warnx("doing nothing as requested");

  if(pcap)
    init_pcap(pcap);

  /* Register the cleanup function as the most
     common way to leave the event loop is SIGINT. */
  atexit(cleanup);

  start_uart(tty, speed, event);

EXIT:
  return exit_status;
}
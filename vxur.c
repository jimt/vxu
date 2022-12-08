/*
 * vxur - Yaesu/Vertex radio read
 *   a Linux implementation of the yard utility which writes data
 *   downloaded from a Yaesu radio in "clone" mode to a file
 * Jim Tittsler 7J1AJH/AI8A  7J1AJH@OnJapan.net
 *
 * The original yard/yarw Windows utilities were written by
 * Jose R. Camara, camaraya@quatacorp.com.
 * 
 * 
 * Copyright (c) 2002,2003 James W. Tittsler 7J1AJH/AI8A
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "vxu.h"
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

/* 9600 baud, 8 bits, 2 stop bits */
#define SERIALPARAMS	(B9600 | CS8 | CSTOPB | CRTSCTS | CLOCAL | CREAD )

#define TRANSFERSIZE	15
#define BUFFERSIZE	32000
#define	MAXBLOCKS	500
#define BLOCKINFOBUFSIZE (MAXBLOCKS*6)

#define FOOTER "<<<RAW CLONE DATA BEFORE THIS - FOLLOWS SETS OF BLOCK START POSITION (L,H), BLOCK LENGHT (L,H) AND CURRENT CHECKSUM (L,H)>>>"

#define TRUE	1
#define FALSE	0

void signal_handler_stop(int status);
int stop_flag = FALSE;
int complete = FALSE;
struct termios oldtio, newtio;
unsigned char buf[BUFFERSIZE], *p;
unsigned char block_info[BLOCKINFOBUFSIZE], *bi;
int fd;
int block_count;
int verbose = 0;
int hash = 0;
unsigned int csum;

void usage()
{
    printf("vxur -h -v -d {device} file\n");
}

/*
 * read and acknowledge a block of data, where a block is defined as
 * a group of bytes the radio sends before pausing
 */
int rx_block()
{
    int rx_bytes = 0;
    int res, oc;
    unsigned char ackbuf[1] = {0x06};
    unsigned char *block_start, *cp, *last_hash;
    unsigned int buf_offset;

    block_start = last_hash = p;

    while (!stop_flag) {
	res = read(fd, p, TRANSFERSIZE);
	/* once we start receiving data, shorten the timeout */
	if ((rx_bytes == 0) && (block_count == 0)) {
	    newtio.c_cc[VTIME] = 2;
	    tcsetattr(fd, TCSANOW, &newtio);
	}
	if (res > 0) {
	    p += res;
	    rx_bytes += res;
	    if (hash && (p - last_hash > 200)) {
		fputc('#', stdout);
		fflush(stdout);
		last_hash += 200;
	    }
	} else if (res == 0) {
	    if (rx_bytes > 0) {
		if(hash) {
		    fputc('.', stdout);
		    fflush(stdout);
		}
		oc = write(fd, ackbuf, 1);
		res = read(fd, ackbuf, 1);  /* read echo */
		for (cp = block_start; cp < p; ++cp) csum += *cp;
		buf_offset = block_start - buf;
		*bi++ = (buf_offset & 0xFF);
		*bi++ = buf_offset >> 8;
		*bi++ = (rx_bytes & 0xFF);
		*bi++ = rx_bytes >> 8;
		*bi++ = (csum & 0xFF);
		*bi++ = csum >> 8;
	    }
	    return rx_bytes;
	} else stop_flag = TRUE;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int byte_count;
    FILE *of;
    struct sigaction sigint;
    char devicename[80] = {"/dev/ttyS0"};
    char *filename;
    int c;
#ifdef _GETOPT_H
    static struct option long_options[] = {
	{"help", 0, 0, '?'},
	{"hash", 0, 0, 'h'},
	{"verbose", 0, 0, 'v'},
	{"device", 1, 0, 'd'}
    };
#endif

    while (1) {
#ifdef _GETOPT_H
	c = getopt_long(argc, argv, "?hvd:", long_options, 0);
#else
	c = getopt(argc, argv, "?hvd:");
#endif
	if (c == -1)
	    break;
	switch(c) {
	    case 'v':
		++verbose;
		break;
	    case 'h':
		++hash;
		break;
	    case 'd':
		strncpy(devicename, optarg, sizeof(devicename));
		break;
	    case '?':
		usage();
		exit(0);
		break;
	    case ':':
		fprintf(stderr, "missing arg\n");
		usage();
		exit(-1);
		break;
	}
    }

    if (verbose)
	printf("vxur - Vertex Standard (Yaesu) radio read %s\n",
                VERSIONSTRING);

    if (optind + 1 != argc) {
	fprintf(stderr, "Missing filename.\n");
	usage();
	exit(-1);
    }
    filename = argv[optind];

    if ((fd = open(devicename, O_RDWR | O_NOCTTY)) < 0) {
	perror(devicename);
	exit(-1);
    }

    if (tcgetattr(fd, &oldtio) == -1) {
	fprintf(stderr, "error getting tty attributes %s(%d)\n",
                strerror(errno), errno);
    }

    sigint.sa_handler = signal_handler_stop;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = 0;

    // sigint.sa_restorer = NULL;
    sigaction(SIGINT, &sigint, NULL);
    fcntl(fd, F_SETOWN, getpid());

#if 1
    cfsetospeed(&newtio, B9600);
    cfsetispeed(&newtio, B9600);
    cfmakeraw(&newtio);
    /* two stop bits, ignore handshake, make sure rx enabled */
    newtio.c_cflag |= (CSTOPB | CLOCAL | CREAD);
#else
    newtio.c_cflag = SERIALPARAMS;
    newtio.c_iflag = 0;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
#endif
    newtio.c_cc[VMIN] = 0;	/* minimum chars to accept */
    newtio.c_cc[VTIME] = 50;	/* interchar timer in 0.1S */
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    //if (tcsetattr(fd, TCSAFLUSH, &newtio) == -1) {
	    fprintf(stderr, "error setting tcsetattr, %s(%d)\n",
                    strerror(errno), errno);
    }

    p = buf;  bi = block_info;
    block_count = 0;
    csum = 0;
    while (!stop_flag && !complete) {
	byte_count = rx_block();
	if (byte_count == 0) complete = TRUE;
	else {
	    ++block_count;
	    if (block_count > MAXBLOCKS) {
		fprintf(stderr, "Too many blocks (>%d) received.", MAXBLOCKS);
		stop_flag = TRUE;
	    }
	}
    }

    tcsetattr(fd, TCSANOW, &oldtio);
    close(fd);
    
    if (hash)
	printf("\n");

    if (complete) {
	/* following the raw clone data is a constant footer */
	strcpy(p, FOOTER);
	p += strlen(FOOTER);

	/* following the footer is a table of block info
	 * for each block:
	 *   offset: low,high
	 *   length: low,high
	 *   checksum: low,high
	 */
	memcpy(p, block_info, block_count*6);
	p += block_count*6;

	of = fopen(filename, "wb");
	fwrite(buf, p-buf, 1, of);
	fclose(of);
    } else {
	exit(-1);
    }
    return 0;
}

void signal_handler_stop(int status)
{
    fprintf(stderr, "Cancelled by user.\n");
    stop_flag = TRUE;
}


/*
 * vxuw - Yaesu/Vertex radio write for unix
 *   a Linux implementation of the yarw utility which uploads data
 *   from a file to a Yaesu radio in "clone" mode
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

#define TRANSFERSIZE	16
#define BUFFERSIZE	32000
#define	MAXBLOCKS	500
#define BLOCKINFOBUFSIZE (MAXBLOCKS*6)

#define FOOTER "<<<RAW CLONE DATA BEFORE THIS - FOLLOWS SETS OF BLOCK START POSITION (L,H), BLOCK LENGHT (L,H) AND CURRENT CHECKSUM (L,H)>>>"

#define TRUE	1
#define FALSE	0

#define	ACK	0x06

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

void usage()
{
    printf("vxuw -h -v -d {device} file\n");
}

/*
* transmit a block of data to the radio
*/
int tx_block(int fd, unsigned char *buf, int len, int expect_ack)
{
    int tx_bytes, rx_bytes;
    int res, oc;
    unsigned char ackbuf[TRANSFERSIZE+1];
    unsigned char *p, *last_hash;

    last_hash = buf;
    while (len > 0 && !stop_flag) {
	tx_bytes = (len > TRANSFERSIZE) ? TRANSFERSIZE : len;

	oc = write(fd, buf, tx_bytes);
	
	/* read back the data echoed by the interface */
	rx_bytes = 0;
	p = ackbuf;
	while ((rx_bytes<tx_bytes) && !stop_flag) {
	    res = read(fd, p, tx_bytes-rx_bytes);
	    p += res;
	    rx_bytes += res;
	    if ((rx_bytes == 0) || (res == 0)) {
		fprintf(stderr, "missing echo from radio interface\n");
		stop_flag = TRUE;
		return 0;
	    }

	    // the radio can't take the data at 9600 baud
	    usleep(16000L);
	}
	buf += tx_bytes;
	len -= tx_bytes;
	if (hash && (buf - last_hash > 200)) {
	    fputc('#', stdout);
	    fflush(stdout);
	    last_hash += 200;
	}
    }

    if (expect_ack) {
	res = read(fd, ackbuf, 1);
	if ((res == 0) || (ackbuf[0] != ACK)) {
	    fprintf(stderr, "missing or bad ack from radio\n");
	}
    }

    if (hash) {
	fputc('.', stdout);
	fflush(stdout);
    }

    return 1;
}

int main(int argc, char *argv[])
{
int byte_count;
size_t buflen;
int clonelen;
unsigned char *cp;
unsigned char csum;
FILE *of;
struct sigaction sigint;
char devicename[80] = {"/dev/ttyS0"};
char *filename;
int i,c;
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
	printf("vxuw - Vertex Standard (Yaesu) radio write (%s)\n",
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
    newtio.c_cflag |= (CSTOPB | CLOCAL | CREAD);  /* two stop bits, ignore handshake */
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
        fprintf(stderr, "error setting tty attributes, %s(%d)\n",
                strerror(errno), errno);
    }

    /* allow time for some self-powered interfaces to come up */
    sleep(2);

    if((of = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "Unable to open %s\n", filename);
        exit(-1);
    }
    buflen = fread(buf, 1, BUFFERSIZE, of);
    fclose(of);

    /* make sure it looks like a clone file by verifying that
     * the last block offset + last block length is the end of
     * the raw clone data
     */
    clonelen = buf[buflen-6] + buf[buflen-5]*256
	+ buf[buflen-4] + buf[buflen-3]*256;
    if((clonelen >= buflen) || (buf[clonelen] != '<')) {
	fprintf(stderr, "Invalid file format");
	stop_flag = TRUE;
    }

    /* look for the end of the FOOTER message */
    bi = strchr(&buf[clonelen], '>');
    if (bi == NULL) {
	fprintf(stderr, "Invalid file format");
	stop_flag = TRUE;
    }
    bi += 3;

    p = buf;
    block_count = 0;
    csum = 0;
    while ((bi-buf < buflen) && !stop_flag) {
	bi += 2;
	byte_count = *bi++;
	byte_count += *bi++ * 256;
	bi += 2;

        /* update checksum to include this block */
        cp = p;
        for(i=0; i<byte_count; ++i) csum += *cp++;

        /* if this is the last block, put checksum in last byte
	 * (of course, most radios will have already recorded it on upload
	 */
        if (p+byte_count == &buf[clonelen]) {
            /* the last byte of the last block shouldn't have been included */
            buf[clonelen-1] = csum - buf[clonelen-1];
        }

	tx_block(fd, p, byte_count, (p+byte_count<&buf[clonelen]));
        p += byte_count;
    }

    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &oldtio);
    close(fd);

    if (hash)
	printf("\n");

    if (stop_flag) {
	exit(-1);
    }
    return 0;
}

void signal_handler_stop(int status)
{
    fprintf(stderr, "Cancelled by user.\n");
    stop_flag = TRUE;
}


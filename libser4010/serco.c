#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "serco.h"

#define BAUDRATE B9600
#define TIMEOUT_SEC 25 

ssize_t _read_frame(int fd, uint8_t *buf, size_t max_len)
{
	bool comm_error;
	bool stuff_first;
	size_t len;
	ssize_t ret;
	uint8_t c;

	len = 0;
	comm_error = false;
	stuff_first = false;

	while (true) {
		ret = read(fd, &c, 1); 
		if (ret < 0) {
			return -1; // System Error
		} else if (ret == 0) {
			if (comm_error) {
				return -2; // Comm. Error
			} else {
				return -3; // Time-out
			}
		}

		// Remove Byte stuffing and detect end-of-record
		if (stuff_first) {
			stuff_first = false;

			if (c == STUFF_BYTE2) {
				break;
			} else if (c != STUFF_BYTE1) {
				comm_error = true;
				continue;
			}
		} else if (c == STUFF_BYTE1) {
			stuff_first = true;
			continue;
		}
		if (comm_error) {
			continue;
		}
		if (len >= max_len) {
			comm_error = true;
			continue;
		}

		buf[len] = c;
		len++;
	}
	
	if (comm_error) {
		return -2; // Comm. Error
	} else {
		return len;
	}
}

int serco_open(struct serco *dev, const char *path)
{
	int fd;
	struct termios newtio;

	srandom(time(NULL) | getpid());

	fd = open(path, O_RDWR | O_NOCTTY ); 
	if (fd == -1) {
		perror(path);
		return -1;
	}

	if (tcgetattr(fd, &(dev->oldtio)) != 0) {
		perror("tcgetattr() failed");
		goto bad;
	}

	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0; // set input mode (non-canonical, no echo,...)
	newtio.c_cc[VTIME] = TIMEOUT_SEC * 10;   // wait atmost 1-sec before returning
	newtio.c_cc[VMIN]  = 0;   // return immediatly when a byte is available

	if (tcflush(fd, TCIFLUSH) != 0) {
		perror("tcflush() failed");
		goto bad;
	}
	if (tcsetattr(fd, TCSANOW, &newtio) != 0) {
		perror("tcsetattr() failed");
		goto bad;
	}

	dev->fd = fd;
//TODO: send some NOP command to sync comm. Because Initial read sometimes fails.

	return 0;
bad:
	close(fd);
	return -1;
}

void serco_close(struct serco *dev)
{
	tcsetattr(dev->fd, TCSANOW, &(dev->oldtio));
	close(dev->fd);
}

int serco_send_command(struct serco *dev, uint8_t opcode,
			const void *payload, size_t payload_len,
			void *res_buf, size_t *res_len)
{
	int i;
	size_t wlen;
	uint8_t buf[1024];
	uint8_t frame_id;
	size_t buf_len;
	ssize_t rlen;
	uint8_t *payload_p = (uint8_t *) payload;

	assert(payload_len + 1 < 512);
	assert(opcode != STUFF_BYTE1);

	do {
		frame_id = random() & 0xff;
	} while (frame_id == STUFF_BYTE1);

	buf[CMD_ID] = frame_id;
	buf[CMD_OPCODE] = opcode;

	buf_len = CMD_PAYLOAD;
	for (i=0; i < payload_len; i++) {
		if (payload_p[i] == STUFF_BYTE1) {
			buf[buf_len++] = STUFF_BYTE1;
		}
		buf[buf_len++] = payload_p[i];
	}
	buf[buf_len++] = STUFF_BYTE1;
	buf[buf_len++] = STUFF_BYTE2;

	wlen = 0;
	while (wlen < buf_len) {
		ssize_t ret;
		ret = write(dev->fd, &buf[wlen], buf_len - wlen);
		if (ret < 0) {
			perror("write() failed");
			return -1;
		}
		wlen += ret;
	}

	rlen = _read_frame(dev->fd, buf, sizeof(buf));
	if (rlen < 0) {
		switch (rlen) {
		case -1:
			perror("read_frame() failed");
			break;
		case -2:
			fprintf(stderr, "read_frame() failed: Error in byte stuffing\n");
			break;
		case -3:
			fprintf(stderr, "read_frame() failed: Timeout\n");
			break;
		default:
			fprintf(stderr, "read_frame() failed: Unknown Error\n");
			break;
		}
		return -1;
	}
	if (rlen < 2) {
		fprintf(stderr, "Result frame too short\n");
		return -1;
	}
	if (buf[RES_ID] != frame_id) {
		fprintf(stderr, "Communication out-of-sync\n");
		return -1;
	}

	if (res_buf != NULL) {
		if (rlen - 2 < *res_len) {
			*res_len = rlen - 2;
		}
		memcpy(res_buf, &buf[RES_PAYLOAD], *res_len);
	}

	return buf[RES_STATUS];
}

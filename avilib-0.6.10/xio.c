/*
 *  libxio.c
 *
 *  Copyright (C) Lukas Hejtmanek - January 2004
 *
 *  This file is part of transcode, a linux video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "xio.h"

#include <fcntl.h>
#include <unistd.h>
#include <proto/dos.h>

struct instance {
	BPTR    fh;
	int     flags;
	int64_t file_pos;
};

#define MAX_INSTANCES 16
static struct instance instances[MAX_INSTANCES];

int xio_open(const void *pathname, int flags, ...) {
	int fd, i;
	int open_mode;

	fd = -1;
	for (i = 0; i < MAX_INSTANCES; i++) {
		if (instances[i].fh == ZERO) {
			fd = i;
			break;
		}
	}

	if (fd == -1)
		return -1;

	if (flags & O_CREAT)
		open_mode = MODE_NEWFILE;
	else
		open_mode = MODE_OLDFILE;

	instances[fd].fh = IDOS->FOpen(pathname, open_mode, 0);
	if (instances[fd].fh == ZERO)
		return -1;

	instances[fd].flags    = flags;
	instances[fd].file_pos = 0;

	return fd;
}

ssize_t xio_read(int fd, void *buf, size_t count) {
	ssize_t nbytes;

	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd].fh == ZERO)
		return -1;

	nbytes = IDOS->FRead(instances[fd].fh, buf, 1, count);
	if (nbytes == -1)
		return -1;

	instances[fd].file_pos += nbytes;
	return nbytes;
}

ssize_t xio_write(int fd, const void *buf, size_t count) {
	ssize_t nbytes;

	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd].fh == ZERO)
		return -1;

	nbytes = IDOS->FWrite(instances[fd].fh, buf, 1, count);
	if (nbytes == -1)
		return -1;

	instances[fd].file_pos += nbytes;
	return nbytes;
}

int xio_ftruncate(int fd, int64_t length) {
	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd].fh == ZERO)
		return -1;

	if (!IDOS->ChangeFileSize(instances[fd].fh, length, OFFSET_BEGINNING))
		return -1;

	if (instances[fd].file_pos > length)
		instances[fd].file_pos = length;

	return 0;
}

static int extend_file(BPTR fh, int64_t num_bytes) {
	char buffer[1024];
	size_t size;
	ssize_t nbytes;

	if (!IDOS->ChangeFilePosition(fh, 0, OFFSET_END))
		return -1;

	while (num_bytes > 0) {
		size = sizeof(buffer);
		if (size > num_bytes)
			size = num_bytes;

		nbytes = IDOS->Write(fh, buffer, size);
		if (nbytes <= 0)
			return -1;

		num_bytes -= nbytes;
	}

	return 0;
}

int64_t xio_lseek(int fd, int64_t offset, int whence) {
	int64_t current_pos, new_pos, file_size;

	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd].fh == ZERO)
		return -1;

	current_pos = instances[fd].file_pos;
	file_size = IDOS->GetFileSize(instances[fd].fh);

	switch (whence) {
		case SEEK_SET:
			new_pos = offset;
			break;
		case SEEK_CUR:
			new_pos = current_pos + offset;
			break;
		case SEEK_END:
			if (file_size == -1)
				return -1;
			new_pos = file_size + offset;
			break;
		default:
			return -1;
	}

	if (new_pos == current_pos)
		return new_pos;

	if (new_pos < 0)
		return -1;

	if ((instances[fd].flags & O_ACCMODE) != O_RDONLY && file_size != -1 && new_pos > file_size) {
		if (extend_file(instance[fd].fh, new_pos - file_size) < 0)
			return -1;

		instances[fd].file_pos = new_pos;
		return new_pos;
	}

	if (!IDOS->ChangeFilePosition(instances[fd].fh, new_pos, OFFSET_BEGINNING))
		return -1;

	instances[fd].file_pos = new_pos;
	return new_pos;
}

int xio_close(int fd) {
	BPTR fh;

	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd].fh == ZERO)
		return -1;

	fh = instances[fd].fh;

	instances[fd].fh       = ZERO;
	instances[fd].flags    = 0;
	instances[fd].file_pos = 0;

	if (!IDOS->FClose(fh))
		return -1;

	return 0;
}


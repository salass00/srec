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

#define MAX_INSTANCES 16
static BPTR instances[MAX_INSTANCES];

int xio_open(const void *pathname, int flags, ...) {
	int fd, i;
	int open_mode;

	fd = -1;
	for (i = 0; i < MAX_INSTANCES; i++) {
		if (instances[i] == ZERO) {
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

	instances[fd] = IDOS->FOpen(pathname, open_mode, 0);
	if (instances[fd] == ZERO)
		return -1;

	return fd;
}

ssize_t xio_read(int fd, void *buf, size_t count) {
	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd] == ZERO)
		return -1;

	return IDOS->FRead(instances[fd], buf, 1, count);
}

ssize_t xio_write(int fd, const void *buf, size_t count) {
	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd] == ZERO)
		return -1;

	return IDOS->FWrite(instances[fd], buf, 1, count);
}

int xio_ftruncate(int fd, int64_t length) {
	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd] == ZERO)
		return -1;

	if (!IDOS->ChangeFileSize(instances[fd], length, OFFSET_BEGINNING))
		return -1;

	return 0;
}

int64_t xio_lseek(int fd, int64_t offset, int whence) {
	int mode;

	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd] == ZERO)
		return -1;

	switch (whence) {
		case SEEK_SET:
			mode = OFFSET_BEGINNING;
			break;
		case SEEK_CUR:
			mode = OFFSET_CURRENT;
			break;
		case SEEK_END:
			mode = OFFSET_END;
			break;
		default:
			return -1;
	}

	if (!IDOS->ChangeFilePosition(instances[fd], offset, mode))
		return -1;

	return IDOS->GetFilePosition(instances[fd]);
}

int xio_close(int fd) {
	if (fd < 0 || fd >= MAX_INSTANCES || instances[fd] == ZERO)
		return -1;

	if (!IDOS->FClose(instances[fd]))
		return -1;

	return 0;
}


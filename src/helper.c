// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WLS_TX_ONLINE "/sys/class/power_supply/qcom-battmgr-wls-tx/online"
#define BATTERY_CAPACITY "/sys/class/power_supply/qcom-battmgr-bat/capacity"
#define MINIMUM_TX_CAPACITY 20

static int read_capacity(void)
{
	char buffer[16] = { 0 };
	char *end;
	long capacity;
	ssize_t length;
	int fd;

	fd = open(BATTERY_CAPACITY, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	length = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (length <= 0)
		return -1;

	errno = 0;
	capacity = strtol(buffer, &end, 10);
	if (errno || end == buffer || capacity < 0 || capacity > 100)
		return -1;

	return capacity;
}

int main(int argc, char **argv)
{
	const char *value;
	ssize_t written;
	int fd;

	if (argc != 2 || (strcmp(argv[1], "0") && strcmp(argv[1], "1"))) {
		fprintf(stderr, "usage: %s 0|1\n", argv[0]);
		return 2;
	}

	value = argv[1];
	if (!strcmp(value, "1")) {
		int capacity = read_capacity();

		if (capacity < 0) {
			fprintf(stderr, "cannot read battery capacity\n");
			return 1;
		}
		if (capacity < MINIMUM_TX_CAPACITY) {
			fprintf(stderr,
				"battery capacity must be at least %d%% (currently %d%%)\n",
				MINIMUM_TX_CAPACITY, capacity);
			return 1;
		}
	}

	fd = open(WLS_TX_ONLINE, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "cannot open %s: %s\n", WLS_TX_ONLINE,
			strerror(errno));
		return 1;
	}

	written = write(fd, value, 1);
	if (written != 1) {
		fprintf(stderr, "cannot write %s: %s\n", WLS_TX_ONLINE,
			written < 0 ? strerror(errno) : "short write");
		close(fd);
		return 1;
	}

	if (close(fd)) {
		fprintf(stderr, "cannot close %s: %s\n", WLS_TX_ONLINE,
			strerror(errno));
		return 1;
	}

	return 0;
}

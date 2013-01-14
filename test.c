#include <stdio.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "keycodes.h"

void pp_timeval(struct timeval *tv)
{
	double epoch = tv->tv_sec + tv->tv_usec * 1E-6;
	printf("%f", epoch);
}

char *events[EV_CNT] = {
	[0x00] = "EV_SYN",
	[0x01] = "EV_KEY",
	[0x02] = "EV_REL",
	[0x03] = "EV_ABS",
	[0x04] = "EV_MSC",
	[0x05] = "EV_SW",
	[0x11] = "EV_LED",
	[0x12] = "EV_SND",
	[0x14] = "EV_REP",
	[0x15] = "EV_FF",
	[0x16] = "EV_PWR",
	[0x17] = "EV_FF_STATUS",
	[0x1f] = "EV_MAX"
};

char *unknown_event_type = "-?-";

static
void pp_event(struct input_event *ev)
{
	char *event_type = events[ev->type];
	if (!event_type)
		event_type = unknown_event_type;

	pp_timeval(&(ev->time));
	printf(" %s: ", event_type);

	if (ev->type == EV_KEY) {
		char *name = keycode_names[ev->code];
		char static_name[20];
		if (!name) {
			snprintf(static_name, sizeof(static_name), "x%04x", ev->code);
			name = static_name;
		}
		printf("%s, 0x%08x\n", name, ev->value);
	} else {
		printf("0x%04x, 0x%08x\n", ev->code, ev->value);
	}
}


static
int read_all(int fd, void *_buffer, size_t nbytes)
{
	char *buffer = _buffer;
	int total_readen = 0;
	while (nbytes) {
		int readen = read(fd, buffer, nbytes);
		/* fprintf(stderr, "Read something (%d)\n", readen); */
		if (!readen)
			break;
		if (readen < 0) {
			if (errno == EINTR)
				continue;
			return readen;
		}
		total_readen += readen;
		buffer += readen;
		nbytes -= readen;
	}
	return total_readen;
}

int main(int argc, char **argv)
{
	int fd = open(argv[1], O_RDONLY);
	struct input_event ev;
	int rv;

	fprintf(stderr, "opened device\n");

	alarm(10);

	rv = ioctl(fd, EVIOCGRAB, 1);
	fprintf(stderr, "grabbed device (%d)\n", rv);

	while ((read_all(fd, &ev, sizeof(ev)))) {
		pp_event(&ev);
	}

	return 0;
}

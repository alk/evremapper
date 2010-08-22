#include <stdio.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#include "keycodes.h"

int source_evdev_fd;
int uinput_fd;

static inline
int get_bit(void *_bits, int num)
{
	char *bits = _bits;
	return (bits[num / 8] >> (num % 8)) & 1;
}

static inline
void set_bit(void *_bits, int num, int value)
{
	char *bits = _bits;
	char mask = (1 << (num % 8));
	value = (value) ? mask : 0;
	char *ptr = bits + (num / 8);
	char byte = *ptr;
	byte &= ~mask;
	byte |= value;
	*ptr = byte;
}

static
void xioctl(int fd, int request, void *arg)
{
	int rv = ioctl(fd, request, arg);
	if (rv < 0) {
		char perror_string[1024];
		snprintf(perror_string, sizeof(perror_string), "ioctl(%d, %d, %p)", fd, request, arg);
		perror(perror_string);
		abort();
	}
}

static
void xioctl_int(int fd, int request, long arg)
{
	xioctl(fd, request, (void *)arg);
}

void create_uinput()
{
	uinput_fd = open("/dev/input/uinput", O_RDWR);
	if (uinput_fd < 0) {
		perror("open uinput");
		exit(1);
	}

	for (int i = 0; i < KEY_CNT; i++) {
		xioctl_int(uinput_fd, UI_SET_KEYBIT, i);
	}

	xioctl_int(uinput_fd, UI_SET_EVBIT, EV_SYN);
	xioctl_int(uinput_fd, UI_SET_EVBIT, EV_KEY);
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
void write_event(struct input_event *ev)
{
	write(uinput_fd, ev, sizeof(*ev));
	ev->type = EV_SYN;
	ev->code = 0;
	ev->value = 0;
	write(uinput_fd, ev, sizeof(*ev));
}

static
int fn_pressed = 0;

#define FN_KEY_CODE KEY_LEFTMETA
#define NEW_LEFTMETA KEY_LEFTALT
#define NEW_LEFTALT 0x56

int translate_event(struct input_event *ev)
{
	if (ev->type != EV_KEY)
		return 1;

	switch (ev->code) {
	case NEW_LEFTALT:
		ev->code = KEY_LEFTALT;
		break;
	case NEW_LEFTMETA:
		ev->code = KEY_LEFTMETA;
		break;
	case KEY_BRIGHTNESSDOWN:
	case KEY_BRIGHTNESSUP:
	case KEY_VOLUMEUP:
	case KEY_VOLUMEDOWN:
		return 0;
	case FN_KEY_CODE:
		if (ev->value == 2)
			return 0;
		fn_pressed = ev->value;
		return 0;
	}

	if (!fn_pressed)
		return 1;

	switch (ev->code) {
#define TR(a,b) case a: ev->code = b; break
		TR(KEY_BACKSPACE, KEY_DELETE);
		TR(KEY_ENTER, KEY_INSERT);
		TR(KEY_F1, KEY_BRIGHTNESSDOWN);
		TR(KEY_F2, KEY_BRIGHTNESSUP);
		TR(KEY_F3, KEY_SCALE);
		TR(KEY_F4, KEY_DASHBOARD);
		TR(KEY_F5, KEY_KBDILLUMDOWN);
		TR(KEY_F6, KEY_KBDILLUMUP);
		TR(KEY_F7, KEY_PREVIOUSSONG);
		TR(KEY_F8, KEY_PLAYPAUSE);
		TR(KEY_F9, KEY_NEXTSONG);
		TR(KEY_F10, KEY_MUTE);
		TR(KEY_F11, KEY_VOLUMEDOWN);
		TR(KEY_F12, KEY_VOLUMEUP);
		TR(KEY_UP, KEY_PAGEUP);
		TR(KEY_DOWN, KEY_PAGEDOWN);
		TR(KEY_LEFT, KEY_HOME);
		TR(KEY_RIGHT, KEY_END);
#undef TR
	}
	return 1;
}

static
void *events_eater(void *_arg)
{
	struct input_event ev;
	char *arg = _arg;
	int fd = open(arg, O_RDONLY);

	if (fd < 0) {
		perror("evdev open");
		exit(1);
	}

	if (ioctl(fd, EVIOCGRAB, 1) < 0) {
		perror("EVIOCGRAB");
		exit(1);
	}

	while (1) {
		if (read_all(fd, &ev, sizeof(ev)) <= 0)
			return 0;
	}
}

int main(int argc, char **argv)
{
	int verbose = 0;
	if (argc >= 2 && strcmp(argv[1], "-v") == 0) {
		argv++;
		argc--;
		verbose = 1;
	}
	source_evdev_fd = open(argv[1], O_RDONLY);
	if (source_evdev_fd < 0) {
		perror("evdev open");
		exit(1);
	}

	while (argc > 2) {
		pthread_t thread;
		pthread_create(&thread, 0, events_eater, argv[--argc]);
	}

	create_uinput();

	if (ioctl(source_evdev_fd, EVIOCGRAB, 1) < 0) {
		perror("EVIOCGRAB");
		exit(1);
	}

	struct uinput_user_dev devinfo = {
		.name = "repeater",
		.id = {
			.bustype = BUS_VIRTUAL,
			.vendor = 0xffff,
			.product = 0xffff,
			.version = 1
		}
	};

	write(uinput_fd, &devinfo, sizeof(devinfo));
	xioctl(uinput_fd, UI_DEV_CREATE, 0);

	struct input_event ev;
	while (1) {
		read_all(source_evdev_fd, &ev, sizeof(ev));
		if (ev.type == EV_SYN || ev.type == EV_MSC)
			continue;
		if (!translate_event(&ev))
			continue;
		if (verbose)
			pp_event(&ev);
		write_event(&ev);
	}
}

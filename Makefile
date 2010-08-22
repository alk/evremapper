
CFLAGS := -O2 -Wall -pedantic -std=gnu99 -ggdb3
LDFLAGS := -lpthread

all: test repeater

test: test.o keycodes.o

repeater: repeater.o keycodes.o

clean:
	rm -rf *.o test repeater

install:
	mkdir -p /opt/evrepeater
	cp -frl repeater run-pi-mapper.rb /opt/evrepeater
	cp -frl evremapper-init /etc/init.d/evremapper

.PHONY: all clean install

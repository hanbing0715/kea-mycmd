CC=g++
CFLAGS=-I/usr/include/kea -g -ggdb -O0 -Wall
LDFLAGS=-L/usr/lib -fpic --shared
TARGET=/lib/x86_64-linux-gnu/kea/hooks/

libdhcp_mycmd.so: mycmd.cc mycmd_log.cc mycmd_msgs.cc
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

mycmd_msgs.cc: mycmd_msgs.mes
	kea-msg-compiler $<

install: libdhcp_mycmd.so
	cp $< $(TARGET)$<

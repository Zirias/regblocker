CC := gcc
CFLAGS := -O3 -g0 -Wall -Werror -pedantic -std=c99
STRIP := strip

RMF := del /f /s

regblocker.exe: regblocker.c
	$(CC) $(CFLAGS) -o $@ $^
	$(STRIP) $@

clean:
	$(RMF) regblocker.exe

.PHONY: clean

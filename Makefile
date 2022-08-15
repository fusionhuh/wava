CC = gcc
PREFIX = /usr/local

CFLAGS = -std=c++17 -Wno-conversion-null -O3 -pthread `pkg-config --cflags libpulse-simple` `pkg-config --cflags libconfig++` `pkg-config --cflags fftw3`
LIBS = -lm -lstdc++ `pkg-config --libs libpulse-simple` `pkg-config --libs libconfig++` `pkg-config --libs fftw3`
INCLUDES = includes/
DEPS = $(INCLUDES)/*.h
OBJ = wava.o output/cli.o output/graphics.o input/pulse.o input/common.o transform/wavatransform.o

%.o: %.c $(DEPS)
	$(CC) -I$(INCLUDES) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(DEPS)
	$(CC) -I$(INCLUDES) $(CFLAGS) -c $< -o $@

wava: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

install: wava
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f wava ${DESTDIR}${PREFIX}/bin/
	chmod 755 ${DESTDIR}${PREFIX}/bin/wava

.PHONY: clean

uninstall:
	rm ${DESTDIR}${PREFIX}/bin/wava

clean:
	rm -f $(OBJ) wava

CC = gcc
PREFIX = /usr/local

CFLAGS = -std=c++17 -Wno-conversion-null -O3 -pthread `pkg-config --cflags libpulse-simple` `pkg-config --cflags libconfig++` `pkg-config --cflags fftw3`
LIBS = -lm -lstdc++ `pkg-config --libs libpulse-simple` `pkg-config --libs libconfig++` `pkg-config --libs fftw3`
INCLUDES = includes/
DEPS = $(INCLUDES)/*.hpp $(INCLUDES)/*.h
OBJ = wava.o output/cli.o output/graphics.o
SUBDIRS = libwava

.PHONY: clean subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS): 
	$(MAKE) -C $@

%.o: %.c $(DEPS)
	$(CC) -I$(INCLUDES) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(DEPS)
	$(CC) -I$(INCLUDES) $(CFLAGS) -c $< -o $@

wava: $(OBJ)
	$(CC) -I$(INCLUDES) $(CFLAGS) -o $@ libwava/libwava.so $^ $(LIBS)

install: wava
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f wava ${DESTDIR}${PREFIX}/bin/
	chmod 755 ${DESTDIR}${PREFIX}/bin/wava


uninstall:
	rm ${DESTDIR}${PREFIX}/bin/wava

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir -f Makefile clean; \
	done
	rm -f $(OBJ) wava


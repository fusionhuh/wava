CC = gcc
CFLAGS = -std=c++17 -Wno-conversion-null -O3 -pthread `pkg-config --cflags libpulse-simple` `pkg-config --cflags ncurses` `pkg-config --cflags libconfig++`
LIBS = -lm -lstdc++ -lfftw3 `pkg-config --libs libpulse-simple` `pkg-config --libs ncurses` `pkg-config --libs libconfig++`
INCLUDES = includes/
DEPS = $(INCLUDES)/*.h
OBJ = wava.o output/cli.o output/graphics.o input/pulse.o input/common.o transform/wavatransform.o

%.o: %.c $(DEPS)
	$(CC) -I$(INCLUDES) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(DEPS)
	$(CC) -I$(INCLUDES) $(CFLAGS) -c -o $@ $<

wava: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -f $(OBJ) wava

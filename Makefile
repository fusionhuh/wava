CC = gcc
CFLAGS = -std=c++14 -O2 -pthread `pkg-config --cflags libpulse-simple`
LIBS = -lm -lstdc++ -lfftw3 `pkg-config --libs libpulse-simple`
DEPS = output/cli.h output/graphics.h input/common.h input/pulse.h transform/wavatransform.h
OBJ = wava.o output/cli.o output/graphics.o input/pulse.o input/common.o transform/wavatransform.o

%.o: %.c $(DEPS)
	$(CC) -O2 -c -o $@ $<

%.o: %.cpp $(DEPS)
	$(CC) -O2 -c -o $@ $<

wava: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -f $(OBJ) wava

CC = gcc
CFLAGS = -std=c++14 -pthread `pkg-config --cflags libpulse-simple`
LIBS = -lm -lstdc++ -lfftw3 `pkg-config --libs libpulse-simple`
DEPS = graphics.h
OBJ = wava.o graphics.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $<
 
wava: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

.PHONY: clean

clean:
	rm -f $(OBJ) wava

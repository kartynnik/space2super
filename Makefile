CC = g++
CFLAGS = -W -Wall -std=c++11
LIBS = -lX11 -lXtst
PROG = space2super
DEBUG_PROG = $(PROG).debug
DEPS = libx11-dev libxtst-dev

SRC = main.cpp

all: options $(PROG)

options:
	@echo "$(PROG) build options:"
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"

deps:
	sudo apt-get install -y $(DEPS)

undeps:
	sudo apt-get remove -y $(DEPS)

run: $(PROG)
	./$(PROG)

debug: $(DEBUG_PROG)
	./$(DEBUG_PROG)

gdb: $(DEBUG_PROG)
	gdb -ex 'break main' -ex 'run' --args $(DEBUG_PROG)

$(PROG): $(SRC) Makefile
	$(CC) -O3 -DNDEBUG -o $@ $(SRC) $(CFLAGS) $(LIBS)

$(DEBUG_PROG): $(SRC) Makefile
	$(CC) -g -o $@ $(SRC) $(CFLAGS) $(LIBS)

clean:
	@echo "removing $(PROG) and $(DEBUG_PROG)"
	rm -f $(PROG) $(DEBUG_PROG)

.PHONY: all options clean

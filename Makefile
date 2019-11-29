CC = g++
CFLAGS = -W -Wall -std=c++11
LIBS = -lX11 -lXtst
DEPS = libxtst-dev

PROG = space2super
DEBUG_PROG = $(PROG).debug

SRC = $(PROG).cpp

# These are only example arguments used for debugging (`debug` and `run`),
# otherwise they are dynamically provided by `s2sctl`.
# Original `Super_L` key code, original Space key code, remapped Space key code, timeout in milliseconds.
DEFAULT_ARGS = 65 250 600

all: $(PROG)

options:
	@echo "$(PROG) build options:"
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"

deps:
	sudo apt-get install -y $(DEPS)

undeps:
	sudo apt-get remove -y $(DEPS)

run: $(PROG)
	./$(PROG) $(DEFAULT_ARGS)

debug: $(DEBUG_PROG)
	./$(DEBUG_PROG) $(DEFAULT_ARGS)

gdb: $(DEBUG_PROG)
	gdb -ex 'break main' -ex 'run' --args $(DEBUG_PROG) $(DEFAULT_ARGS)

$(PROG): $(SRC) Makefile
	$(CC) -O3 -DNDEBUG -o $@ $(SRC) $(CFLAGS) $(LIBS)

$(DEBUG_PROG): $(SRC) Makefile
	$(CC) -g -o $@ $(SRC) $(CFLAGS) $(LIBS)

clean:
	@echo "Removing $(PROG) and $(DEBUG_PROG)"
	rm -f $(PROG) $(DEBUG_PROG)

.PHONY: all clean debug deps gdb options run undeps

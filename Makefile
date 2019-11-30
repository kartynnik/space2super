CC = g++
CFLAGS = -W -Wall -std=c++11
OPT_FLAGS = -O3
LIBS = -lX11 -lXtst
DEPS = libxtst-dev

PROG = space2super
VERBOSE_PROG = space2super.verbose
DEBUG_PROG = $(PROG).debug

SRC = $(PROG).cpp

# These are only example arguments used for debugging (`debug` and `run`),
# otherwise they are dynamically provided by `s2sctl`.
# Please note that if the original Space key code is not remapped,
# the synthesized events of pressing the same key may cause an infinite loop.
# Original Space key code; timeout in milliseconds.
DEFAULT_ARGS = 65 600

all: $(PROG)

options:
	@echo "$(PROG) build options:"
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "OPT_FLAGS = $(OPT_FLAGS)"

deps:
	sudo apt-get install -y $(DEPS)

undeps:
	sudo apt-get remove -y $(DEPS)

run: $(PROG)
	./$(PROG) $(DEFAULT_ARGS)

verbose: $(PROG)
	./$(VERBOSE_PROG) $(DEFAULT_ARGS)

debug: $(DEBUG_PROG)
	./$(DEBUG_PROG) $(DEFAULT_ARGS)

gdb: $(DEBUG_PROG)
	gdb -ex 'break main' -ex 'run' --args $(DEBUG_PROG) $(DEFAULT_ARGS)

$(PROG): $(SRC) Makefile
	$(CC) $(OPT_FLAGS) -DNDEBUG -o $@ $(SRC) $(CFLAGS) $(LIBS)

$(VERBOSE_PROG): $(SRC) Makefile
	$(CC) -O3 -o $@ $(SRC) $(CFLAGS) $(LIBS)

$(DEBUG_PROG): $(SRC) Makefile
	$(CC) $(OPT_FLAGS) -g -o $@ $(SRC) $(CFLAGS) $(LIBS)

clean:
	@echo "Removing $(PROG), $(VERBOSE_PROG) and $(DEBUG_PROG)"
	rm -f $(PROG) $(VERBOSE_PROG) $(DEBUG_PROG)

.PHONY: all clean debug deps gdb options run undeps

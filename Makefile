CC       := clang
DIRBIN   := bin
DIRBUILD := build

CSRCS    := $(shell find src -name "*.c")
COBJS    := $(patsubst src/%.c,$(DIRBUILD)/%.o,$(CSRCS))

### Compilation options
CFLAGS   := -std=c89 -Wall -pedantic # -Wextra
CFLAGS   += -Wshadow -Wpointer-arith -Wvla -Wdeclaration-after-statement
CFLAGS   += -Wcast-align -Wstrict-prototypes -Wmissing-prototypes
CFLAGS   += -Wconversion -Wcast-qual


CFLAGS   += -Iinclude
LDFLAGS  +=
LIB      += -lpthread

CFLAGS   += -D_DEFAULT_SOURCE -g3# -O2


.PHONY: worker

worker: $(COBJS)
	@mkdir -p $(DIRBIN)
	$(CC) $(LDFLAGS) -o $(DIRBIN)/$@ $^ $(LIB)

clean:
	rm -rf build

cleanall:
	rm -rf build bin


$(DIRBUILD)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

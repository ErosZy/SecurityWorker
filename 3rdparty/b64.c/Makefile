
CFLAGS = -std=c99 -Wall
TEST_CFLAGS = -Ideps
CUSTOM_MALLOC_FLAGS = -Db64_malloc=custom_malloc -Db64_realloc=custom_realloc

ifeq ($(USE_CUSTOM_MALLOC), true)
CFLAGS += $(CUSTOM_MALLOC_FLAGS)
endif

default: test
	./$<

test: CFLAGS+=$(TEST_CFLAGS)
test: test.o encode.o decode.o deps/ok/ok.o

clean:
	rm -f *.o test deps/ok/ok.o

.PHONY: default clean

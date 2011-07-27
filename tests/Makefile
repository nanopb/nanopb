CFLAGS=-ansi -pedantic -Wall -I .. -g -O0
DEPS=../pb_decode.c ../pb_decode.h ../pb.h

all: test_decode1

clean:
	rm -f test_decode1

test_decode1: test_decode1.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< ../pb_decode.c

fuzztest: test_decode1
	I=1; while cat /dev/urandom | ./test_decode1 > /dev/null; do I=$(($I+1)); echo -en "\r$I"; done
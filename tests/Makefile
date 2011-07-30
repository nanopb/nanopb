CFLAGS=-ansi -Wall -Werror -I .. -g -O0
DEPS=../pb_decode.c ../pb_decode.h ../pb.h
TESTS=test_decode1 decode_unittests

all: $(TESTS)

clean:
	rm -f test_decode1

%: %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< ../pb_decode.c

run_unittests: decode_unittests
	./decode_unittests

run_fuzztest: test_decode1
	bash -c 'I=1; while cat /dev/urandom | ./test_decode1 > /dev/null; do I=$$(($$I+1)); echo -en "\r$$I"; done'
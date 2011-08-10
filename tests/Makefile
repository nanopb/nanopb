CFLAGS=-ansi -Wall -Werror -I .. -g -O0
DEPS=../pb_decode.c ../pb_decode.h ../pb_encode.c ../pb_encode.h ../pb.h person.h unittests.h
TESTS=test_decode1 test_encode1 decode_unittests encode_unittests

all: $(TESTS) run_unittests

clean:
	rm -f $(TESTS)

%: %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< ../pb_decode.c ../pb_encode.c

run_unittests: decode_unittests encode_unittests
	./decode_unittests
	./encode_unittests

run_fuzztest: test_decode1
	bash -c 'I=1; while cat /dev/urandom | ./test_decode1 > /dev/null; do I=$$(($$I+1)); echo -en "\r$$I"; done'
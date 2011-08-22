CFLAGS=-ansi -Wall -Werror -I .. -g -O0 --coverage
LDFLAGS=--coverage
DEPS=../pb_decode.h ../pb_encode.h ../pb.h person.h unittests.h
TESTS=test_decode1 test_encode1 decode_unittests encode_unittests

all: $(TESTS) run_unittests breakpoints

clean:
	rm -f $(TESTS) *.o *.gcda *.gcno

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

test_decode1: test_decode1.o ../pb_decode.o person.o
test_encode1: test_encode1.o ../pb_encode.o person.o
decode_unittests: decode_unittests.o ../pb_decode.o person.o
encode_unittests: encode_unittests.o ../pb_encode.o person.o

person.c person.h: person.proto
	protoc -I. -I../generator -I/usr/include -operson.pb $<
	python ../generator/nanopb_generator.py person.pb

breakpoints: ../*.c *.c
	grep -n 'return false;' $^ | cut -d: -f-2 | xargs -n 1 echo b > $@

coverage:
	gcov -o .. ../pb_encode.c
	gcov -o .. ../pb_decode.c

run_unittests: decode_unittests encode_unittests test_encode1 test_decode1
	./decode_unittests > /dev/null
	./encode_unittests > /dev/null
	
	[ "`./test_encode1 | ./test_decode1`" = \
	"`./test_encode1 | protoc --decode=Person -I. -I../generator -I/usr/include person.proto`" ]

run_fuzztest: test_decode1
	bash -c 'I=1; while cat /dev/urandom | ./test_decode1 > /dev/null; do I=$$(($$I+1)); echo -en "\r$$I"; done'
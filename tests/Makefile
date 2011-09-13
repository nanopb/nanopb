CFLAGS=-ansi -Wall -Werror -I .. -g -O0 --coverage
LDFLAGS=--coverage
DEPS=../pb_decode.h ../pb_encode.h ../pb.h person.pb.h callbacks.pb.h unittests.h unittestproto.pb.h
TESTS=test_decode1 test_encode1 decode_unittests encode_unittests

all: breakpoints $(TESTS) run_unittests

clean:
	rm -f $(TESTS) person.pb* *.o *.gcda *.gcno

%.o: %.c
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

pb_encode.o: ../pb_encode.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<
pb_decode.o: ../pb_decode.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

test_decode1: test_decode1.o pb_decode.o person.pb.o
test_encode1: test_encode1.o pb_encode.o person.pb.o
test_decode_callbacks: test_decode_callbacks.o pb_decode.o callbacks.pb.o
test_encode_callbacks: test_encode_callbacks.o pb_encode.o callbacks.pb.o
decode_unittests: decode_unittests.o pb_decode.o unittestproto.pb.o
encode_unittests: encode_unittests.o pb_encode.o unittestproto.pb.o

%.pb: %.proto
	protoc -I. -I../generator -I/usr/include -o$@ $<

%.pb.c %.pb.h: %.pb ../generator/nanopb_generator.py
	python ../generator/nanopb_generator.py $<

breakpoints: ../*.c *.c
	grep -n 'return false;' $^ | cut -d: -f-2 | xargs -n 1 echo b > $@

coverage: run_unittests
	gcov pb_encode.gcda
	gcov pb_decode.gcda

run_unittests: decode_unittests encode_unittests test_encode1 test_decode1 test_encode_callbacks test_decode_callbacks
	rm -f *.gcda
	
	./decode_unittests > /dev/null
	./encode_unittests > /dev/null
	
	[ "`./test_encode1 | ./test_decode1`" = \
	"`./test_encode1 | protoc --decode=Person -I. -I../generator -I/usr/include person.proto`" ]
	
	[ "`./test_encode_callbacks | ./test_decode_callbacks`" = \
	"`./test_encode_callbacks | protoc --decode=TestMessage callbacks.proto`" ]

run_fuzztest: test_decode1
	bash -c 'I=1; while cat /dev/urandom | ./test_decode1 > /dev/null; do I=$$(($$I+1)); echo -en "\r$$I"; done'

CFLAGS=-Wall -Werror -I .. -g -O0
DEPS=double_conversion.c ../pb_decode.c ../pb_decode.h ../pb_encode.c ../pb_encode.h ../pb.h

all: run_tests

clean:
	rm -f test_conversions encode_double decode_double doubleproto.pb.c doubleproto.pb.h

test_conversions: test_conversions.c double_conversion.c
	$(CC) $(CFLAGS) -o $@ $^

%: %.c $(DEPS) doubleproto.pb.h doubleproto.pb.c
	$(CC) $(CFLAGS) -o $@ $< double_conversion.c ../pb_decode.c ../pb_encode.c doubleproto.pb.c

doubleproto.pb.c doubleproto.pb.h: doubleproto.proto ../generator/nanopb_generator.py
	protoc -I. -I../generator -I/usr/include -odoubleproto.pb $<
	python ../generator/nanopb_generator.py doubleproto.pb

run_tests: test_conversions encode_double decode_double
	./test_conversions
	./encode_double | ./decode_double
    

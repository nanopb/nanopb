CFLAGS=-ansi -Wall -Werror -I .. -g -O0
DEPS=../pb_decode.c ../pb_decode.h ../pb_encode.c ../pb_encode.h ../pb.h

all: encode decode
	./encode 1 | ./decode
	./encode 2 | ./decode
	./encode 3 | ./decode

clean:
	rm -f encode unionproto.pb.h unionproto.pb.c

%: %.c $(DEPS) unionproto.pb.h unionproto.pb.c
	$(CC) $(CFLAGS) -o $@ $< ../pb_decode.c ../pb_encode.c unionproto.pb.c

unionproto.pb.h unionproto.pb.c: unionproto.proto ../generator/nanopb_generator.py
	protoc -I. -I../generator -I/usr/include -ounionproto.pb $<
	python ../generator/nanopb_generator.py unionproto.pb

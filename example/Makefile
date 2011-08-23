CFLAGS=-ansi -Wall -Werror -I .. -g -O0
DEPS=../pb_decode.c ../pb_decode.h ../pb_encode.c ../pb_encode.h ../pb.h

all: server client

clean:
	rm -f server client fileproto.pb.c fileproto.pb.h

%: %.c $(DEPS) fileproto.pb.h fileproto.pb.c
	$(CC) $(CFLAGS) -o $@ $< ../pb_decode.c ../pb_encode.c fileproto.pb.c common.c

fileproto.pb.c fileproto.pb.h: fileproto.proto ../generator/nanopb_generator.py
	protoc -I. -I../generator -I/usr/include -ofileproto.pb $<
	python ../generator/nanopb_generator.py fileproto.pb

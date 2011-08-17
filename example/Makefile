CFLAGS=-ansi -Wall -Werror -I .. -g -O0
DEPS=../pb_decode.c ../pb_decode.h ../pb_encode.c ../pb_encode.h ../pb.h

all: server

%: %.c $(DEPS) fileproto.h
	$(CC) $(CFLAGS) -o $@ $< ../pb_decode.c ../pb_encode.c fileproto.c

fileproto.h: fileproto.proto ../generator/nanopb_generator.py
	protoc -I. -I../generator -I/usr/include -ofileproto.pb $<
	python ../generator/nanopb_generator.py fileproto.pb

CFLAGS = -ansi -Wall -Werror -g -O0

# Path to the nanopb root folder
NANOPB_DIR = ../..
DEPS = $(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_decode.h \
       $(NANOPB_DIR)/pb_encode.c $(NANOPB_DIR)/pb_encode.h $(NANOPB_DIR)/pb.h
CFLAGS += -I$(NANOPB_DIR)

all: server client

clean:
	rm -f server client fileproto.pb.c fileproto.pb.h

%: %.c $(DEPS) fileproto.pb.h fileproto.pb.c
	$(CC) $(CFLAGS) -o $@ $< $(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_encode.c fileproto.pb.c common.c

fileproto.pb.c fileproto.pb.h: fileproto.proto $(NANOPB_DIR)/generator/nanopb_generator.py
	protoc -ofileproto.pb $<
	python $(NANOPB_DIR)/generator/nanopb_generator.py fileproto.pb

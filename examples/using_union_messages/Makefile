CFLAGS = -ansi -Wall -Werror -g -O0

# Path to the nanopb root folder
NANOPB_DIR = ../..
DEPS = $(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_decode.h \
       $(NANOPB_DIR)/pb_encode.c $(NANOPB_DIR)/pb_encode.h $(NANOPB_DIR)/pb.h
CFLAGS += -I$(NANOPB_DIR)

all: encode decode
	./encode 1 | ./decode
	./encode 2 | ./decode
	./encode 3 | ./decode

clean:
	rm -f encode unionproto.pb.h unionproto.pb.c

%: %.c $(DEPS) unionproto.pb.h unionproto.pb.c
	$(CC) $(CFLAGS) -o $@ $< $(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_encode.c unionproto.pb.c

unionproto.pb.h unionproto.pb.c: unionproto.proto $(NANOPB_DIR)/generator/nanopb_generator.py
	protoc -ounionproto.pb $<
	python $(NANOPB_DIR)/generator/nanopb_generator.py unionproto.pb

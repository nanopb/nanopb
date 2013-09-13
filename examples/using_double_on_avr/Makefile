CFLAGS = -Wall -Werror -g -O0

# Path to the nanopb root directory
NANOPB_DIR = ../..
DEPS = double_conversion.c $(NANOPB_DIR)/pb.h \
	$(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_decode.h \
	$(NANOPB_DIR)/pb_encode.c $(NANOPB_DIR)/pb_encode.h
CFLAGS += -I$(NANOPB_DIR)

all: run_tests

clean:
	rm -f test_conversions encode_double decode_double doubleproto.pb.c doubleproto.pb.h

test_conversions: test_conversions.c double_conversion.c
	$(CC) $(CFLAGS) -o $@ $^

%: %.c $(DEPS) doubleproto.pb.h doubleproto.pb.c
	$(CC) $(CFLAGS) -o $@ $< double_conversion.c \
		$(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_encode.c doubleproto.pb.c

doubleproto.pb.c doubleproto.pb.h: doubleproto.proto $(NANOPB_DIR)/generator/nanopb_generator.py
	protoc -odoubleproto.pb $<
	python $(NANOPB_DIR)/generator/nanopb_generator.py doubleproto.pb

run_tests: test_conversions encode_double decode_double
	./test_conversions
	./encode_double | ./decode_double
    

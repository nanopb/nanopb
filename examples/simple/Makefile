# Compiler flags to enable all warnings & debug info
CFLAGS = -Wall -Werror -g -O0

# Path to the nanopb root folder
NANOPB_DIR = ../..
CFLAGS += -I$(NANOPB_DIR)

# C source code files that are required
CSRC  = simple.c                   # The main program
CSRC += simple.pb.c                # The compiled protocol definition
CSRC += $(NANOPB_DIR)/pb_encode.c  # The nanopb encoder
CSRC += $(NANOPB_DIR)/pb_decode.c  # The nanopb decoder

# Build rule for the main program
simple: $(CSRC)
	$(CC) $(CFLAGS) -osimple $(CSRC)

# Build rule for the protocol
simple.pb.c: simple.proto
	protoc -osimple.pb simple.proto
	python $(NANOPB_DIR)/generator/nanopb_generator.py simple.pb


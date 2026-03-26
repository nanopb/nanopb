# Include the nanopb provided Makefile rules
include ../../extra/nanopb.mk

# Compiler flags to enable all warnings & debug info
CFLAGS = -Wall -Werror -g -O0
CFLAGS += "-I$(NANOPB_INCLUDE)"

# C source code files that are required
CSRC  = simple.c                   # The main program
CSRC += simple.pb.c                # The compiled protocol definition
CSRC += $(NANOPB_CORE)             # The nanopb core .c files

# Build rule for the main program
simple: $(CSRC)
	$(CC) $(CFLAGS) -osimple $(CSRC)

# Build rule for the protocol
simple.pb.c: simple.proto
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out=. simple.proto


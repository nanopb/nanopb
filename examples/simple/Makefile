# Include the nanopb provided Makefile rules
include ../../extra/nanopb.mk

# Compiler flags to enable all warnings & debug info
CFLAGS = -Wall -Werror -g -O0
CFLAGS += -I$(NANOPB_DIR)

# C source code files that are required
CSRC  = simple.c                   # The main program
CSRC += simple.pb.c                # The compiled protocol definition
CSRC += $(NANOPB_DIR)/pb_encode.c  # The nanopb encoder
CSRC += $(NANOPB_DIR)/pb_decode.c  # The nanopb decoder

# Build rule for the main program
simple: $(CSRC)
	@echo
	@echo "*** Building the main binary..."
	$(CC) $(CFLAGS) -osimple $(CSRC)
	@echo
	@echo "*** Congrats! You can now run ./simple to see what it does!"
	@echo

# Build rule for the protocol
simple.pb.c: simple.proto
	@echo
	@echo "*** Building the protocol definition..."
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out=. simple.proto


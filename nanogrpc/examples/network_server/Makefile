# Include the nanopb provided Makefile rules
# Path to the nanopb root directory
NANOPB_DIR := $(abspath ../../../)

# Files for the nanopb core
NANOPB_CORE = $(NANOPB_DIR)/pb_encode.c $(NANOPB_DIR)/pb_decode.c $(NANOPB_DIR)/pb_common.c

# Check if we are running on Windows
ifdef windir
WINDOWS = 1
endif
ifdef WINDIR
WINDOWS = 1
endif

# Check whether to use binary version of nanopb_generator or the
# system-supplied python interpreter.
ifneq "$(wildcard $(NANOPB_DIR)/generator-bin)" ""
	# Binary package
	PROTOC = $(NANOPB_DIR)/generator-bin/protoc
	PROTOC_OPTS =
else
	# Source only or git checkout
	PROTOC = protoc
	ifdef WINDOWS
		PROTOC_OPTS = --plugin=protoc-gen-nanopb=$(NANOPB_DIR)/generator/protoc-gen-nanopb.bat
	else
		PROTOC_OPTS = --plugin=protoc-gen-nanopb=$(NANOPB_DIR)/generator/protoc-gen-nanopb
	endif
endif

BUILD_DIR = $(abspath .)

NANOGRPC_DIR := $(NANOPB_DIR)/nanogrpc
NANOGRPC_CORE = $(NANOGRPC_DIR)/ng_server.c
# Compiler flags to enable all warnings & debug info
CFLAGS = -ansi -Wall -Werror -g -O0
CFLAGS += -DPB_ENABLE_MALLOC -DPB_FIELD_16BIT
CFLAGS += -I$(NANOPB_DIR)
CFLAGS += -I$(NANOGRPC_DIR)
CFLAGS += -I$(abspath .)

# PROTOC_OPTS += -I$(abspath ../../)

PROTOC_INC += -I$(abspath ../..)
PROTOC_INC += -I.

PROTOC_OPTS += $(PROTOC_INC)
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.SECONDARY:

VPATH =  $(abspath ../..)
VPATH += $(abspath .)
vpath %.proto $(abspath ../../)
vpath %.options $(abspath ../../)

.PHONY: all
all: server client

% : %.c common.c fileproto.pb.c fileproto.ng.c nanogrpc.pb.c
	echo $@
	$(CC) $(CFLAGS) -o $@ $^ $(NANOPB_CORE) $(NANOGRPC_CORE)



# Rule for building .pb.c and .pb.h

%.ng.c %.ng.h: %.proto $(wildcard %.options)
	cd $(dir $(abspath $<)) && \
	$(PROTOC) --plugin=$(NANOPB_DIR)/generator/protoc-gen-grpc $(PROTOC_INC) --grpc_out=$(BUILD_DIR) $<

# Override rule for building .pb.c and .pb.h
%.pb.c %.pb.h: %.proto $(wildcard %.options)
	cd $(dir $(abspath $<)) && \
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out=$(BUILD_DIR) $<


.PHONY: clean
clean:
	rm -f server client *.pb.* *.ng.*

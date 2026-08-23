# This is an include file for Makefiles. It provides rules for building
# .pb.c and .pb.h files out of .proto, as well the path to nanopb core.

# Path to the nanopb root directory
NANOPB_DIR := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))))

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
	PROTOC_OPTS =
	ifdef WINDOWS
	    PROTOC = python $(NANOPB_DIR)/generator/protoc
	else
	    PROTOC = $(NANOPB_DIR)/generator/protoc
	endif
endif

# Options appended *after* --nanopb_out. Use this for generators that inject
# into the files nanopb produces, such as the nanopb-validate plugin: protoc
# runs generators in command line order and can only insert into a file an
# earlier generator already produced. For example:
#
#   PROTOC_OPTS += --nanopb_opt=--protoc-insertion-points
#   PROTOC_POST_OPTS += --nanopb-validate_out=.
#
PROTOC_POST_OPTS ?=

# Rule for building .pb.c and .pb.h
%.pb.c %.pb.h: %.proto %.options
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out=. $(PROTOC_POST_OPTS) $<

%.pb.c %.pb.h: %.proto
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out=. $(PROTOC_POST_OPTS) $<


NANOPB_PROTOC = $(or $(PROTOC),protoc)

PROTOBUF_INC ?= $(shell pkg-config --silence-errors --variable=includedir protobuf)
PROTOC_FLAGS += -I$(or $(PROTOBUF_INC),/usr/include)

all: options_pb2.py

%_pb2.py: %.proto
	$(NANOPB_PROTOC) $(PROTOC_FLAGS) --python_out=. -I. $<

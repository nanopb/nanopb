include ../extra/nanopb.mk

NANO_GRPC_PROTO_DIR = $(abspath .)
NANO_GRPC_OUT_DIR = $(abspath .)


# Build rule for the protocol
protos:
	cd $(NANO_GRPC_PROTO_DIR) &&  $(PROTOC) $(PROTOC_OPTS) --nanopb_out=$(NANO_GRPC_OUT_DIR) ./nanogrpc.proto

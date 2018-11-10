all: nanopb_pb2.py

%_pb2.py: %.proto
	protoc --python_out=. $<

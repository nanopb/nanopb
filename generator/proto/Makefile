all: nanopb_pb2.py plugin_pb2.py

nanopb_pb2.py: nanopb.proto
	protoc --python_out=. -I /usr/include -I . nanopb.proto

plugin_pb2.py: plugin.proto
	protoc --python_out=. -I /usr/include -I . plugin.proto

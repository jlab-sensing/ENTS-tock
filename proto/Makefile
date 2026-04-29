PROTOC = protoc
NANOPB = nanopb_generator

C_DIR = ../embedded/libents/proto

PYTHON_DIR = ../python/src/ents/proto

BUILD_DIR = build

PROTO_FILES = $(wildcard *.proto)

all: c python

c:
	mkdir -p build
	$(NANOPB) --output-dir=$(BUILD_DIR) -L"#include \"../nanopb/%s\"" $(PROTO_FILES)
	cp $(wildcard $(BUILD_DIR)/*.pb.c) $(C_DIR)
	cp $(wildcard $(BUILD_DIR)/*.pb.h) $(C_DIR)

python:
	$(PROTOC) --python_out=$(BUILD_DIR) $(PROTO_FILES)
	cp $(wildcard $(BUILD_DIR)/*_pb2.py) $(PYTHON_DIR)

clean:
	rm -r build

.PHONY: all clean c python

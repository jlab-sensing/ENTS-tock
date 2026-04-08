PROTOC = protoc
NANOPB = nanopb_generator

C_DIR = ../embedded/libents/protoc

PYTHON_DIR = ../python/src/ents/proto

BUILD_DIR = build

PROTO_FILES = $(wildcard *.proto)

all: c python

c:
	mkdir -p build
	$(NANOPB) --output-dir=$(BUILD_DIR) $(PROTO_FILES)
	cp $(BUILD_DIR)/* $(C_DIR)

python:
	$(PROTOC) --python_out=$(BUILD_DIR) $(PROTO_FILES)
	cp $(BUILD_DIR)/* $(PYTHON_DIR)

clean:
	rm -r build

.PHONY: all clean c python

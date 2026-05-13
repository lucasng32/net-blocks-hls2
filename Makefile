CXX      := g++
CXXFLAGS := -std=c++17 -I include/ -DNB_HLS_CODEGEN

BUILD_DIR := build
OUT_DIR   := output

.PHONY: all clean test

all: generate

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(BUILD_DIR)/gen_all: src/gen_all.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

generate: $(BUILD_DIR)/gen_all | $(OUT_DIR)
	./$(BUILD_DIR)/gen_all

test: all
	$(CXX) -std=c++17 -I include/ -I output/ -c tests/compile_test.cpp -o $(BUILD_DIR)/compile_test.o

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)

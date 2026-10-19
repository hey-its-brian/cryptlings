# Host build of the engine + test harness (no Arduino needed).
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude
BIN = build/test_host

$(BIN): src/cryptlings_core.cpp host_test/test_host.cpp include/cryptlings_core.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) src/cryptlings_core.cpp host_test/test_host.cpp -o $(BIN)

test: $(BIN)
	./$(BIN)

trace: $(BIN)
	./$(BIN) verbose

clean:
	rm -rf build

.PHONY: test trace clean

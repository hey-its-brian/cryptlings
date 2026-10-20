# Host builds: engine tests and the desktop simulator (no Arduino needed).
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude -Isrc/app
APP_SRC = src/cryptlings_core.cpp src/app/gfx.cpp src/app/fonts.cpp src/app/art.cpp \
          src/app/app.cpp src/app/app_draw.cpp src/app/app_draw_round.cpp src/app/app_game.cpp

build/test_host: src/cryptlings_core.cpp host_test/test_host.cpp include/cryptlings_core.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) src/cryptlings_core.cpp host_test/test_host.cpp -o $@

build/sim: $(APP_SRC) sim/sim_main.cpp $(wildcard src/app/*.h) src/assets/art.h src/assets/fonts.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(APP_SRC) sim/sim_main.cpp -o $@

# The round (StopWatch) layout: same app, -DCRYPT_ROUND=1, its own art and fonts.
build/sim_round: $(APP_SRC) sim/sim_main.cpp $(wildcard src/app/*.h) src/assets/art_round.h src/assets/fonts_round.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DCRYPT_ROUND=1 $(APP_SRC) sim/sim_main.cpp -o $@

test: build/test_host
	./build/test_host

trace: build/test_host
	./build/test_host verbose

# Screenshots of every screen into build/shots/ (PPM; convert with any image tool)
sim: build/sim
	./build/sim build/shots

sim_round: build/sim_round
	./build/sim_round build/shots_round

clean:
	rm -rf build

.PHONY: test trace sim sim_round clean

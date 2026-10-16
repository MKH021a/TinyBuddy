# Builds the native simulator (same core sources as the firmware).
#   make            -> build/tbsim
#   make test       -> build + run the automated QA suite
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wshadow -Wno-unused-parameter
CORE     := core/src
SRCS     := $(wildcard $(CORE)/*.cpp) $(wildcard sim/*.cpp)
HDRS     := $(wildcard $(CORE)/*.h) $(wildcard sim/*.h)

build/tbsim: $(SRCS) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -I$(CORE) -Isim $(SRCS) -o $@

test: build/tbsim
	./build/tbsim test

clean:
	rm -rf build

.PHONY: test clean

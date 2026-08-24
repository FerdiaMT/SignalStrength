CXX ?= c++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude
TARGET := bin/signalstrength
TEST_TARGET := bin/test_scoring
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
SOURCES := src/main.cpp src/collector_client.cpp src/cli.cpp src/network.cpp src/scoring.cpp src/dashboard.cpp src/storage.cpp src/macos_wifi.mm
LDFLAGS := -framework CoreWLAN -framework Foundation
else
SOURCES := src/main.cpp src/collector_client.cpp src/cli.cpp src/network.cpp src/scoring.cpp src/dashboard.cpp src/storage.cpp src/linux_wifi.cpp
LDFLAGS :=
endif

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

run: all
	./$(TARGET) --location "desk"

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): tests/test_scoring.cpp src/scoring.cpp include/app_types.h include/scoring.h include/wifi_info.h
	mkdir -p bin
	$(CXX) $(CXXFLAGS) tests/test_scoring.cpp src/scoring.cpp -o $(TEST_TARGET)

clean:
	rm -rf bin

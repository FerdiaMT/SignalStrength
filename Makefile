CXX ?= c++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude
TARGET := bin/signalstrength
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
SOURCES := src/main.cpp src/cli.cpp src/network.cpp src/scoring.cpp src/dashboard.cpp src/storage.cpp src/macos_wifi.mm
LDFLAGS := -framework CoreWLAN -framework Foundation
else
SOURCES := src/main.cpp src/cli.cpp src/network.cpp src/scoring.cpp src/dashboard.cpp src/storage.cpp src/linux_wifi.cpp
LDFLAGS :=
endif

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

run: all
	./$(TARGET) --location "desk"

clean:
	rm -rf bin

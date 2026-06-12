CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Isrc -Itests/mocks -I../src -I../tests/mocks
TARGET   = run_tests

SRCS = \
    run_tests.cpp \
    test_browse.cpp \
    test_preset_store.cpp \
    ../src/presets/browse_machine.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

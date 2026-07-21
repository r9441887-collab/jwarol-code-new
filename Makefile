CXX = g++
CXXFLAGS = -std=c++17 -O2 -Iinclude

all: jwarol2

jwarol2: src/jwarol_native_compiler.cpp src/jwarol_lexer.cpp src/jwarol_parser.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f jwarol2

.PHONY: all clean

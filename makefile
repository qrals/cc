target = build/program
lib = -lm
cc = g++
c_flags = \
-funsigned-char -Wall -Wextra -Wno-char-subscripts -std=c++14 -O3 # -g
obj := $(patsubst src/%.cpp, build/%.o, $(wildcard src/*.cpp))
hdr = $(wildcard src/*.hpp)

all: $(target)

$(obj) : build/%.o: src/%.cpp $(hdr)
	mkdir -p build/
	$(cc) -c $(c_flags) $< -o $@

.PRECIOUS : $(target) $(obj)

$(target) : $(obj)
	$(cc) -o $@ $(obj) -Wall $(lib)

clean :
	rm -rf build/

.PHONY : all clean

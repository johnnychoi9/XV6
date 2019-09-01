CXX=g++
CXXFLAGS=-std=c++14 -g -O3  -pthread -Wall -Werror -pedantic 
# ASAN = Address SANitizer, see https://github.com/google/sanitizers/wiki/AddressSanitizer
CXXFLAGS_ASAN=-std=c++14 -g -O3 -pthread -fsanitize=address -Wall -Werror -pedantic

all: life life-asan 

%-asan.o: %.cc
	$(CXX) -c $(CXXFLAGS_ASAN) -o $@ $<

life.o: life.cc life.h
life-asan.o: life.cc life.h

life-parallel.o: life-parallel.cc life.h
life-parallel-asan.o: life-parallel.cc life.h

life-serial.o: life-parallel.cc life.h
life-serial-asan.o: life-parallel.cc life.h

main.o: main.cc life.h timing.h
main-asan.o: main.cc life.h timing.h

timing.o: timing.cc timing.h
timing-asan.o: timing.cc timing.h

life: main.o life.o timing.o life-parallel.o life-serial.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

life-asan: main-asan.o life-asan.o timing-asan.o life-parallel-asan.o life-serial-asan.o
	$(CXX) $(CXXFLAGS_ASAN) -o $@ $^ -lpthread

clean:
	rm -f *.o

SUBMIT_FILENAME=life-submission-$(shell date +%Y%m%d%H%M%S).tar.gz

archive:
	tar -zcf $(SUBMIT_FILENAME) $(wildcard *.cc *.h *.hh *.H *.cpp *.C *.c *.txt *.md *.pdf) Makefile 
	@echo "Created $(SUBMIT_FILENAME); please upload and submit this file."

submit: archive

.PHONY: all clean archive submit

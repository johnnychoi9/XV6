CXX=g++
CXXFLAGS=-Wall -pedantic -std=c++14 -ggdb -O2 -fsanitize=address

all: pool-test libpool.a

libpool.a: pool.o
	ar rcs $@ $^

pool.o: pool.cc pool.h

pool-test.o: pool-test.cc pool.h

pool-test: pool-test.o libpool.a
	g++ $(CXXFLAGS) -o $@ $^ -lpthread

SUBMIT_FILENAME=pool-submission-$(shell date +%Y%m%d%H%M%S).tar.gz

submit:
	tar -zcf $(SUBMIT_FILENAME) $(wildcard *.cc *.h *.hh *.H *.cpp *.C *.c *.txt *.md *.pdf) Makefile 
	@echo "Created $(SUBMIT_FILENAME); please upload and submit this file."

skeleton: pool-base.h pool-base.cc
	rm -f pool-skeleton.tar.gz
	rm -rf for-skeleton
	mkdir -p for-skeleton/pool
	cp pool-base.h for-skeleton/pool/pool.h
	cp pool-base.cc for-skeleton/pool/pool.cc
	cp pool-test.cc Makefile for-skeleton/pool/
	cd for-skeleton && tar cvf ../pool-skeleton.tar.gz pool

clean:
	rm -f *.o

.PHONY: all clean submit skeleton

CXXFLAGS=-Wall -Werror -pedantic -std=c++11 -g -O2
CXX=g++

all: ftp-server test-partial-reads

ftp-server: ftp-server.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test-partial-reads: test-partial-reads.o
	$(CXX) $(CXXFLAGS) -o $@ $^

SUBMIT_FILENAME=ftp-submission-$(shell date +%Y%m%d%H%M%S).tar.gz

submit:
	tar -zcf $(SUBMIT_FILENAME) $(wildcard *.cc *.h *.hh *.H *.cpp *.C *.c *.txt *.md *.pdf) Makefile 
	@echo "Created $(SUBMIT_FILENAME); please upload and submit this file."

clean:
	rm -f *.o

.PHONY: all clean

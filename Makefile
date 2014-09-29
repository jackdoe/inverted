all:
	 g++ -g3 -Wall -Wno-c++11-extensions -o test test.cc
	 g++ -g3 -Wall -Wno-c++11-extensions -O0 -o bench bench.cc

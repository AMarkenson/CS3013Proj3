all: addem life


addem: addem.cpp
	g++ addem.cpp -o addem -lpthread

life: life.cpp
	g++ life.cpp -o life -lpthread

clean:
	rm -f addem life
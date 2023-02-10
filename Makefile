xbat: xbat.cpp
	g++ xbat.cpp -I/usr/include -L/usr/lib -lX11 -ludev -o xbat

.PHONY: run

run: xbat
	./xbat

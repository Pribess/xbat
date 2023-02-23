xbat: xbat.cpp
	g++ xbat.cpp -I/usr/include -L/usr/lib -lX11 -ludev -o xbat

.PHONY: run install uninstall

run: xbat
	./xbat

install: xbat
	cp xbat /usr/bin/xbat

uninstall:
	rm /usr/bin/xbat

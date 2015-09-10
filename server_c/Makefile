all: project

project: main.c arduino.o
	clang main.c arduino.o -o project  

arduino: arduino.c arduino.h
	clang -c arduino.c -lpthread

clean: 
	rm -rf *.o

clobber: clean
	rm -rf arduino_o
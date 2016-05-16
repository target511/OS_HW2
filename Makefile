all : hw2

hw2 : testcase.o hw2.o Disk.o
	gcc -o hw2 testcase.o hw2.o Disk.o

testcase.o : testcase.c hw2.h Disk.h
	gcc -c testcase.c
	
hw2.o :	hw2.c hw2.h Disk.h
	gcc -c hw2.c

Disk.o : Disk.c Disk.h
	gcc -c Disk.c

clean:
	rm -f testcase.o hw2.o Disk.o

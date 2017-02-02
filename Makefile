default:
	gcc -pthread -o lab1a lab1a.c
clean:
	$(RM) lab1a *.o *~
dist:
	tar -cvzf lab1a-404428077.tar.gz lab1a.c README Makefile

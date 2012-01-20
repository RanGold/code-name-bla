binFLAGS = -pedantic-errors -c -Wall -I ./Src/ -g -o ./
exeFLAGS = -ansi -pedantic-errors -Wall -Werror -g -lm -o 
allExe = tftp-server

all: clean $(allExe)

clean:
	-rm ./*.o $(allExe)

tftp-server: tftp-server.o
	gcc $(exeFLAGS)tftp-server ./tftp-server.o 

tftp-server.o:
	gcc $(binFLAGS)tftp-server.o ./tftp-server.c
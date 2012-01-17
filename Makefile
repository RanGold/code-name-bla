binFLAGS = -pedantic-errors -c -Wall -I ./Src/ -g -o ./
exeFLAGS = -ansi -pedantic-errors -Wall -Werror -g -lm -o 
allExe = mail_server mail_client

all: clean $(allExe)

clean:
	-rm ./*.o $(allExe)

mail_client: common.o protocol.o mail_client.o
	gcc $(exeFLAGS)mail_client ./common.o ./protocol.o ./mail_client.o

mail_server: common.o protocol.o mail_server.o
	gcc $(exeFLAGS)mail_server ./common.o ./protocol.o ./mail_server.o 

mail_client.o:
	gcc $(binFLAGS)mail_client.o ./mail_client.c

mail_server.o:
	gcc $(binFLAGS)mail_server.o ./mail_server.c

protocol.o:
	gcc $(binFLAGS)protocol.o ./protocol.c
	
common.o:
	gcc $(binFLAGS)common.o ./common.c
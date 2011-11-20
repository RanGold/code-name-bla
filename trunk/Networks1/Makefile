binFLAGS = -pedantic-errors -c -Wall -I ./Src/ -g -o ./Bin/
testBinFLAGS = -ansi -pedantic-errors -c -Wall -I ./Src/ -g -o ./Bin/Tests/
exeFLAGS = -ansi -pedantic-errors -Wall -Werror -g -lm -o
testExeFLAGS = -ansi -pedantic-errors -Wall -Werror -g -lm -o ./Misc/Tests/
allExe = mail_server mail_client

test: 

testclean: 
	-rm ./Misc/Tests/* ./Bin/Tests/* 

all: $(allExe)

clean:
	-rm ./Bin/*.o $(allExe)

mail_client: mail_client.o common.o
	gcc $(exeFLAGS)mail_client ./Bin/mail_client.o ./Bin/common.o

mail_server: mail_server.o common.o
	gcc $(exeFLAGS)mail_server ./Bin/mail_server.o ./Bin/common.o

mail_client.o:
	gcc $(binFLAGS)mail_client.o ./Src/mail_client.c

mail_server.o:
	gcc $(binFLAGS)mail_server.o ./Src/mail_server.c
	
common.o:
	gcc $(binFLAGS)common.o ./Src/common.c
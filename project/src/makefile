Server_end:Server_end.o
	g++ -o $@ $^  -lworkflow -lwfrest -lssl -lcrypto -lcrypt
Server_end.o:Server_end.cc
	g++ -c $^ -o $@ -I../include -DDEBUG -g -Wdeprecated-declarations
.PHONY:clean
clean:
	rm -rf Server_end Server_end.o
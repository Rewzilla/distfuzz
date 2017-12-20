
CC			:= mpicc 
CFLAGS		:= -g -lcrypto -lm `mysql_config --cflags --libs`
NODEFILE	:= nodes.lst

all: test main
	for n in `cat $(NODEFILE)`; do \
		ssh $$n rm -rf bin; \
		scp -r bin $$n:~/; \
	done

test:
	$(CC) $(CFLAGS) -o bin/test src/test.c

runtest:
	mpirun --hostfile $(NODEFILE) bin/test

main:
	$(CC) $(CFLAGS) -o bin/main \
		src/main.c src/base64.c src/functions.c \

clean:
	rm -rf bin/main
	rm -rf bin/test

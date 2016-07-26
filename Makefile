
CC = cc
CFLAGS = -g

# すべてのプログラムを作るルール
all: microdb all-test

# すべてのテストプログラムを作るルール
all-test: test-file test-datadef test-datamanip test-buffer

# すべてのテストプログラムを実行するルール
do-test: test-file
	./test-file
	./test-datadef
	./test-datamanip
	./test-buffer

# 「microdb」を作成するためのルールは、今後追加される予定
# とりあえず、今のところは「何もしない」という設定にしておく。
microdb: file.o datadef.o datamanip.o error.o main.o
	$(CC) -o microdb $(CFLAGS) file.o datadef.o datamanip.o error.o main.o

test-buffer: test-buffer.o file.o 
	$(CC) -o test-buffer $(CFLAGS) test-buffer.o file.o

test-datamanip: test-datamanip.o file.o datadef.o datamanip.o error.o
	$(CC) -o test-datamanip $(CFLAGS) test-datamanip.o file.o datadef.o datamanip.o error.o

test-datamanip2: test-datamanip2.o file.o datadef.o datamanip.o error.o
	$(CC) -o test-datamanip2 $(CFLAGS) test-datamanip2.o file.o datadef.o datamanip.o error.o

test-datadef: test-datadef.o file.o datadef.o datamanip.o error.o
	$(CC) -o test-datadef $(CFLAGS) test-datadef.o file.o datadef.o datamanip.o error.o

test-file: test-file.o file.o  
	$(CC) -o test-file $(CFLAGS) test-file.o file.o 

file.o: file.c microdb.h
	$(CC) -o file.o $(CFLAGS) -c file.c 

test-file.o: test-file.c microdb.h
	$(CC) -o test-file.o $(CFLAGS) -c test-file.c 

datadef.o: datadef.c microdb.h error.h 
	$(CC) -o datadef.o $(CFLAGS) -c datadef.c

test-datadef.o: test-datadef.c microdb.h error.h
	$(CC) -o test-datadef.o $(CFLAGS) -c test-datadef.c

test-datamanip.o: test-datamanip.c microdb.h error.h
	$(CC) -o test-datamanip.o $(CFLAGS) -c test-datamanip.c

test-datamanip2.o: test-datamanip2.c microdb.h error.h
	$(CC) -o test-datamanip2.o $(CFLAGS) -c test-datamanip2.c

error.o: error.c error.h
	$(CC) -o error.o $(CFLAGS) -c error.c

datamanip.o: datamanip.c error.h microdb.h 
	$(CC) -o datamanip.o $(CFLAGS) -c datamanip.c

main.o: main.c error.h microdb.h
	$(CC) -o main.o $(CFLAGS) -c main.c


clean:
	rm -f *.o

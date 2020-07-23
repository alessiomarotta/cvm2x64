.PHONY: cvm2x64 debug clean

CC = gcc
OUT = cvm2x64

cvm2x64: cvm2x64.c
	$(CC) -o $(OUT) $^

debug: cvm2x64.c
	$(CC) -DDEBUG -o $(OUT) $^

clean:
	rm -f $(OUT)
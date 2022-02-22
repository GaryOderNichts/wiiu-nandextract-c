.PHONY: all clean

all:
	gcc find_block.c -o find_block -Wno-format-truncation

clean:
	rm -f find_block

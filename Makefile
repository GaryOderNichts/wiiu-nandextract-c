.PHONY: all clean

all:
	gcc extractor.c aes.c -o extractor -Wno-format-truncation -O3

clean:
	rm -f extractor

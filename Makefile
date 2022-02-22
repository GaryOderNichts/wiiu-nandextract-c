.PHONY: all clean

all:
	gcc extractor.c aes.c -o extractor -Wno-format-truncation

clean:
	rm -f extractor

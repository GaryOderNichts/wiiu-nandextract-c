# wiiu-nandextract-c
Wii (U) NAND Extractor written in C

## Usage
`extractor-win.exe nand.bin` or for linux `./extractor-linux nand.bin`  
keys.bin or otp.bin needs to be in the same folder.

## Build
build with gcc  
To build for linux you need to replace the io.h with the linux headers and add a mode to mkdir

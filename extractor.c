/*  This file is part of Wii (U) NAND Extractor C.
 *  Copyright (C) 2020 GaryOderNichts
 *
 *  This file was ported from Wii NAND Extractor.
 *  Copyright (C) 2009 Ben Wilson
 *  
 *  Wii NAND Extractor is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Wii NAND Extractor is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include "aes.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define bswap32 __builtin_bswap32
#define bswap16 __builtin_bswap16
#endif

uint8_t* key;
FILE* rom;
int32_t loc_super;
int32_t loc_fat;
int32_t loc_fst;
FileType fileType = Invalid;
NandType nandType;

char* nandName;
int8_t initSuccess = 0;

char* stringReplaceAll(const char *search, const char *replace, char *string) 
{
	char* searchStart = strstr(string, search);
	while (searchStart != NULL)
	{
		char* tempString = (char*) malloc(strlen(string) * sizeof(char)+1);
		if(tempString == NULL) {
			return NULL;
		}

		strcpy(tempString, string);

		int len = searchStart - string;
		string[len] = '\0';

		strcat(string, replace);

		len += strlen(search);
		strcat(string, (char*)tempString+len);

		free(tempString);

		searchStart = strstr(string, search);
	}
	
	return string;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <nandfile> [output]\n", argv[0]);
		return 0;
	}
	
	//init nand

	rom = fopen(argv[1], "rb");
	if (rom == NULL)
	{
		printf("error opening %s\n", argv[1]);
		return -1;
	}

	if (!getFileType() || !getNandType() || !getKey())
		return -1;

	loc_super = findSuperblock();
	if (loc_super == -1)
	{
		printf("can't find superblock!\n");
		return 0;
	}
	
	int32_t fatlen = getClusterSize() * 4;
	loc_fat = loc_super;
	loc_fst = loc_fat + 0x0C + fatlen;

	nandName = calloc(PATH_MAX, sizeof(char));
	if (argc > 2)
	{
		strncpy(nandName, argv[2], PATH_MAX);
	}
	else
	{
		sscanf(argv[1], "%[^.]", nandName);
	}

	initSuccess = 1;
	extractNand();

	fclose(rom);
	free(key);
	free(nandName);

	return 0;
}

int32_t getPageSize(void)
{
	if (fileType == NoECC)
		return PAGE_SIZE;
	else
		return PAGE_SIZE + SPARE_SIZE;
}

int32_t getClusterSize(void)
{
	return getPageSize() * 8;
}

uint8_t getFileType(void)
{
	rewind(rom);
	fseek(rom, 0, SEEK_END);
	uint64_t length = ftell(rom);
	switch (length)
	{
	case PAGE_SIZE * 8 * CLUSTERS_COUNT:
		fileType = NoECC;
		return 1;
	case (PAGE_SIZE + SPARE_SIZE) * 8 * CLUSTERS_COUNT:
		fileType = ECC;
		return 1;
	case (PAGE_SIZE + SPARE_SIZE) * 8 * CLUSTERS_COUNT + 0x400:
		fileType = BootMii;
		return 1;
	default:
		return 0;
	}
}

uint8_t getNandType(void)
{
	rewind(rom);
	fseek(rom, getClusterSize() * 0x7FF0, SEEK_SET);
	uint32_t cluster;
	fread(&cluster, sizeof(uint32_t), 1, rom);
	switch (bswap32(cluster))
	{
	case 0x53464653: // SFFS
		nandType = Wii;
		return 1;
	case 0x53465321: // SFS! or a byteswapped !SFS
		if (fileType == BootMii) return 0; // Invalid dump type for WiiU
		nandType = WiiU;
		return 1;
	default:
		return 0;
	}
}

uint8_t getKey(void)
{
	//TODO key from text
	
	if (fileType == BootMii)
	{
		rewind(rom);
		fseek(rom, 0x21000158, SEEK_SET);
		uint8_t* bootmiikey = calloc(16, sizeof(uint8_t));
		fread(bootmiikey, sizeof(uint8_t), 16, rom);
		key = bootmiikey;
		return 1; 
	}
	else
	{
		key = readOTP("otp.bin");
		if (key != NULL)
			return 1;
		
		if (nandType == Wii)
		{
			key = readKeyfile("keys.bin");
			if (key != NULL)
				return 1;
		}
	}

	printf("Error key not valid\n");
	return 0;
}

uint8_t* readKeyfile(char* path)
{
	uint8_t* retval = malloc(sizeof(char) * 16);

	FILE* keyfile = fopen(path, "rb");
	if (keyfile == NULL)
	{
		free(retval);
		return NULL;
	}
	
	fseek(keyfile, 0x158, SEEK_SET);
	fread(retval, sizeof(uint8_t), 16, keyfile);
	fclose(keyfile);

	return retval;
}

uint8_t* readOTP(char* path)
{
	uint8_t* retval = malloc(sizeof(char) * 16);

	FILE* otpfile = fopen(path, "rb");
	if (otpfile == NULL)
	{
		free(retval);
		return NULL;
	}

	if (nandType == Wii)
		fseek(otpfile, 0x058, SEEK_SET);
	else
		fseek(otpfile, 0x170, SEEK_SET);

	fread(retval, sizeof(uint8_t), 16, otpfile);
	fclose(otpfile);

	return retval;
}

int32_t findSuperblock(void)
{
	uint32_t loc = ((nandType == Wii) ? 0x7F00 : 0x7C00) * getClusterSize();
	uint32_t end = CLUSTERS_COUNT * getClusterSize();
	uint32_t len = getClusterSize() * 0x10;
	uint32_t current, magic, last = 0;

	uint8_t irewind = 1;
	for (; loc < end; loc += len)
	{
		rewind(rom);
		fseek(rom, loc, SEEK_SET);
		fread(&magic, 4, 1, rom);
		if (bswap32(magic) != ((nandType == Wii) ? 0x53464653 /*SFFS*/ : 0x53465321 /*!SFS*/))
		{
			printf("this is not a supercluster\n");
			irewind++;
			continue;
		}

		fread(&current, 4, 1, rom);
		current = bswap32(current);

		if (current > last)
			last = current;
		else
		{
			irewind = 1;
			break;
		}

		if (loc == end)
			irewind = 1;
	}

	if (!last)
		return -1;

	loc -= len * irewind;

	return loc;
}

fst_t getFST(uint16_t entry)
{
	fst_t fst;

	// compensate for 64 bytes of ecc data every 64 fst entries
	int32_t n_fst = (fileType == NoECC) ? 0 : 2;
	int32_t loc_entry = (((entry / 0x40) * n_fst) + entry) * 0x20;

	rewind(rom);
	fseek(rom, loc_fst + loc_entry, SEEK_SET);

	fread(&fst.filename, sizeof(uint8_t), 0x0C, rom);
	fread(&fst.mode, sizeof(uint8_t), 1, rom);
	fread(&fst.attr, sizeof(uint8_t), 1, rom);

	uint16_t sub;
	fread(&sub, sizeof(uint16_t), 1, rom);
	sub = bswap16(sub);
	fst.sub = sub;

	uint16_t sib;
	fread(&sib, sizeof(uint16_t), 1, rom);
	sib = bswap16(sib);
	fst.sib = sib;

	uint32_t size;
	if ((entry + 1) % 64 == 0) //the entry for every 64th fst item is interrupted
	{
		fread(&size, 2, 1, rom);
		fseek(rom, 0x40, SEEK_CUR);
		fread((char*) (&size) + 2, 2, 1, rom);
	}
	else
	{
		fread(&size, sizeof(uint32_t), 1, rom);
	}
	
	size = bswap32(size);
	fst.size = size;

	uint32_t uid;
	fread(&uid, sizeof(uint32_t), 1, rom);
	uid = bswap32(uid);
	fst.uid = uid;

	uint16_t gid;
	fread(&gid, sizeof(uint16_t), 1, rom);
	gid = bswap16(gid);
	fst.gid = gid;

	uint32_t x3;
	fread(&x3, sizeof(uint32_t), 1, rom);
	x3 = bswap32(x3);
	fst.x3 = x3;

	fst.mode &= 1;

	return fst;
}

void extractNand(void)
{
	if (initSuccess != 1)
	{
		printf("NAND has not been initialized successfully!");
		return;
	}
	
	mkdir(nandName, 0777);
	extractFST(0, "");
}

uint8_t* getCluster(uint16_t cluster_entry)
{
	uint8_t* cluster = calloc(0x4000, sizeof(uint8_t));
	uint8_t* page = calloc(getPageSize(), sizeof(uint8_t));

	rewind(rom);
	fseek(rom, cluster_entry * getClusterSize(), SEEK_SET);

	for (int i = 0; i < 8; i++)
	{
		fread(page, sizeof(uint8_t), getPageSize(), rom);
		memcpy((uint8_t*) cluster + (i * 0x800), page, 0x800);
	}

	free(page);

	return aesDecrypt(key, cluster, 0x4000);
}

uint16_t getFAT(uint16_t fat_entry)
{
	/*
	* compensate for "off-16" storage at beginning of superblock
	* 53 46 46 53   XX XX XX XX   00 00 00 00
	* S  F  F  S     "version"     padding?
	*   1     2       3     4       5     6
	*/
	fat_entry += 6;

	// location in fat of cluster chain
	int32_t n_fat = (fileType == NoECC) ? 0 : 0x20;
	int32_t loc = loc_fat + ((((fat_entry / 0x400) * n_fat) + fat_entry) * 2);

	rewind(rom);
	fseek(rom, loc, SEEK_SET);

	uint16_t fat;
	fread(&fat, sizeof(uint16_t), 1, rom);
	return bswap16(fat);
}

void extractFST(uint16_t entry, char* parent)
{
	fst_t fst = getFST(entry);

	if (fst.sib != 0xffff)
		extractFST(fst.sib, parent);

	switch (fst.mode)
	{
	case 0:
		extractDir(fst, entry, parent);
		break;
	case 1:
		extractFile(fst, entry, parent);
		break;
	default:
		printf("ignoring unsupported mode: %x\n", fst.mode);
		break;
	}
}

void extractDir(fst_t fst, uint16_t entry, char* parent)
{
	char* newfilename = malloc(PATH_MAX);
	newfilename[0] = '\0';

	if (strcmp(fst.filename, "/") != 0)
	{
		if (strcmp(parent, "/") != 0 && strcmp(parent, "") != 0)
		{
			snprintf(newfilename, PATH_MAX, "%s/%s", parent, fst.filename);
		}
		else
		{
			strcpy(newfilename, fst.filename);
		}

		char path[PATH_MAX] = { 0 };
		snprintf(path, PATH_MAX, "%s/%s", nandName, newfilename);
		mkdir(path, 0777);

		printf("dir: %s\n", path);
	}
	else
	{
		strcpy(newfilename, fst.filename);
	}

	if (fst.sub != 0xffff)
		extractFST(fst.sub, newfilename); 

	free(newfilename);
}

void extractFile(fst_t fst, uint16_t entry, char* parent)
{
	uint16_t fat;
	uint32_t cluster_span = (uint32_t) (fst.size / 0x4000) + 1;
	uint8_t* data = calloc(cluster_span * 0x4000, sizeof(uint8_t));

	char filename[PATH_MAX] = { 0 };
	snprintf(filename, 13, "%s", fst.filename);
	stringReplaceAll(":", "-", filename);

	char newfilename[PATH_MAX] = { 0 };
	snprintf(newfilename, PATH_MAX, "/%s/%s", parent, filename);

	char path[PATH_MAX] = { 0 };
	snprintf(path, PATH_MAX, "%s%s", nandName, newfilename);

	FILE* bf = fopen(path, "wb");
	if(bf == NULL)
	{
		printf("Error opening %s: %d\n", path, errno);
	}

	fat = fst.sub;
	for (int i = 0; fat < 0xFFF0; i++)
	{
		//extracting...
		printf("extracting %s cluster %i\n", filename, i);
		uint8_t* cluster = getCluster(fat);
		memcpy((uint8_t*) (data + (i * 0x4000)), cluster, 0x4000);
		fat = getFAT(fat);
		free(cluster);
	}

	fwrite(data, fst.size, 1, bf);
	fclose(bf);

	free(data);

	printf("extracted file: %s\n", newfilename);
}

uint8_t* aesDecrypt(uint8_t* key, uint8_t* enc_data, size_t data_size)
{
	uint8_t iv[16] = { 0 };

	struct AES_ctx ctx;
	AES_init_ctx_iv(&ctx, key, iv);

	AES_CBC_decrypt_buffer(&ctx, enc_data, data_size);

	return enc_data;
}

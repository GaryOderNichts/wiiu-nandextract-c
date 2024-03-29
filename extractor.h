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

#ifndef _EXTRACTOR_H_
#define _EXTRACTOR_H_

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 0x800
#define SPARE_SIZE 0x40
#define CLUSTERS_COUNT 0x8000

typedef struct
{
	uint8_t filename[0x0C];
	uint8_t mode;
	uint8_t attr;
	uint16_t sub;
	uint16_t sib;
	uint32_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t x3;
} fst_t;

typedef enum
{
	Invalid,
	NoECC,
	ECC,
	BootMii
} FileType;

typedef enum
{
	Wii,
	WiiU
} NandType;

int32_t getPageSize(void);
int32_t getClusterSize(void);
uint8_t getFileType(void);
uint8_t getNandType(void);
uint8_t getKey(void);
uint8_t* readKeyfile(char* path);
uint8_t* readOTP(char* path);
int32_t findSuperblock(void);

uint8_t* getCluster(uint16_t cluster_entry);
uint16_t getFAT(uint16_t fat_entry);
fst_t getFST(uint16_t entry);
void extractNand(void);
void extractFST(uint16_t entry, char* parent);

void extractDir(fst_t fst, uint16_t entry, char* parent);
void extractFile(fst_t fst, uint16_t entry, char* parent);

uint8_t* aesDecrypt(uint8_t* key, uint8_t* enc_data, size_t data_size);
uint16_t bswap16(uint16_t value);
uint32_t bswap32(uint32_t value);

#endif
// 주의사항
// 1. blockmap.h에 정의되어 있는 상수 변수를 우선적으로 사용해야 함
// 2. blockmap.h에 정의되어 있지 않을 경우 본인이 이 파일에서 만들어서 사용하면 됨
// 3. 필요한 data structure가 필요하면 이 파일에서 정의해서 쓰기 바람(blockmap.h에 추가하면 안됨)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "blockmap.h"
// 필요한 경우 헤더 파일을 추가하시오.

FILE *flashfp;

typedef struct _mapping {
	int lbn;
	int pbn;
} mapping;

mapping table[BLOCKS_PER_DEVICE];

//
// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//
void ftl_open()
{
	//
	// address mapping table 초기화 또는 복구
	// free block's pbn 초기화
    // address mapping table에서 lbn 수는 DATABLKS_PER_DEVICE 동일


	// flashfp가 존재하지 않으면 새로 만들어준다.
	if(flashfp == NULL) {
		if((flashfp = fopen("flashmemory", "w+b")) == NULL) exit(-1);

		char* tempbuf = (char *)malloc(BLOCK_SIZE);
		memset(tempbuf, 0xFF, BLOCK_SIZE);

		for(int i = 0; i < BLOCKS_PER_DEVICE; i++) {
			fwrite(tempbuf, BLOCK_SIZE, 1, flashfp);
		}
	}

	// 일단 mapping table의 lbn을 0 ~ DATABLKS_PER_DEVICE - 1로 하고
	// pbn을 전부 -1로 초기화 한다.
	for(int i = 0; i < DATABLKS_PER_DEVICE; i++) {
		table[i].lbn = i;
		table[i].pbn = -1;
	}

	table[DATABLKS_PER_DEVICE].lbn = 15;
	table[DATABLKS_PER_DEVICE].pbn = 0;

	// pbn의 값을 가져온다.
	for(int i = 0; i < BLOCKS_PER_DEVICE; i++) {
		char tempbuf[PAGE_SIZE];
		dd_read(i * PAGES_PER_BLOCK, tempbuf);

		int lbn;
		memcpy(&lbn, tempbuf + SECTOR_SIZE, 4);

		if(lbn != -1) table[lbn].pbn = i;
	}
	return;
}

//
// 이 함수를 호출하는 쪽(file system)에서 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 함
// (즉, 이 함수에서 메모리를 할당 받으면 안됨)
//
void ftl_read(int lsn, char *sectorbuf)
{
	int offset = lsn % PAGES_PER_BLOCK;
	char tempbuff[PAGE_SIZE];
	dd_read(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK + offset, tempbuff);

	memcpy(sectorbuf, tempbuff, SECTOR_SIZE);

	return;
}

//
// 이 함수를 호출하는 쪽(file system)에서 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 함
// (즉, 이 함수에서 메모리를 할당 받으면 안됨)
//
void ftl_write(int lsn, char *sectorbuf)
{
	// lsn을 통한 offset 구하기
	int offset = lsn % PAGES_PER_BLOCK;
	
	// 만약에 table이 비어있을 경우 반대 값 넣기
	if(table[lsn / PAGES_PER_BLOCK].pbn == -1) {
		table[lsn / PAGES_PER_BLOCK].pbn = DATABLKS_PER_DEVICE - (lsn / PAGES_PER_BLOCK);
		
		char tempbuf[PAGE_SIZE];
		dd_read(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK, tempbuf);
		memcpy(tempbuf + SECTOR_SIZE, &table[lsn / PAGES_PER_BLOCK].lbn, 4);
		dd_write(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK, tempbuf);
	}

	// 입력받은 lsn에 값이 있는지 확인하기 위해서 값을 불러옴
	char readBuf[PAGE_SIZE];
	int readLsn;
	dd_read(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK + offset, readBuf);
	memcpy(&readLsn, readBuf + SECTOR_SIZE + sizeof(int), sizeof(int));

	// 만약에 사용하지 않은 블럭일 경우
	if(readLsn == -1) {
		memcpy(readBuf, sectorbuf, SECTOR_SIZE);
		memcpy(readBuf + SECTOR_SIZE + 4, &lsn, 4);

		dd_write(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK + offset, readBuf);
	} 
	// 만약에 입력한 값이 사용하던 블록에 들어간 경우
	else {

		char tempbuf[PAGES_PER_BLOCK][PAGE_SIZE];

		for(int i = 0; i < PAGES_PER_BLOCK; i++) {
			if(i == offset) {
				memcpy(tempbuf[i], sectorbuf, SECTOR_SIZE);
				memcpy(tempbuf[i] + SECTOR_SIZE, readBuf + SECTOR_SIZE, sizeof(int));
				memcpy(tempbuf[i] + SECTOR_SIZE + sizeof(int), &lsn, sizeof(int));
				
			} else {
				dd_read(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK + i, tempbuf[i]);
			}
		}

		dd_erase(table[lsn / PAGES_PER_BLOCK].pbn);

		// 현재 PBN과 Free Block의 PBN을 교체한다.
		int t = table[lsn / PAGES_PER_BLOCK].pbn;
		table[lsn / PAGES_PER_BLOCK].pbn = table[DATABLKS_PER_DEVICE].pbn;
		table[DATABLKS_PER_DEVICE].pbn = t;

		char* freeBlockLBN = (char *)malloc(PAGE_SIZE);
		memset(freeBlockLBN, 0xFF, PAGE_SIZE);
		memcpy(freeBlockLBN + SECTOR_SIZE, &table[DATABLKS_PER_DEVICE].lbn, sizeof(int));
		dd_write(table[DATABLKS_PER_DEVICE].pbn * PAGES_PER_BLOCK, freeBlockLBN);


		for(int i = 0; i < PAGES_PER_BLOCK; i++) {
			dd_write(table[lsn / PAGES_PER_BLOCK].pbn * PAGES_PER_BLOCK + i, tempbuf[i]);
		}
	}


	return;
}


void ftl_print()
{
	printf("LBN PBN\n");
	for(int i = 0; i < DATABLKS_PER_DEVICE; i++) {
		printf("%3d %3d\n", table[i].lbn, table[i].pbn);
	}
	printf("free block's pbn=%d\n", table[DATABLKS_PER_DEVICE].pbn);
	return;
}

void printAll() {
	fseek(flashfp, 0, SEEK_SET);

	for(int i = 0; i < BLOCKS_PER_DEVICE; i++) {
		printf("== %d번째 BLOCK ==\nLBN LSN  SEEK  DATA\n", i);
		for(int j = 0; j < PAGES_PER_BLOCK; j++) {
			char tempbuf[PAGE_SIZE];
			char sector[512];
			int lbn, lsn, seek;

			dd_read(i * PAGES_PER_BLOCK + j, tempbuf);
			seek = ftell(flashfp);
			memcpy(sector, tempbuf, 512);
			memcpy(&lbn, tempbuf + SECTOR_SIZE, 4);
			memcpy(&lsn, tempbuf + SECTOR_SIZE + 4, 4);

			//if(lbn > 100 || lbn < -100) lbn = 999;
			//if(lsn > 100 || lsn < -100) lsn = 999;

			if(sector[0] == -1) printf("%3d %3d %5d  string is empty CODE: ", lbn, lsn, seek);
			else printf("%3d %3d %5d  \033[1;32m%s\033[0m CODE: ", lbn, lsn, seek, sector);

			for(int i = 0; i < 50; i++) {
				printf("%d ", sector[i]);
			}

			if(lbn == 15) printf("  <<< HERE IS FREE BLOCK\n");
			else printf("\n");
		}
		printf("\n");
	}
}
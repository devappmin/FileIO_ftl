// 
// 과제3의 채점 프로그램은 기본적으로 아래와 같이 동작함
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "blockmap.h"

FILE *flashfp;

/****************  prototypes ****************/
void ftl_open();
void ftl_write(int lsn, char *sectorbuf);
void ftl_read(int lsn, char *sectorbuf);
void ftl_print();

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

//
// 이 함수는 file system의 역할을 수행한다고 생각하면 되고,
// file system이 flash memory로부터 512B씩 데이터를 저장하거나 데이터를 읽어 오기 위해서는
// 각자 구현한 FTL의 ftl_write()와 ftl_read()를 호출하면 됨
//
int main(int argc, char *argv[])
{
	char *blockbuf;
    char sectorbuf[SECTOR_SIZE];
	int lsn, i;

    flashfp = fopen("flashmemory", "w+b");
	if(flashfp == NULL)
	{
		printf("file open error\n");
		exit(1);
	}
	   
    //
    // flash memory의 모든 바이트를 '0xff'로 초기화한다.
    // 
    blockbuf = (char *)malloc(BLOCK_SIZE);
	memset(blockbuf, 0xFF, BLOCK_SIZE);

	for(i = 0; i < BLOCKS_PER_DEVICE; i++)
	{
		fwrite(blockbuf, BLOCK_SIZE, 1, flashfp);
	}

	free(blockbuf);

	ftl_open();    // ftl_read(), ftl_write() 호출하기 전에 이 함수를 반드시 호출해야 함

	char hello[512] = "Overwrite Check ";
	char* temp = (char *)malloc(sizeof(char) * SECTOR_SIZE);
	int a = 0;
	while(a != 100) {
		scanf("%d", &a);
		ftl_write(a, hello);
		ftl_read(a, temp);
		printAll();
		ftl_print();
		printf("%s %s\n", hello, temp);
	}


	//ftl_write(53, "Sup nigga");


	fclose(flashfp);

	return 0;
}
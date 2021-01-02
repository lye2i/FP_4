#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "sectormap.h"

// 필요한 경우 헤더 파일을 추가하시오.

//
// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//

int dd_read(int ppn,char *pagebuf);
int dd_write(int ppn,char *pagebuf);
int dd_erase(int pbn);

int address_mapping_table[DATAPAGES_PER_DEVICE]; //addres_mapping_table
int garbage_collection[DATAPAGES_PER_DEVICE];
int free_pbn; //free_pbn
int psn; //lsn에 할당 할 psn
int garbage; //garbage page의 갯수

void ftl_open()
{
	//
	// address mapping table 초기화
	// free block's pbn 초기화
    	// address mapping table에서 lbn 수는 DATABLKS_PER_DEVICE 동일
	
	for(int i=0; i<DATAPAGES_PER_DEVICE; i++){ //address_mappint_table, garbage_collection 초기화
		address_mapping_table[i] = -1;
		garbage_collection[i] = -1;}

	free_pbn = DATABLKS_PER_DEVICE; //처음 free block's pbn은 마지막 block으로 초기화
	psn = 0; //처음 할당되는 ppn
	garbage = 0; //처음에 garbage는 존재하지 않음

	return;
}

//
// 이 함수를 호출하기 전에 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 한다.
// 즉, 이 함수에서 메모리를 할당받으면 안된다.
//
void ftl_read(int lsn, char *sectorbuf)
{
	char *pagebuf;
	pagebuf = (char *)malloc(sizeof(char)*PAGE_SIZE); //pagebuf 메모리 할당

	int ppn = address_mapping_table[lsn]; //인자로 받은 lsn에 매핑되어 있는 ppn값 구하기

	if(ppn==-1){ //매핑되어있는 페이지가 없다면 에러
		fprintf(stderr, "매팅되어 있는 psn이 없어 읽을 수 없습니다.\n");}

	else if(lsn>=DATAPAGES_PER_DEVICE){ //입력받은 lsn이 address_mapping_table에 존재하지 않는다면 에러
		fprintf(stderr, "입력받은 lsn은 존재하지 않습니다.\n");}

	else{ //매핑되어있는 페이지가 있다면 해당 페이지 읽기
		if(dd_read(ppn, pagebuf)==-1){ //해당 페이지의 데이터 읽기 안되면 에러
			fprintf(stderr, "flash memory page read error\n");}

		memcpy(sectorbuf, pagebuf, SECTOR_SIZE); //flash memory의 sector데이터를 sectorbuf에 전달
	}
	
	return;
}

void binary_integer(int lsn, char *spare) //이진수 구하기
{
       	char binary[8];
	char result[8];
    	int position=0;
    	int j=0;

    	while(1){
		if(lsn%2==0) //이진수로 변환
			binary[position] = '0';
		else
			binary[position] = '1';
       		lsn = lsn / 2; //2로 나눈 몫을 저장
		position++; //자릿수 변경

        	if(lsn == 0) //몫이 0이 되면 반복을 끝냄
			break; }

    	for(j; j<8-position; j++)
	    	result[j] = '0';

    	for (int i = position - 1; i >= 0; i--){
        	result[j] = binary[i];
		j++; }
	memcpy(spare, result, strlen(result)); //이진수 sparebuf에 쓰기
}

void ftl_write(int lsn, char *sectorbuf)
{
	int ppn = address_mapping_table[lsn];
	char *sparebuf;
	sparebuf = (char *)malloc(sizeof(char)*SPARE_SIZE); //spare버퍼 메모리 할당
	memset((void *)sparebuf, (char)0xFF, SPARE_SIZE); //sparebuf 초기화
	char *pagebuf;
	pagebuf = (char *)malloc(sizeof(char)*PAGE_SIZE); //pagebuf 메모리 할당
	char *move_pagebuf; //garbage collection을 사용할때 다른 page들을 옮기기 위한 버퍼
	move_pagebuf = (char *)malloc(sizeof(char)*PAGE_SIZE); //move_pagebuf 메모리 할당

	if(lsn>=DATAPAGES_PER_DEVICE) //입력받은 lsn이 address_mapping_table에 존재하지 않는다면 에러
		fprintf(stderr, "입력받은 lsn은 존재하지 않습니다.\n");

	else if(ppn==-1 && psn<DATAPAGES_PER_DEVICE){ //lsn에 매핑된 psn이 없고 가용한 psn이 있다면
		binary_integer(lsn, sparebuf); //lsn를 이진수로 구해서 spare에 넣기
		memcpy(pagebuf, sectorbuf, SECTOR_SIZE); //인자로 받은 sector버퍼의 데이터 pagebuf에 저장하기
		sprintf(pagebuf, "%s%s", pagebuf,sparebuf); //pagebuf에 sparebuf도 저장
		if(dd_write(psn,pagebuf)==-1) //해당 페이지에 데이터 쓰기 안되면 에러
			fprintf(stderr, "flash memory page write error\n");
		address_mapping_table[lsn]=psn; //lsn에 매핑된 psn 저장
		psn++; //다음 매핑 psn
	}

	else if(ppn!=-1 && psn<DATAPAGES_PER_DEVICE){ //lsn에 매핑된 psn이 있고 가용한 psn이 있다면
		binary_integer(lsn, sparebuf); //lsn을 이진수로 구해서 spare에 넣기
		memcpy(pagebuf, sectorbuf, SECTOR_SIZE);//인자로 받은 sector버퍼의 데이터 pagebuf에 저장하기
		sprintf(pagebuf, "%s%s", pagebuf, sparebuf); //pagebuf에 sparebuf도 저장
		if(dd_write(psn, pagebuf)==-1) //파일의 해당 페이지에 데이터 쓰기 안되면 에러
			fprintf(stderr, "page write error\n");
		garbage_collection[garbage]=ppn; //update를 하므로 매핑되어있던 ppn은 garbage_page가 됨
		address_mapping_table[lsn]=psn; //lsn에 매핑된 psn저장
		psn++; //다음 매핑 psn
		garbage++; //garbage_page 갯수 증가
	}

	else if(psn==DATAPAGES_PER_DEVICE && garbage!=0){ //가용한 free_page가 없고 garbage_page가 있다면
		int garbage_page = garbage_collection[garbage-1]; //garbage_collection에 저장된 garbage_page구하기
		int garbage_block = garbage_page / PAGES_PER_BLOCK; //garbage page가 있는 블록 구하기
		int page_in_garbageblock = garbage_page % PAGES_PER_BLOCK; //그 블록 안에서 garbage_page의 위치 구하기
		
		for(int i=0; i<PAGES_PER_BLOCK; i++){ //garbage_page이외의 페이지의 데이터를 읽어서 free_block의 페이지에 복사
			if(i==page_in_garbageblock) //garbage_page라면 복사 안함
				continue;
			memset((void *)move_pagebuf, (char)0xFF, PAGE_SIZE);
			dd_read(garbage_block*PAGES_PER_BLOCK+i, move_pagebuf); //페이지의 데이터 읽어오기
			dd_write(free_pbn*PAGES_PER_BLOCK+i, move_pagebuf); //읽어온 데이터들 freeblock에 쓰기
			for(int j=0; j<DATAPAGES_PER_DEVICE; j++){ //기존에 매핑되어있던 페이지들도 이동한 페이지로 바꿈

				if(address_mapping_table[j]==garbage_block*PAGES_PER_BLOCK+i)
					address_mapping_table[j]=free_pbn*PAGES_PER_BLOCK+i;
			}
		}

		int new_psn = (free_pbn*PAGES_PER_BLOCK)+(page_in_garbageblock); //새로운 psn
		int new_ppn = address_mapping_table[lsn]; //매핑되어있던 ppn가져오기

		for(int i=0; i<garbage-1; i++){ //이동한 페이지들 중 다른 garbage_page가 있다면 옮겨진 페이지를 저장
			if(garbage_collection[i] / PAGES_PER_BLOCK == garbage_block) //같은 블록에 garbage 페이지가 존재했다는 의미이므로
				garbage_collection[i] = (free_pbn * PAGES_PER_BLOCK)+(garbage_collection[i]%PAGES_PER_BLOCK);
		}
		
		dd_erase(garbage_block); //다른 데이터들을 freeblock에 복사했으므로 garbage_block은 소거
		free_pbn = garbage_block; //이제 소거한 블록이 free_block이 된다.
		garbage--; //garbage_page를 하나 사용했으므로 갯수 감소

		binary_integer(lsn, sparebuf); //lsn를 이진수로 구해서 spare에 넣기
		memcpy(pagebuf, sectorbuf, SECTOR_SIZE); //인자로 받은 sector버퍼의 데이터 pagebuf에 저장하기
		sprintf(pagebuf, "%s%s", pagebuf, sparebuf); //pagebuf에 sparebuf도 저장

		if(dd_write(new_psn,pagebuf)==-1) //해당 페이지에 데이터 쓰기 안되면 에러
			fprintf(stderr, "flash memory page write error\n");
		address_mapping_table[lsn]=new_psn; //lsn에 매핑된 psn 저장

		if(ppn!=-1){ //만약 입력받은 lsn이 update 하는거라면
			garbage_collection[garbage] = new_ppn; //원래 매팅되어있던 ppn은 garbage page가 됨
			garbage++; //garbage_page갯수 증가
		}
	}

	else if(psn==DATAPAGES_PER_DEVICE && garbage==0) //가용한 페이지도 없고 garbage page도 없다면 더이상 쓰기 안됨
		fprintf(stderr,"가용한 free_page가 없어서 더 이상 쓸 수 없습니다.\n");
		
	return;
}

void ftl_print()
{
	printf("lpn ppn\n");
	for(int i=0; i<DATAPAGES_PER_DEVICE; i++) //address_mapping_table 출력
			printf("%d %d\n", i, address_mapping_table[i]);
	printf("free block's pbn=%d\n", free_pbn); //현재의 free_pbn출력
	return;
}

#include <stdio.h>
#include <string.h>

#include "hw2.h"
#include "Disk.h"

#define BUF_SIZE	1024

void PrintBufInfo();
void PrintDiskInfo(int start_blk, int end_blk);
void PrintDiskAccessCountInfo();//modified

int testcase1(){
    int  i;
    char pData[BUF_SIZE];

	DevResetDiskAccessCount();//modified
	// Write 8 blocks
	for( i = 0; i < 8; i++ ){

		sprintf(pData, "[block %d]", i);
		BufWrite(i, pData);
    }

    PrintBufInfo();

    // Write 1 blocks
    {
		sprintf(pData, "[block %d]", 8);
		BufWrite(8, pData);
    }

    // the Garbage collection has been done by BufDaemon

    // Write 6 blocks
    // the Garbage collection has been done by BufDaemon when i is 10, 12, 14, but after write buf
	for( i = 9; i < 15; i++ ){
		
		sprintf(pData, "[block %d]", i);
		BufWrite(i, pData);
    }

    PrintBufInfo();

	printf("========= BufSync() =========\n" );
	BufSync();

	PrintBufInfo();
    PrintDiskInfo(0, 14);

    PrintDiskAccessCountInfo(); //modified

    return 1;
}

int testcase2(){
    int  i;
	char pData[BUF_SIZE];

	DevResetDiskAccessCount();//modified
	// read 5 block
	for( i = 0; i < MAX_BUF_NUM/2; i++ ){

		BufRead(i, pData);
		printf("read[%d]:%s ", i, (char *)pData);
    }
	printf( "\n----------------------------------\n" );

	PrintBufInfo();

    // read 5 block
	for( i = MAX_BUF_NUM/2; i < MAX_BUF_NUM; i++ ){

		BufRead(i, pData);
		printf("read[%d]:%s ", i, (char *)pData);

    }
	printf( "\n----------------------------------\n" );

    PrintBufInfo();

    PrintDiskAccessCountInfo();//modified

    return 1;
}

int testcase3(){
    int  i;
    char pData[BUF_SIZE];

    DevResetDiskAccessCount();//modified
	// read & write 5 block
	for( i = 10; i < 10 + MAX_BUF_NUM/2; i++ ){

		BufRead(i,pData);
		printf("read[%d]:%s ", i, (char *)pData);

		sprintf(pData, "[block %d]", i + 100);
		BufWrite(i,pData);

    }
	printf( "\n----------------------------------\n" );

	printf("========= BufSync() =========\n" );
	BufSync();

    PrintBufInfo();
	PrintDiskInfo(0, 14);

    // Append 5 block
    for( i = 15; i < 15 + MAX_BUF_NUM/2; i++ ){

		sprintf(pData, "[block %d]", i + 100);
		BufWrite(i,pData);
    }
	printf("========= BufSync() =========\n" );
	BufSync();

    PrintBufInfo();
	PrintDiskInfo(0, 19);
	
	PrintDiskAccessCountInfo();//modified

    return 1;
}

int main()
{
    BufInit();

	if ( testcase1() == 1 )
		printf("\n testcase 1 complete. \n\n");
	else
		goto out;	// modified

	if ( testcase2() == 1 )
		printf("\n testcase 2 complete. \n\n");
	else
		goto out;	// modified

	if ( testcase3() == 1 )
		printf("\n testcase 3 complete. \n\n");
	else
		goto out;	// modified

out:
	BufFinish();

    return 0;
}

void PrintBufInfo()
{
	int		i, j, numBuf = 0;
	Buf*	ppBufInfo[MAX_BUF_NUM];

	memset( ppBufInfo, 0, (MAX_BUF_NUM * sizeof(Buf*)) );
	GetBufInfoByListNum( BUF_LIST_DIRTY, ppBufInfo, &numBuf );

	printf( "Num of Bufs in the BUF_LIST_DIRTY : %d\n", numBuf );
    for( i = 0; i < numBuf; i++ ){
		printf( "Buf[%d,", ppBufInfo[i]->blkno );
		if ( ppBufInfo[i]->state == BUF_STATE_CLEAN )
		{
			printf( "C, " );
		}
		else if ( ppBufInfo[i]->state ==  BUF_STATE_DIRTY )
		{
			printf( "D, " );
		}
		else {
			printf( "I, " );
		}
		printf( "%s], ", (char*)ppBufInfo[i]->pMem );
    }

    printf( "\n----------------------------------\n" );


	memset( ppBufInfo, 0, (MAX_BUF_NUM * sizeof(Buf*)) );
	GetBufInfoByListNum( BUF_LIST_CLEAN, ppBufInfo, &numBuf );

	printf( "Num of Bufs in the BUF_LIST_CLEAN : %d\n", numBuf );
    for( i = 0; i < numBuf; i++ ){
		printf( "Buf[%d,", ppBufInfo[i]->blkno );
		if ( ppBufInfo[i]->state == BUF_STATE_CLEAN )
		{
			printf( "C, " );
		}
		else if ( ppBufInfo[i]->state ==  BUF_STATE_DIRTY )
		{
			printf( "D, " );
		}
		else {
			printf( "I, " );
		}
		printf( "%s], ", (char*)ppBufInfo[i]->pMem );
    }

    printf( "\n----------------------------------\n" );


	memset( ppBufInfo, 0, (MAX_BUF_NUM * sizeof(Buf*)) );
	GetBufInfoByListNum( BUF_LIST_FREE, ppBufInfo, &numBuf );

	printf( "Num of Bufs in the BUF_LIST_FREE : %d\n", numBuf );
    for( i = 0; i < numBuf; i++ ){
		printf( "Buf[%d,", ppBufInfo[i]->blkno );
		if ( ppBufInfo[i]->state == BUF_STATE_CLEAN )
		{
			printf( "C], " );
		}
		else if ( ppBufInfo[i]->state ==  BUF_STATE_DIRTY )
		{
			printf( "D], " );
		}
		else {
			printf( "I], " );
		}
    }

    printf( "\n----------------------------------\n" );

	memset( ppBufInfo, 0, (MAX_BUF_NUM * sizeof(Buf*)) );
	GetBufInfoInLruList( ppBufInfo, &numBuf );

	printf( "Num of Bufs in the Global list : %d\n", numBuf );
    for( i = 0; i < numBuf; i++ ){
		printf( "Buf[%d,", ppBufInfo[i]->blkno );
		if ( ppBufInfo[i]->state == BUF_STATE_CLEAN )
		{
			printf( "C], " );
		}
		else if ( ppBufInfo[i]->state ==  BUF_STATE_DIRTY )
		{
			printf( "D], " );
		}
		else {
			printf( "I], " );
		}
    }

    printf( "\n----------------------------------\n" );

}

void PrintDiskInfo(int start_blk, int end_blk)
{
	int i;
	char buf[BUF_SIZE];

	printf( "Disk block Info\n" );
	for ( i = start_blk; i <= end_blk; i++)
	{
		DevReadBlock( i, buf );
		printf("%d : %s ", i, buf);
	}
	printf( "\n----------------------------------\n" );
}

void PrintDiskAccessCountInfo() //modified
{
	printf("---------- Informaion of Disk Access Count ---------- \n");
    printf("Disk Read Count : %d\n",DevGetDiskReadCount());
    printf("Disk Write Count : %d\n",DevGetDiskWriteCount());
    printf("Disk Read Count + Disk Write Count : %d\n",DevGetDiskReadCount() + DevGetDiskWriteCount());
    printf("----------------------------------------------------- \n");	
}

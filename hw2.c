#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/sched.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include "hw2.h"
#include "Disk.h"

#define DAEMON_STACK_SIZE (2048)
#define DAEMON_LOWER	(1)
#define DAEMON_UPPER	(3)

int cloneID;
int semap = 1;
	
void Init();
Buf* LruFind(int blkno);
void LruInsert (Buf* pBuf);
Buf* LruDeleteAndGetLru();
void BufInsertToTail(Buf* pBuf, int blkno, BufList listNum);
void  BufInsertToHead(Buf* pBuf, int blkno, BufList listNum);
Buf* BufFind(int blkno);
void BufDeleteBuf(Buf* pBuf);
Buf* BufGetNewBuffer(void);
void InsertObjectIntoObjFreeList(Buf* pBuf);
void BufInit(void);
void BufWrite(int blkno, void* pData);
void BufSync(void);
void BufDaemon();
void GetBufInfoByListNum(BufList listnum, Buf** ppBufInfo, int* pNumBuf);
void GetBufInfoInLruList(Buf** ppBufInfo, int* pNumBuf);
void BufFinish();

void BufFinish() {}

void Init() {
	int flags = SIGCHLD|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_VM;
	
	char* daemonStack;
	daemonStack = (char*)malloc(DAEMON_STACK_SIZE);
	
	cloneID = clone(BufDaemon, (char*)daemonStack+DAEMON_STACK_SIZE, flags, NULL);
	kill(cloneID, SIGSTOP);
}

Buf* LruFind(int blkno) {
	Buf* iterator;

	if (pLruListHead == NULL)
		return NULL;

	for (iterator = pLruListHead; iterator != NULL; iterator = iterator->plNext) {
		if (iterator->blkno == blkno)
			return iterator;
	}

	return NULL;
}

// MRU 갱신
void LruInsert (Buf* pBuf) {
	// 이미 있는 Buf가 MRU가 되어 다시 들어오는 경우.
	Buf* find = LruFind(pBuf->blkno);
	if (find != NULL) {
		// 이미 들어와있던 이 Buf가 LRU이자 MRU인 경우
		if (find == pLruListHead && find == pLruListTail) {
			// 그대로 두면 되므로
			return;
		}
		// 이미 들어와있던 이 Buf가 MRU인 경우
		else if (find == pLruListTail) {
			// 그대로 두면 되므로
			return;
		}
		// 이미 들어와있던 이 Buf가 LRU인 경우
		else if (find == pLruListHead) {
			find->plNext->plPrev = NULL;
			pLruListHead = find->plNext;
			LruInsert(find); // Recursive
		}
		else {
			find->plNext->plPrev = find->plPrev;
			find->plPrev->plNext = find->plNext;
			LruInsert(find); // Recursive
		}
	}
	
	else {
		// 새로운 Buf가 들어온 경우.
		if (pLruListHead == NULL) {
			// 리스트가 비어있는 경우.
			pBuf->plPrev = NULL;
			pBuf->plNext = NULL;
			pLruListHead = pBuf;
			pLruListTail = pBuf;
		}
		else {
			pLruListTail->plNext = pBuf;
			pBuf->plPrev = pLruListTail;
			pBuf->plNext = NULL;
			pLruListTail = pBuf;
		}
	}
}


// 성공시 Lru를 return.
// 실패시 NULL을 return.
Buf* LruDeleteAndGetLru() {
	Buf* pResult;
	/*
	// 리스트에 하나밖에 없을 경우(헤더이자 테일)
	if (pLruListHead == pLruListTail) {
		// Object Table과 Hash Table 처리
		BufDeleteBuf(pLruListHead);
		
		pResult = pLruListHead;
		pLruListHead = pLruListTail = NULL;
	}
	// 리스트가 비어있는 경우 (데몬에 의해 없을 경우이지만 만약을 위해 예외처리한다.)
	else if (pLruListHead == NULL) {
		return NULL;
	}
	else {*/
		// Object Table과 Hash Table 처리
		BufDeleteBuf(pLruListHead);
		
		pResult = pLruListHead;
		pLruListHead->plNext->plPrev = NULL;
		pLruListHead = pResult->plNext;
	//}
	
	
	pResult->plPrev = pResult->plNext = NULL;
	
	
	return pResult;
}

void BufInsertToTail(Buf* pBuf, int blkno, BufList listNum) {
	// ### Object Table Setting ### //
	if (ppObjListHead[listNum] == NULL) {
		// 리스트가 비어있는 경우.
		pBuf->poPrev = NULL;
		pBuf->poNext = NULL;
		ppObjListHead[listNum] = pBuf;
		ppObjListTail[listNum] = pBuf;
	}
	else {
		ppObjListTail[listNum]->poNext = pBuf;
		pBuf->poPrev = ppObjListTail[listNum];
		pBuf->poNext = NULL;
		ppObjListTail[listNum] = pBuf;
	}

	// ### Hash Table Setting ### //
	if (ppHashTail[blkno%HASH_TBL_SIZE] == NULL) {
		// Hash Table이 비어있는 경우
		ppHashHead[blkno%HASH_TBL_SIZE] = pBuf;
		ppHashTail[blkno%HASH_TBL_SIZE] = pBuf;
		pBuf->phPrev = NULL;
		pBuf->phNext = NULL;
	}
	else {
		ppHashTail[blkno%HASH_TBL_SIZE]->phNext = pBuf;
		pBuf->phPrev = ppHashTail[blkno%HASH_TBL_SIZE];
		pBuf->phNext = NULL;
		ppHashTail[blkno%HASH_TBL_SIZE] = pBuf;
	}
	pBuf->blkno = blkno;
}


void  BufInsertToHead(Buf* pBuf, int blkno, BufList listNum) {
	// ### Object Table Setting ### //
	if (ppObjListHead[listNum] == NULL) {
		// 리스트가 비어있는 경우.
		pBuf->poPrev = NULL;
		pBuf->poNext = NULL;
		ppObjListHead[listNum] = pBuf;
		ppObjListTail[listNum] = pBuf;
	}
	else {
		ppObjListTail[listNum]->poNext = pBuf;
		pBuf->poPrev = ppObjListTail[listNum];
		pBuf->poNext = NULL;
		ppObjListTail[listNum] = pBuf;
	}

	// ### Hash Table Setting ### //
	if (ppHashTail[blkno%HASH_TBL_SIZE] == NULL) {
		// Hash Table이 비어있는 경우
		ppHashHead[blkno%HASH_TBL_SIZE] = pBuf;
		ppHashTail[blkno%HASH_TBL_SIZE] = pBuf;
		pBuf->phPrev = NULL;
		pBuf->phNext = NULL;
	}
	else {
		ppHashHead[blkno%HASH_TBL_SIZE]->phPrev = pBuf;
		pBuf->phNext = ppHashHead[blkno%HASH_TBL_SIZE];
		pBuf->phPrev = NULL;
		ppHashHead[blkno%HASH_TBL_SIZE] = pBuf;
	}
	pBuf->blkno = blkno;
}

// 성공시 Object* 반환
// 실패시 NULL 반환
Buf* BufFind(int blkno) {
	Buf* iterator;

	if (ppHashHead[blkno%HASH_TBL_SIZE] == NULL) {
		return NULL;
	}

	for (iterator = ppHashHead[blkno%HASH_TBL_SIZE]; iterator != NULL; iterator = iterator->phNext)
		if (iterator->blkno == blkno) return iterator;
	return NULL;
}

void BufDeleteBuf(Buf* pBuf) {
	int i = 0;
	for (i = 0; i < 2; i++) {
		// Obj가 Object 리스트의 헤더이자 테일인 경우
		if (pBuf == ppObjListHead[i] && pBuf == ppObjListTail[i]) {
			ppObjListHead[i] = ppObjListTail[i] = NULL;
			break;
		}
		// Obj가 Object 리스트의 헤더인 경우
		else if (pBuf == ppObjListHead[i]) {
			pBuf->poNext->poPrev = NULL;
			ppObjListHead[i] = pBuf->poNext;
			break;
		}
		// Obj가 Object 리스트의 테일인 경우
		else if (pBuf == ppObjListTail[i]) {
			pBuf->poPrev->poNext = NULL;
			ppObjListTail[i] = pBuf->poPrev;
			break;
		}
		// Obj가 테이블의 중간에 있는 일반적인 경우
		else if (i == BUF_LIST_CLEAN) { // 위 조건에 모두 충족되지 않고 넘어왔으므로 일반적인 경우가 된다.
			pBuf->poPrev->poNext = pBuf->poNext;
			pBuf->poNext->poPrev = pBuf->poPrev;
			break;
		}
	}
	pBuf->poNext = pBuf->poPrev = NULL;

	for (i = 0; i < HASH_TBL_SIZE; i++) {
		// Obj가 Hash Table의 헤더이자 테일인 경우
		if (pBuf == ppHashHead[i] && pBuf == ppHashTail[i]) {
			ppHashHead[i] = ppHashTail[i] = NULL;
			break;
		}
		// Obj가 Hash Table의 헤더인 경우
		else if (pBuf == ppHashHead[i]) {
			pBuf->phNext->phPrev = NULL;
			ppHashHead[i] = pBuf->phNext;
			break;
		}
		// Obj가 Hash Table의 테일인 경우
		else if (pBuf == ppHashTail[i]) {
			pBuf->phPrev->phNext = NULL;
			ppHashTail[i] = pBuf->phPrev;
			break;
		}
		// Obj가 테이블의 중간에 있는 일반적인 경우
		else if (i == HASH_TBL_SIZE - 1) {// 위 조건에 모두 충족되지 않고 넘어왔으므로 일반적인 경우가 된다.
			pBuf->phPrev->phNext = pBuf->phNext;
			pBuf->phNext->phPrev = pBuf->phPrev;
			break;
		}
	}
	pBuf->phNext = pBuf->phPrev = NULL;
}

Buf* BufGetNewBuffer(void) {
	Buf* temp = ppObjListTail[BUF_LIST_FREE];
	/*
	if (temp == ppObjListHead[BUF_LIST_FREE]) {
		// 남아있는 Obj가 1개인 경우 (헤드이자 테일인 경우)
		temp->poNext = temp->poPrev = NULL;
		ppObjListTail[BUF_LIST_FREE] = ppObjListHead[BUF_LIST_FREE] = NULL;
	}
	else if (temp == NULL) {
		// 비어있는 리스트에서 빼려하는 경우
		return NULL;
	}
	else {*/
		ppObjListTail[BUF_LIST_FREE] = ppObjListTail[BUF_LIST_FREE]->poPrev;
		ppObjListTail[BUF_LIST_FREE]->poNext = NULL;
	//}
	
	if (ppObjListHead[BUF_LIST_FREE] == ppObjListTail[BUF_LIST_FREE]) {
		// Daemon 깨우기
		semap = 0;
		kill(cloneID, SIGCONT);
		while(semap != 1) {
		
		}
	}
	
	return temp;
}

void InsertObjectIntoObjFreeList(Buf* pBuf) {
	// 비어있는 리스트에 추가하려는 경우
	if (ppObjListTail[BUF_LIST_FREE] == NULL) {
		ppObjListHead[BUF_LIST_FREE] = pBuf;
		ppObjListTail[BUF_LIST_FREE] = pBuf;
		pBuf->poPrev = pBuf->poNext = NULL;
	}
	else {
		ppObjListHead[BUF_LIST_FREE]->poPrev = pBuf;
		pBuf->poNext = ppObjListHead[BUF_LIST_FREE];
		pBuf->poPrev = NULL;
		ppObjListHead[BUF_LIST_FREE] = pBuf;
	}
	
	if (pBuf->state == BUF_STATE_DIRTY) {
		DevWriteBlock(pBuf->blkno, pBuf->pMem);
	}
	
	pBuf->state = -1;
	pBuf->blkno = BLKNO_INVALID;
	pBuf->phPrev = pBuf->phNext = pBuf->plPrev = pBuf->plNext = NULL;
}

void BufInit(void) {
	int i = 0;
	Buf* pBuf = (Buf*)malloc(sizeof(Buf) * MAX_BUF_NUM);
	
	for (i; i < MAX_BUF_NUM; i++) {
		(pBuf + i)->state = -1;
		(pBuf + i)->pMem = malloc(strlen("block[0000]")+1);
		BufInsertToTail(pBuf + i, BLKNO_INVALID, BUF_LIST_FREE);
	}
	Init();
	DevCreateDisk();
	DevOpenDisk();
}



void BufRead(int blkno, void* pData) {
	Buf* find = BufFind(blkno);
	Buf* pNewBuf;
	// HIT
	if (find != NULL) {
		LruInsert(find);
		//pData = find->pMem;
		memmove(pData, find->pMem, strlen(find->pMem) + 1);
	}
	// MISS
	else {
		// Create New Buffer
		pNewBuf = BufGetNewBuffer();
		pNewBuf->blkno = blkno;
		pNewBuf->state = BUF_STATE_CLEAN;
		DevReadBlock(blkno, pNewBuf->pMem);

		BufInsertToTail(pNewBuf, pNewBuf->blkno, BUF_LIST_CLEAN);
		LruInsert(pNewBuf);

		memmove(pData, pNewBuf->pMem, strlen(pNewBuf->pMem) + 1);
	}
}

void BufWrite(int blkno, void* pData) {
	Buf* find = BufFind(blkno);
	Buf* pNewBuf;
	// HIT
	if (find != NULL) {
		if (find->state == BUF_STATE_CLEAN && find->state != -1) {
			// Move to Dirty list.
			find->state = BUF_STATE_DIRTY;
			BufDeleteBuf(find);
			BufInsertToTail(find, blkno, BUF_LIST_DIRTY);
		}
		LruInsert(find);
		
		memmove(find->pMem, pData, strlen(pData) + 1);
	}
	// MISS
	else {
		// Create New Buffer
		pNewBuf = BufGetNewBuffer();
		pNewBuf->blkno = blkno;
		pNewBuf->state = BUF_STATE_DIRTY;

		BufInsertToTail(pNewBuf, pNewBuf->blkno, BUF_LIST_DIRTY);
		LruInsert(pNewBuf);
		
		memmove(pNewBuf->pMem, pData, strlen(pData) + 1);
	}
}

void BufSync(void) {
	Buf* iterator;
	Buf* temp;
	for (iterator = ppObjListHead[BUF_LIST_DIRTY]; iterator != NULL; iterator = temp) {
		temp = iterator->poNext;
		DevWriteBlock(iterator->blkno, iterator->pMem);
		iterator->state = BUF_STATE_CLEAN;
		BufDeleteBuf(iterator);
		BufInsertToTail(iterator, iterator->blkno, BUF_LIST_CLEAN);
	}
}

void BufDaemon() {
	while(1) {
		if (ppObjListHead[BUF_LIST_FREE] == ppObjListTail[BUF_LIST_FREE] && semap == 0) {
			InsertObjectIntoObjFreeList(LruDeleteAndGetLru());
			InsertObjectIntoObjFreeList(LruDeleteAndGetLru());
			semap = 1;
			kill(cloneID, SIGSTOP);
		}
	}
}

void GetBufInfoByListNum(BufList listnum, Buf** ppBufInfo, int* pNumBuf) {
	Buf* iterator;
	*pNumBuf = 0;

	for (iterator = ppObjListHead[listnum]; iterator != NULL; iterator = iterator->poNext) {
		ppBufInfo[(*pNumBuf)++] = iterator;
	}
}

void GetBufInfoInLruList(Buf** ppBufInfo, int* pNumBuf) {
	Buf* iterator;
	*pNumBuf = 0;

	for (iterator = pLruListHead; iterator != NULL; iterator = iterator->plNext) {
		ppBufInfo[(*pNumBuf)++] = iterator;
	}
}

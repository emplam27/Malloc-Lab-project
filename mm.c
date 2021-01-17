/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4             // word, header, footer 사이즈(byte)
#define DSIZE 8             // double word 사이즈(byte)
#define CHUNKSIZE (1<<12)   // heap 확장 사이즈(byte)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))    // 사이즈와 할당된 비트를 word로 압축

#define GET(p)      (*(unsigned int *)(p))       // 해당 주소 안에 실제 값 불러오기
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 해당 주소 안의 실제 값 저장하기

#define GET_SIZE(p)  (GET(p) & ~0x7) // '... 1111 1000'과 AND 연산하여 block의 사이즈 확인하기
#define GET_ALLOC(p) (GET(p) & 0x1)  // '... 0000 0001'과 AND 연산하여 block의 할당 여부 확인하기

#define HDRP(bp) ((char *)(bp) - WSIZE)                      // block pointer를 받아 header 포인터 확인하기
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // block pointer를 받아 footer 포인터 확인하기

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음 block의 block pointer로 이동하기
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 block의 block pointer로 이동하기

static void *extend_heap(size_t words);
static void *coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static char *heap_listp; // 힙의 시작점을 나타내는 포인터

/* 
 * mm_init - 할당을 수행할 힙 영역을 만들어줌
 * mm_init의 return value는 프로그램 시작에 문제가 생겼을 경우 -1, 그렇지 않을 경우 0(정상작동)
 */
int mm_init(void)
{   
    // 비어있는 힙 생성
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*) - 1) {
        return -1;
    }

    /*
     * 비어있는 힙에 사전작업을 수행. 처음 생성되는 heap의 사이즈는 16byte이며, 
     * 순서대로 start point, initial block header, initial block footer, end point 모두 4byte로 구성됨
     */ 

    // start point: heap이 시작되는 지점을 알기위한 부분. padding의 역할도 수행
    PUT(heap_listp, 0);  
    // initial block header: heap에 할당된 처음 block header. 이후 블록을 찾아갈 수 있게 하는 역할
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    // heap에 할당된 처음 block footer. heap의 처음 블록을 알 수 있게 하는 역할
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    // end point: heap이 끝나는 지점을 알기 위한 부분. heap이 확장되면 확장된 heap의 끝으로 이동
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    heap_listp += (2 * WSIZE);  // initial block의 bp가 되어 탐색의 시작점이 되는 포인터

    // CHUNKSIZE 만큼 힙을 확장. 오류상황이 발생하면 -1, 성공적으로 확장하였다면 0 반환
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *     malloc은 리턴하는 메모리를 초기화하지 않는다.
 */
void *mm_malloc(size_t size)
{
    size_t adjust_size;           // 블록 사이즈 조정
    size_t extend_size;           // 힙 확장 사이즈
    char* bp;

    if (size == 0) {
        return NULL;
    }
    
    // 만일 DSIZE 이하의 값이 들어오면 DSIZE만큼 payload를 만들어 주고, header와 footer를 포함하여 16의 블록을 할당
    if (size <= DSIZE) {
        adjust_size = DSIZE * 2;
    } 
    // header와 footer를 포함하여 DSIZE의 배수만큼 block 사이즈를 할당
    else {
        adjust_size = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);
    }
    
    // 사이즈에 맞는 위치 탐색
    if ((bp = find_fit(adjust_size)) != NULL) {
        place(bp, adjust_size);
        return bp;
    }
    // 사이즈에 맞는 위치가 없는 경우, 추가적으로 힙 영역 요청 및 배치
    extend_size = MAX(adjust_size, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, adjust_size);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    // printf("freeeeeeeeeeeeeeeeeeeeeeeeee\n");
    // printf("HDRP(bp): %p\n", HDRP(bp));
    // printf("GET_ALLOC(HDRP(bp)): %d\n", GET_ALLOC(HDRP(bp)));
    // printf("GET_SIZE(HDRP(bp)): %d\n", GET_SIZE(HDRP(bp)));
    // printf("\n");
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE((char *)oldptr - WSIZE) - DSIZE; // header의 사이즈
    if (size < copySize) {
      copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // words가 홀수여도 짝수로 만들어주기
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    // 오류상황이 발생하면 프로그램을 종료
    // 오류상황이 발생하지 않으면 block pointer는 새로 할당된 힙의 처음 block pointer
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // printf("extend1111111111111111111111\n");
    // 처음 할당된 heap의 경우에는 alloc이 1인 block header와 block footer를 만들어 줬다면, 
    // 새로 할당된 heap의 경우에는 alloc이 0인 block header와 block footer가 필요함
    // 새로 할당된 heap에 free block header, free block footer, end point를 지정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    // printf("extend222222222222222222\n");

    // 만일 이전 블록이 free상태인 블록이라면 합쳐주기
    return coalesce(bp);
}

static void *coalesce(void* bp)
{
    // printf("start coalesce  eeeeeeeeeeeeeeeeeeee\n");
    // printf("bp: %p\n", bp);
    // printf("HDRP(bp): %p\n", HDRP(bp));
    // printf("GET_ALLOC(HDRP(bp)): %d\n", GET_ALLOC(HDRP(bp)));
    // printf("GET_SIZE(HDRP(bp)): %d\n", GET_SIZE(HDRP(bp)));
    // printf("FTRP(bp): %p\n", FTRP(bp));
    // printf("GET_ALLOC(FTRP(bp)): %d\n", GET_ALLOC(FTRP(bp)));
    // printf("GET_SIZE(FTRP(bp)): %d\n", GET_SIZE(FTRP(bp)));
    // printf("((char *)(bp) - DSIZE)): %p\n", ((char *)(bp) - DSIZE));
    // printf("GET_SIZE(((char *)(bp) - DSIZE))): %d\n", GET_SIZE(((char *)(bp) - DSIZE)));
    // printf("\n");
    // printf("PREV_BLKP(bp): %p\n", PREV_BLKP(bp));
    // printf("FTRP(PREV_BLKP(bp)): %p\n", FTRP(PREV_BLKP(bp)));
    // printf("GET_ALLOC(FTRP(PREV_BLKP(bp))): %d\n", GET_ALLOC(FTRP(PREV_BLKP(bp))));
    // printf("GET_SIZE(FTRP(PREV_BLKP(bp))): %d\n", GET_SIZE(FTRP(PREV_BLKP(bp))));
    // printf("HDRP(PREV_BLKP(bp)): %p\n", HDRP(PREV_BLKP(bp)));
    // printf("GET_ALLOC(HDRP(PREV_BLKP(bp))): %d\n", GET_ALLOC(HDRP(PREV_BLKP(bp))));
    // printf("GET_ALLOC(HDRP(PREV_BLKP(bp))): %d\n", GET_ALLOC(HDRP(PREV_BLKP(bp))));
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    // printf("prev_alloc: %d\n", prev_alloc);
    // printf("middle1111111 coalesce  eeeeeeeeeeeeeeeeeeee\n");
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // printf("middle22222222 coalesce  eeeeeeeeeeeeeeeeeeee\n");
    size_t size = GET_SIZE(HDRP(bp));
    // printf("middle33333333333 coalesce  eeeeeeeeeeeeeeeeeeee\n");

    // free한 block 앞, 뒤에 모두 할당 되어있는 block이 있는 경우
    if (prev_alloc && next_alloc) {
        // printf("coalesce  1111111111111111111\n");
        return bp;
    }

    // free한 블록 뒤에만 free 되어있는 block이 있는 경우
    else if (prev_alloc && !next_alloc) {
        // printf("coalesce  2222222222222222222\n");

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // free한 블록 앞에만 free 되어있는 block이 있는 경우
    else if (!prev_alloc && next_alloc) {
        // printf("coalesce  333333333333333333333\n");

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // free한 block 앞, 뒤에 모두 free 되어있는 block이 있는 경우
    else {
        // printf("coalesce  444444444444444444444\n");

        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // printf("end coalesce  eeeeeeeeeeeeeeeeeeee\n");
    return bp;
}

static void* find_fit(size_t asize) 
{
    size_t curr_alloc;
    size_t curr_size;
    void* curr_bp = heap_listp;

    // printf("-------------beforefind-------------\n");
    // printf("bp : %p\n", curr_bp);
    // printf("asize : %d\n", asize);
    // printf("GET_ALLOC(HDRP(curr_bp) : %d\n", GET_ALLOC(HDRP(curr_bp)));
    // printf("GET_SIZE(HDRP(bp)) : %d\n", GET_SIZE(HDRP(curr_bp)));
    // printf("\n");

    while (1) {
        // printf("-------------find-------------\n");
        // printf("bp : %p\n", curr_bp);
        // printf("asize : %d\n", asize);
        // printf("HDRP(curr_bp) : %p\n", HDRP(curr_bp));
        // printf("GET_ALLOC(HDRP(curr_bp) : %d\n", GET_ALLOC(HDRP(curr_bp)));
        // printf("GET_SIZE(HDRP(bp)) : %d\n", GET_SIZE(HDRP(curr_bp)));
        // printf("FTRP(curr_bp) : %p\n", FTRP(curr_bp));
        // printf("GET_ALLOC(FTRP(curr_bp)) : %d\n", GET_ALLOC(FTRP(curr_bp)));
        // printf("GET_SIZE(FTRP(curr_bp)) : %d\n", GET_SIZE(FTRP(curr_bp)));
        // printf("\n");
        curr_alloc = GET_ALLOC(HDRP(curr_bp));
        curr_size = GET_SIZE(HDRP(curr_bp));

        // end point에 도달하게 되면 NULL 반환
        if (!curr_size) {
            // printf("find endpoint\n");
            return NULL;
        }

        // 현재 포인터의 block이 할당되어 있다면 사이즈만큼 뒤로 이동
        if (curr_alloc) {
            curr_bp = NEXT_BLKP(curr_bp);
            continue;
        }
        // 현재 포인터의 block이 할당되어있지 않고, 사이즈가 asize보다 작다면 뒤로 이동
        if (!curr_alloc && (curr_size < asize)) {
            curr_bp = NEXT_BLKP(curr_bp);
            continue;
        }
        // 현재 포인터의 block이 할당되어있지 않고, 사이즈가 asize보다 크다면 현재 위치 반환
        // printf("find fit success\n");
        return curr_bp;
    }
    // printf("find end???????????????\n");
}

static void place(void* bp, size_t asize) 
{
    // printf("-------------place-------------\n");
    // printf("bp : %p\n", bp);
    // printf("asize : %d\n", asize);
    // printf("GET_SIZE(HDRP(bp)) : %d\n", GET_SIZE(HDRP(bp)));

    // 만일 할당 하려는 사이즈와 공간의 사이즈가 같다면 header, footer의 alloc만 변환
    if (GET_SIZE(HDRP(bp)) == asize) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // printf("header : %p\n", HDRP(bp));
        // printf("GET_ALLOC(HDRP(bp)) : %d\n", GET_ALLOC(HDRP(bp)));
        // printf("GET_SIZE(HDRP(bp)) : %d\n", GET_SIZE(HDRP(bp)));
        // printf("footer : %p\n", FTRP(bp));
        // printf("GET_ALLOC(FTRP(bp)) : %d\n", GET_ALLOC(FTRP(bp)));
        // printf("GET_SIZE(FTRP(bp)) : %d\n", GET_SIZE(FTRP(bp)));
        // printf("\n");
    }

    /* 
     * 만일 asize보다 공간의 사이즈가 크다면,
     * HDRP(old_bp)에 asize, alloc 1 설정
     * FTRP(new_bp)에 asize, alloc 1 설정
     * HDRP(new_bp)에 old_size - asize, alloc 0 설정
     * FTRP(old_bp)에 old_size - asize, alloc 0 설정
     */
    else {
        // printf("place unsame size\n");
        // printf("\n");
        size_t old_size = GET_SIZE(HDRP(bp));
        size_t new_size = old_size - asize;
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(new_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(new_size, 0));
        // void *old_bp, *new_bp;
        // size_t old_size, remain_size;

        // old_size = GET_SIZE(HDRP(bp));
        // remain_size = old_size - asize;

        // old_bp = bp;
        // new_bp = old_bp + asize;

        // void* old_header = HDRP(old_bp);
        // void* old_footer = FTRP(old_bp);
        // PUT(old_header, PACK(asize, 1)); // old_header
        // PUT(old_footer, PACK(remain_size, 0)); // old_footer

        // void* new_header = HDRP(new_bp);
        // void* new_footer = FTRP(old_bp);
        // PUT(new_header, PACK(remain_size, 0)); // new_header
        // PUT(new_footer, PACK(asize, 1)); // new_footer

        // printf("old_header : %p\n", old_header);
        // printf("GET_ALLOC(old_header) : %d\n", GET_ALLOC(old_header));
        // printf("GET_SIZE(old_header) : %d\n", GET_SIZE(old_header));
        // printf("old_footer : %p\n", old_footer);
        // printf("GET_ALLOC(old_footer) : %d\n", GET_ALLOC(old_footer));
        // printf("GET_SIZE(old_footer) : %d\n", GET_SIZE(old_footer));
        // printf("new_header : %p\n", new_header);
        // printf("GET_ALLOC(new_header) : %d\n", GET_ALLOC(new_header));
        // printf("GET_SIZE(new_header) : %d\n", GET_SIZE(new_header));
        // printf("new_footer : %p\n", new_footer);
        // printf("GET_ALLOC(new_footer) : %d\n", GET_ALLOC(new_footer));
        // printf("GET_SIZE(new_footer) : %d\n", GET_SIZE(new_footer));
        // printf("\n");

    }
}









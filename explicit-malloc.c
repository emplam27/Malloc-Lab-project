#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "1",
    /* First member's full name */
    "emplam27",
    /* First member's email address */
    "emplam27@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define WSIZE     sizeof(void *)    // word, header, footer 사이즈(byte)
#define DSIZE     (2 * WSIZE)       // double word 사이즈(byte)
#define CHUNKSIZE (1<<12)           // heap 확장 사이즈(byte)
#define MINIMUM   24           // heap 확장 사이즈(byte)

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

/* freeList의 이전 포인터와 다음 포인터 계산 */
#define NEXT_FLP(bp)  (*((char**)(bp) + 1))      // 다음 free list 요소의 bp를 가져옴
#define PREV_FLP(bp)  (*((char**)(bp)))          // 이전 free list 요소의 bp를 가져옴

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_free(void *bp);
static void del_free(void *bp);

static char *heap_listp; // 힙의 시작점을 나타내는 포인터
static char *free_listp; // free block들의 시작점을 나타내는 포인터

int mm_init(void)
{   
    if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void*) - 1) {
        return -1;
    }

    PUT(heap_listp, 0);  
    PUT(heap_listp + (1 * WSIZE), PACK(MINIMUM, 1));
    PUT(heap_listp + (2 * WSIZE), NULL);
    PUT(heap_listp + (3 * WSIZE), NULL);
    PUT(heap_listp + (6 * WSIZE), PACK(MINIMUM, 1));
    PUT(heap_listp + (7 * WSIZE), PACK(0, 1));

    free_listp = heap_listp + (2 * WSIZE);
    
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t adjust_size;
    size_t extend_size;
    char* bp;

    if (size == 0) {
        return NULL;
    }
    if (size <= MINIMUM - DSIZE) {
        adjust_size = MINIMUM;
    } 
    else {
        adjust_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    if ((bp = find_fit(adjust_size)) != NULL) {
        place(bp, adjust_size);
        return bp;
    }
    extend_size = MAX(adjust_size, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, adjust_size);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

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

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if (size < MINIMUM) {
        size = MINIMUM;
    }
    
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        del_free(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        del_free(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if (!prev_alloc && !next_alloc) {
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        del_free(PREV_BLKP(bp));
        del_free(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_free(bp);
    return bp;
}

static void* find_fit(size_t asize) 
{
    void *bp = free_listp;
    while (!GET_ALLOC(HDRP(bp))) {
        if (asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
        bp = NEXT_FLP(bp);
    }
    return NULL;
}

static void place(void *bp, size_t asize) 
{
    int cur_size = GET_SIZE(HDRP(bp));
    del_free(bp);
    if (cur_size - asize >= MINIMUM) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(cur_size - asize, 0));
        PUT(FTRP(bp), PACK(cur_size - asize, 0));
        coalesce(bp);
    }
    else {
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
}


static void add_free(void *bp)
{
    NEXT_FLP(bp) = free_listp;
    PREV_FLP(bp) = NULL;
    PREV_FLP(free_listp) = bp;
    free_listp = bp;
}

static void del_free(void *bp)
{
    if (bp == free_listp) {
        PREV_FLP(NEXT_FLP(bp)) = PREV_FLP(bp);
        free_listp = NEXT_FLP(bp);
        return;
    }
    NEXT_FLP(PREV_FLP(bp)) = NEXT_FLP(bp);
    PREV_FLP(NEXT_FLP(bp)) = PREV_FLP(bp);
}

///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
//
///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Main File:        All test files mentioned below
// This File:        heapAlloc.c
// Other Files:      align1.c, align2.c, align3.c, alloc1.c, alloc1_nospace.c, 
//                   alloc2_nospace.c, alloc3.c, coalesce1.c, coalesce2.c, coalesce3.c
//                   coalesce4.c, coalesce5.c, coalesce6.c, free1.c, free2.c, writeable.c
// Semester:         CS 354 Fall 2019
//
// Author:            Shruti Sharma
// Email:             sharma224@wisc.edu
// CS Login:          shruti@cs.wisc.edu
//

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "heapAlloc.h"
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *previouslyAllocd = NULL ; // pointer to most recently allocated payload

//Function to return header to end of mark of heap
blockHeader* getEndMark() {
   return (blockHeader *)((void *)heapStart + allocsize);
}

/*Function for padding requested size of block to a multiple of 8
* 
* size - size requested by the user
* Returns the padded size
*/
int padToMultipleOf8(int size){
  if(size % 8 != 0){
      if(size < 8){
        size += (8 - size); //a padding of (8 - size)     
      }  
      if(size > 8){ 
        size = size + 8;
        size = size - (size % 8);
      }
  }
  return size;
}

/*Function to find free block in heap
*
* mostRecent - most recently allocated block
* requestedSize - size requested by the user
* Returns NULL if no free block found
* Returns pointer to header of next big enough free block
*/
blockHeader* findFreeBlock(blockHeader* mostRecent, int requestedSize){
    blockHeader *iterator = mostRecent; 
    while((iterator->size_status) != 1){ 
        if((iterator->size_status % 2) == 0 &&
            (iterator->size_status - (iterator->size_status % 8)) >= requestedSize) {
           return iterator; 
       } else {
           iterator = (blockHeader *)((void *)iterator +
                (iterator->size_status - (iterator->size_status % 8))); // move to next block
       }
   }
   iterator = heapStart;  //reset iterator to heapStart
   
   while(iterator != mostRecent){
       if((iterator->size_status % 2) == 0 &&
             (iterator->size_status - (iterator->size_status % 8)) >= requestedSize) {
         return iterator; 
       } else {
         iterator = (blockHeader *)((void *)iterator + 
             (iterator->size_status - (iterator->size_status % 8))); // move to next block
       }
   }    
   return NULL;
}

/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function:
 * - Checks size - Returns NULL if not positive or if larger than heap space.
 * - Determines block size rounding up to a multiple of 8 and adding padding as a result.
 * - Uses NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Uses SPLITTING to divide the chosen free block into two if it is too large.
 * - Updates header(s) and footer as needed
 */
void* allocHeap(int size) {     
    if(size <= 0 || size > allocsize){
      return NULL;
    }
    if(previouslyAllocd == NULL){
       previouslyAllocd = heapStart;
    }
    size = size + 4;
    size = padToMultipleOf8(size);
    blockHeader *current = findFreeBlock(previouslyAllocd, size); //current = header to free block
    if(current == NULL) {
       return NULL;
    }
    int currentSize = current->size_status - (current->size_status % 8);
    
    //split to fit requested size
    //size_status of current and next block updated
    if(currentSize > size){
        int newSize = currentSize - size;
        current->size_status = size + 3; //abit = 1, pbit = 1
        blockHeader *nextBlockHeader = (blockHeader*)((void*)current + size);
        nextBlockHeader->size_status = newSize + 2;
        blockHeader *nextBlockFooter = (blockHeader*)((void*)nextBlockHeader +
                                                newSize - sizeof(blockHeader));
        nextBlockFooter->size_status = newSize;
    }
    //No splitting required
    if(currentSize == size) {
        current->size_status += 1; //abit = 1, pbit = 1
        blockHeader *nextBlockHeader = (blockHeader*)((void*)current + size);
        if(nextBlockHeader->size_status != 1) {//if not end mark
            nextBlockHeader->size_status += 2;//pbit = 1, abit = 0
        }
    } 
    previouslyAllocd = current; // set previously allocd var to current header
    return (void*)current + sizeof(blockHeader); //return ptr to payload
} 

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function :
 * - Returns -1 if ptr is NULL.
 * - Returns -1 if ptr is not a multiple of 8.
 * - Returns -1 if ptr is outside of the heap space.
 * - Returns -1 if ptr block is already freed.
 * - USEs IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int freeHeap(void *ptr) {  
    if(ptr == NULL || ((int)ptr % 8) != 0 || ptr <= ((void *)heapStart) 
        || ptr >= (void *)(getEndMark()) || 
            (((blockHeader *)(ptr-4))->size_status % 2) == 0) {
      return -1;
    }
    blockHeader *currHdr = (blockHeader *)(ptr - 4); //header of the payload to be freed
    int currBlockSize = (currHdr->size_status - (currHdr->size_status % 8));
    blockHeader *nextHdr = (blockHeader *)((void *)currHdr + currBlockSize);
    int nextBlockSize = (nextHdr->size_status - (nextHdr->size_status % 8));
    if((currHdr->size_status & 2) == 0 && (nextHdr->size_status % 2) != 0) { //if only prev is free
       blockHeader *prevFooter = (blockHeader *)((void *)currHdr - 4);
       int prevBlockSize = prevFooter->size_status;
       blockHeader *prevHdr = (blockHeader *)((void *)currHdr - prevBlockSize);
       prevHdr->size_status = prevBlockSize + currBlockSize + 2;//UPDATE HEADER; pbit= 1, abit = 0
      ((blockHeader*)((void *)prevHdr + 
      (prevBlockSize + currBlockSize - 4)))->size_status = prevBlockSize + currBlockSize;// UPDATE FOOTER 
      if((currHdr == previouslyAllocd || prevHdr == previouslyAllocd)){
        if(nextHdr->size_status != 1){ //if not end mark
          previouslyAllocd = nextHdr; 
        }
      }
      if(nextHdr->size_status != 1){ //set next block's pbit = 0;
        nextHdr->size_status -= 2;
      }
      return 0;
    } else if((nextHdr->size_status % 2) == 0 && (currHdr->size_status & 2) != 0) { // if only next block is free
       blockHeader *nextFooter = (blockHeader *)((void *)nextHdr + (nextBlockSize - 4));
       nextFooter->size_status = currBlockSize + nextBlockSize; // UPDATE FOOTER 
       currHdr->size_status = currBlockSize + nextBlockSize + 2; //UPDATE HEADER pbit = 0, abit = 0 
       blockHeader *hdrAfterNext = (blockHeader *)((void *)nextHdr + nextBlockSize);
       if((currHdr == previouslyAllocd || nextHdr == previouslyAllocd)){
         if(hdrAfterNext->size_status != 1) {
            previouslyAllocd = hdrAfterNext;
         }
       }  
       return 0;
    } else if((currHdr->size_status & 2) == 0 && (nextHdr->size_status % 2) == 0) { //if previous 
                                                                                  //and next blocks are free
       blockHeader *prevFooter = (blockHeader *)((void *)currHdr - 4);
       int prevBlockSize = prevFooter->size_status;
       blockHeader *prevHdr = (void *)currHdr - prevBlockSize;
       prevHdr->size_status = prevBlockSize + currBlockSize + nextBlockSize + 2; //UPDATE HEADER
      ((blockHeader *)((void *)currHdr + 
          (nextBlockSize - 4)))->size_status = prevBlockSize + currBlockSize + nextBlockSize; //UPDATE FOOTER
       blockHeader *hdrAfterNext = (blockHeader *)((void *)nextHdr + nextBlockSize);
       if((currHdr == previouslyAllocd || nextHdr == previouslyAllocd || prevHdr == previouslyAllocd)){
         if(hdrAfterNext->size_status != 1 ){
             previouslyAllocd = hdrAfterNext;
         }
       }  
       return 0;
    } else { //if neither prev or next blocks are free
        currHdr->size_status -= 1; //UPDATE HEADER, set abit = 0
        if(nextHdr->size_status != 1){
          nextHdr->size_status -= 2; //set next block's pbit = 0
        }
        (((blockHeader *)((void *)currHdr + currBlockSize - 4)))->size_status = currBlockSize; //UPDATE FOOTER
        if(currHdr == previouslyAllocd){
          previouslyAllocd = nextHdr;
        }  
        return 0;
    }
    return -1;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int initHeap(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple initHeap calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dumpMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 

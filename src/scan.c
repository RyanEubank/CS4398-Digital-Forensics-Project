#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "scan.h"
#include "safeio.h"
#include "mbr.h"
#include "superblock.h"

#define INVALID_PARTITION "Invalid Partition: Partition %d does not exist.\n"
#define INVALID_MBR "Invalid MBR: Exiting program.\n"
#define INVALID_SUPERBLOCK "Invalid superblock: Exiting program.\n"
#define GRP_DESC_SIZE 32

uint32_t scanType = ALL_BLOCKS;
pSBlock sb = NULL;
uint8_t* bitmap = NULL;
uint32_t allocatedCount = 0;

// -1 forces datablock bitmap to be populated on first block
uint32_t currentBlockGrp = -1;

uint64_t parsePartitionAddr(int32_t);
void processPartition(process);
void processBlocks(uint32_t, process);
void printProgress(uint32_t*, uint32_t, uint32_t);
void getBitmap(uint32_t, uint32_t);
uint32_t isBlockIncluded(uint32_t);
uint32_t isPowerOf(uint32_t, uint32_t);
uint32_t isAllocated(uint32_t, uint32_t);
uint32_t getGrpDescTableSize(uint32_t);
uint64_t getBitmapAddr(uint32_t);

/* ============================================================
 * Gets the correct partition address and processes each block 
 * in the partition with the given function pointer.
 * 
 * Parameters:
 *  index - the index of the partition in the partition table.
 *  process - pointer to the function called for each block read.
 *  whichBlocks - flag indicating which blocks should be scanned.
 * 
 * Returns:
 * 	Returns the address of the partition if the requested
 *  partition exists, 0 otherwise.
 * ========================================================= */
uint64_t scanPartitionAndProcess(
	int32_t index,
	process process,
	uint32_t whichBlocks
) {
	uint64_t addr = parsePartitionAddr(index);
	if (addr > 0) {
		partition_addr = addr;
		scanType = whichBlocks;
		processPartition(process);
		return addr;
	}
	return 0;
}

/* ============================================================
 * Checks if the given partition is valid and has an entry in
 * the MBR, and returns the partition address.
 * 
 * Parameters:
 * 	partitionIndex - the index of the partition table entry.
 * 
 * Returns:
 * 	Returns a 64-bit byte address for the requested partition.
 *  or 0 if the mbr or partition index is invalid.
 * ========================================================= */
uint64_t parsePartitionAddr(int32_t partitionIndex) {
	pMbr mbr = readMBR(deviceID);
	uint64_t addr = 0;

	if (mbr->_mbr_signature == MBR_SIGNATURE) {
		addr = getPartAddr(mbr, partitionIndex);
		if (addr == 0) 
			fprintf(stderr, INVALID_PARTITION, partitionIndex + 1);
	} else 
		fprintf(stderr, INVALID_MBR);

	free(mbr);
	return addr;
}

/* ============================================================
 * Reads the superblock in the partition to get the neccessary
 * information to process each block with the given function 
 * pointer 'process'.
 * 
 * Parameters:
 *  process - the operation to perform on each block.
 * ========================================================= */
void processPartition(
	process process
) {
	sb = readSuperblock(deviceID, partition_addr + 1024);

	// superblock stores block size as 1024 * 2^n
	// where n is the value stored in the block size field
	blockSize = 1024 << sb->_block_size;
	uint32_t numBlocks = sb->_fs_size_blocks;
	totalBlocks = numBlocks;

	if (sb->_magic_sig == SUPERBLOCK_SIGNATURE) 
		processBlocks(numBlocks, process);
	else
		fprintf(stderr, INVALID_SUPERBLOCK);
	free(sb);
}

/* ============================================================
 * Scans the partition starting at the given address to perform
 * processing on each block with the given function pointer
 * 'process'.
 * 
 * Parameters:
 * 	numBlocks - the number of blocks in the partition.
 *  process - the operation to perform on each block.
 * ========================================================= */
void processBlocks(uint32_t numBlocks,process process) {
	printf("\nPartition Address: 0x%lx\n", partition_addr);
	printf("Block Size: 0x%x\n", blockSize);
	printf("\n---Scanning blocks---\n");

	uint8_t block[blockSize];
	uint32_t current_progress = 0;
	uint64_t nextAddr = partition_addr;

	// go through all blocks in the partition,
	// running each through the processing function
	// passed in the arguments
	for (uint32_t i = 0; i < numBlocks; i++) {
		printProgress(&current_progress, i, numBlocks);

		// skip block if allocated/unallocated/etc. based on
		// scan type
		if (isBlockIncluded(i)) {
			safeRead(deviceID, nextAddr, block, blockSize);
			process(block, nextAddr, i);
		}
		nextAddr += blockSize;
	}

	printProgress(&current_progress, numBlocks, numBlocks);
	printf("\n---Finished scanning---\n\n");

	// another sanity check -> allocated + free should = total blocks
	printf("Scanned %d total blocks.\n", numBlocks);
	printf("Allocated Count: %d\n", allocatedCount);
	printf("Free Blocks: %d\n", sb->_free_blocks);
}

/* ============================================================
 * Checks the relevant data block bitmap and returns whether 
 * the block should be processed in the scan or skipped over 
 * based on its allocation status and scan type.
 * 
 * Parameters:
 * 	blockNum - the block number of the block being scanned.
 * ========================================================= */
uint32_t isBlockIncluded(uint32_t blockNum) {
	// calc block group number of current block being scanned
	uint64_t numBlocksInGrp = (blockSize << 3);
	uint64_t blockGrpNum = blockNum / numBlocksInGrp;

	// only get a new bitmap if in a different block group
	if (blockGrpNum != currentBlockGrp) {
		currentBlockGrp = blockGrpNum;
		getBitmap(currentBlockGrp, blockNum);
	}

	uint32_t isAlloc =  isAllocated(blockNum, numBlocksInGrp);
	if (isAlloc)
		allocatedCount++;

	if (scanType == ALL_BLOCKS)
		return 1;

	return (scanType == ALLOCATED_ONLY) ? isAlloc : !isAlloc;
}

/* ============================================================
 * Returns whether the given block number is allocated in the
 * block bitmap.
 *
 * Parameters:
 *  blockNum - the block number to check
 *  blockSize - the size of the bitmap
 *  numBlocksInGrp - the size of a block group (in blocks)
 * 
 * Returns:
 * 	Returns a 1 if allocated, 0 otherwise.
 * ========================================================= */
uint32_t isAllocated(uint32_t blockNum, uint32_t numBlocksInGrp) {
	uint32_t bitPos = blockNum % numBlocksInGrp;
	uint32_t bytePos = bitPos >> 3;
	uint8_t allocationByte = *(bitmap + bytePos);
	return allocationByte & (1 << (bitPos % 8));
}

/* ============================================================
 * Finds and reads the datablock bitmap into the bitmap
 * buffer so processing can check for which blocks to scan/ignore.
 * 
 * Parameters:
 * 	grpNum - the number of the blockgroup to get the bitmap for.
 *  blockNum - the block number of the block being scanned.
 * ========================================================= */
void getBitmap(uint32_t grpNum, uint32_t blockNum) {
	// allocate the bitmap buffer if not already done
	if (!bitmap) {
		bitmap = (uint8_t*) malloc(blockSize);
		if (!bitmap)
			exit_err("Failed to allocate space for bitmap.");
	}
	
	// sanity check that block addresses/superblock copies
	// are correct
	if (
		grpNum == 0 || 
		grpNum == 1 || 
		isPowerOf(grpNum, 3) ||
		isPowerOf(grpNum, 5) ||
		isPowerOf(grpNum, 7)
	) {
		uint64_t sbAddr = partition_addr + 
			((uint64_t)blockNum * (uint64_t)blockSize);
		if (grpNum == 0)
			sbAddr += 1024;
			
		pSBlock s = readSuperblock(deviceID, sbAddr);
		if (s->_magic_sig != SUPERBLOCK_SIGNATURE) {
			fprintf(stderr, "Invalid Superblock at 0x%lx\n", sbAddr);
			exit(EXIT_FAILURE);
		}
	}

	uint64_t bitmapAddr = getBitmapAddr(grpNum);
	safeRead(deviceID, bitmapAddr, bitmap, blockSize);
}

/* ============================================================
 * Returns if num is a power of base.
 *
 * Parameters:
 *  num - the number to check
 *  base - the base of the powers being checked
 * 
 * Returns:
 * 	Returns a 1 if num is a power of base, 0 otherwise.
 * ========================================================= */
uint32_t isPowerOf(uint32_t num, uint32_t base) {
	if (base == 1)
		return num == 1;

	uint64_t power = 1;
	while (power < num)
		power *= base;

	return power == num;
}

/* ============================================================
 * Returns the address of the datablock bitmap for the 
 * specified block group.
 *
 * Parameters:
 * 	grpNum - the number of the blockgroup to get the bitmap for.
 * 
 * Returns:
 * 	Returns a 64-bit address for the bitmap.
 * ========================================================= */
uint64_t getBitmapAddr(uint32_t grpNum) {
	uint32_t numDescPerBlock = blockSize / GRP_DESC_SIZE;
	uint64_t grpDescAddr = partition_addr + blockSize;
	uint8_t descriptors[blockSize];

	// if group descriptor table is more than 1 block
	// be sure to read the correct block
	if (numDescPerBlock > grpNum) 
		grpDescAddr += (blockSize * (grpNum / numDescPerBlock));
	safeRead(deviceID, grpDescAddr, descriptors, blockSize);
	
	uint32_t offset = grpNum * GRP_DESC_SIZE;

	// the first 4 bytes of the group descriptor 
	// is the bitmap block number
	uint32_t bitmapBlockNum = *((uint32_t*)(descriptors + offset));
	return partition_addr + ((uint64_t)bitmapBlockNum * (uint64_t)blockSize);
}

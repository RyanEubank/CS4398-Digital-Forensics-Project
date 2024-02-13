#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynamicArray.h"
#include "recover.h"
#include "safeio.h"
#include "scan.h"
#include "mbr.h"

#if defined(_DEBUG) || defined(DEBUG)
#include "debug.h"
#endif

#define ISO_SIGNATURE "CD001"	// volume descriptor signature

// global variables
int32_t deviceID = 0;
uint64_t partition_addr = 0;
uint32_t blockSize = 0;
uint32_t totalBlocks = 0;

// local
dynamicArray* firstBlocks;
dynamicArray* indirectBlocks;
dynamicArray* recoveredBlocks;
uint64_t volumeSize = 0;

// defines the type of entry the dynamic arrays will contain
typedef struct {
	uint64_t m_addr; 		// the address of the block
	uint32_t m_blockNum; 	// its block number
	uint32_t m_size; 		// the number of data blocks it points to
} blockEntry;

int32_t isIndirectBlock(const uint32_t*, uint32_t);
int32_t verifyTrailingZeroes(const uint32_t*, const uint32_t*);
void mapBlocks(const uint8_t*, uint64_t, uint32_t);
uint32_t isLikelyFirstBlock(const uint8_t*, uint64_t);
uint32_t hasISOSignature(const uint8_t*);
uint64_t mapSize(const uint8_t*);
void printMatches();
void printMatch(const blockEntry*);
void forEachBlock(dynamicArray*, void (*) (const blockEntry*));
void recover(const blockEntry*);
void recoverIndirectBlocks(uint32_t);
uint32_t recoverIndirectFor(uint32_t, uint32_t*);
uint32_t addBlocksFrom(const uint8_t*);
void writeRecoveredFile();
void writeBlocks(int32_t);
void recordVolumeSize();

/* ============================================================
 * Performs file carving looking for the first block of deleted
 * files then pieces together all of its indirect blocks to
 * recover remaining data blocks.
 * 
 * Parameters:
 * 	device - the file descriptor of the device to read from.
 *  index - the index of the partition in the partition table.
 *  scanType - flag indicating which type of blocks to map.
 * ========================================================= */
void recoverFiles(int32_t device, int32_t index, uint32_t scanType) {
	deviceID = device;
	init(&firstBlocks, 1000, sizeof(blockEntry));
	init(&indirectBlocks, 10000, sizeof(blockEntry));

	uint64_t addr = scanPartitionAndProcess(
		index, mapBlocks, scanType
	);

	// if invalid partition do nothing
	if (addr) {
		printMatches();
		init(&recoveredBlocks, 100000, sizeof(blockEntry));
		printf("\nBeginning Recovery Process...\n\n");
		forEachBlock(firstBlocks, recover);
	}

	free(firstBlocks);
	free(indirectBlocks);
}

/* ============================================================
 * Attempts to find and recover corrseponding data blocks to
 * the given first block entry if it has a high likelihood
 * of being an actual first block match.
 * 
 * Parameters:
 * 	firstBlock - the first block entry to perform file carving on.
 * ========================================================= */
void recover(const blockEntry* firstBlock) {
	printf("First Block Recovered: %d\n", firstBlock->m_blockNum);

	// assume first 12 direct pointers are contiguous and
	// add to recovered list
	for (uint32_t i = 0; i < 12; i++) {
		blockEntry block = {
			firstBlock->m_addr + (i * blockSize), 
			firstBlock->m_blockNum + i, 
			0
		};
		addItem(&recoveredBlocks, &block);
	}

	// now go through indirect blocks and try to match
	// them in sequence assuming the first address
	// pointed to follows from the previous, etc.
	uint32_t next = firstBlock->m_blockNum + 12;
	recoverIndirectBlocks(next);
	writeRecoveredFile();

	// clear list fr next recovered file
	//free(recoveredBlocks);
	//init(&recoveredBlocks, 100000, sizeof(blockEntry));
}

/* ============================================================
 * Attempts to find and recover corrseponding data blocks in
 * the indirect blocks of a file given the next expected block
 * after the first 12 direct pointers.
 * 
 * Parameters:
 * 	nextBlock - the next expected block number to be found
 *              in the single indirect pointer.
 * ========================================================= */
void recoverIndirectBlocks(uint32_t nextBlock) {
	uint32_t lastEntry = 0; // next expected block number across ptrs

	printf("Recovering single indirect pointer: ");
	recoverIndirectFor(nextBlock, &lastEntry);
	printf("Recovering double indirect pointer: ");
	recoverIndirectFor(lastEntry + 1, &lastEntry);
	printf("Recovering triple indirect pointer: ");
	recoverIndirectFor(lastEntry + 1, &lastEntry);
}

/* ============================================================
 * Recovers data blocks from the next indirect block by scanning
 * all of the mapped indirect blocks for a block address that 
 * matches the next anticipated block number.
 * 
 * Parameters:
 * 	firstBlock - the first block entry to perform file carving on.
 *  lastEntry - output parameter for the block number of the last 
 * 				indirect block read in the recursive tree traversal.
 * 
 * Returns:
 * 	Returns the block number of the indirect block pointing to
 *  the given block number, and the last data block entry
 * ========================================================= */
uint32_t recoverIndirectFor(uint32_t nextBlockNum, uint32_t* lastEntryOut) {
	if (nextBlockNum == 1)
		return 0; // end of the line, file has nore more blocks

	uint32_t containsBlock;
	uint32_t isInJournal;
	uint8_t buffer[blockSize];

	for (uint32_t i = 0; i < indirectBlocks->m_numItems; i++) {
		blockEntry* entry = getItem(&indirectBlocks, i);
		safeRead(deviceID, entry->m_addr, buffer, blockSize);

		// check for expected block number at first entry
		// and that the indirect block is not stored in the journal
		// within the first block group
		containsBlock = *((uint32_t*)buffer) == nextBlockNum;
		isInJournal = entry->m_blockNum < (blockSize << 3);

		if (containsBlock && !isInJournal) {
			// if this is the correct indirect block then
			// recursively travserse back up to the root of the indirect
			// tree and add all data blocks with depth first traversal
			if (!recoverIndirectFor(entry->m_blockNum, lastEntryOut)) {
				printf("Found -> %d\n", entry->m_blockNum);
				printf("Mapping data blocks...");
				fflush(stdout);
				*lastEntryOut = addBlocksFrom(buffer);
				printf("\r                        ");
				printf("\rComplete!\n");
			}

			return entry->m_blockNum;
		}
	}
	return 0;
}

/* ============================================================
 * Traverses the tree structure of indirect blocks starting
 * from the given buffer as root, adding all data blocks (leafs)
 * found to the list of recovered blocks. Performs pre-order,
 * depth-first traversal so data blocks will be added in the
 * correct order.
 * 
 * Parameters:
 * 	block - the contents of the indirect block in the tree, 
 *           or the root.
 * 
 * Returns:
 * 	Returns the last block address in the last indirect block
 *  traversed.
 * ========================================================= */
uint32_t addBlocksFrom(const uint8_t* block) {
	uint8_t buffer[blockSize];
	const uint32_t* start = (uint32_t*) block;
	const uint32_t* end = (uint32_t*) (block + blockSize);

	for (const uint32_t* i = start; i < end; i++) {
		uint64_t nextBlockNum = (uint64_t) *i;

		if (nextBlockNum) {
			// if the next block number is not zero then read it
			// and add to list
			uint64_t addr = partition_addr + 
				(nextBlockNum * (uint64_t) blockSize);
			safeRead(deviceID, addr, buffer, blockSize);

			// if this entry is also an indirect block then add its blocks
			if (isIndirectBlock((uint32_t*)buffer, blockSize >> 2)) {
				uint32_t lastEntry = addBlocksFrom(buffer);

				// return the last entry from the final recursive call
				if (i + 1 == end) 
					return lastEntry; 
			}
			// otherwise write this block to the recovered list
			else {
				blockEntry block = {addr, nextBlockNum, blockSize};
				addItem(&recoveredBlocks, &block);
			}
		}
	}

	// return the last entry in the last item so we know what
	// block number should follow from this one for the next double
	// or triple indirect pointer
	return *(end - 1);
}

/* ============================================================
 * Prints each likely first block found during scan along
 * with what signatures were located.
 * ========================================================= */
void printMatches() {
	printf("Total First Block Matches: %d\n", firstBlocks->m_numItems);
	printf("Indirect Block Count: %d\n", indirectBlocks->m_numItems);
	printf("\nListing potential starting blocks for recovered files.\n");
	forEachBlock(firstBlocks, printMatch);
}

/* ============================================================
 * Prints information about a first block match.
 *
 * Parameters:
 * 	item - the entry to print information on.
 * ========================================================= */
void printMatch(const blockEntry* item) {
		if (item->m_size) {
			printf("\n[High Liklihood]: ----------------------\n");
			printf("Address      - %0lx\n", item->m_addr);
			printf("Block Number - %d\n", item->m_blockNum);
			printf("----------------------------------------\n");
		}
}

/* ============================================================
 * Loops through each block in the given dynamic array and
 * processes each with the function pointer func.
 * 
 * Parameters:
 *	blocks - the list of blocks to process.
 *	func - the function to process each with.
 * ========================================================= */
void forEachBlock(
	dynamicArray* blocks, 
	void (*func) (const blockEntry*)
) {
	for (uint32_t i = 0; i < blocks->m_numItems; i++) {
		blockEntry* block = getItem(&blocks, i);
		func(block);
	}
}

/* ============================================================
 * Writes the recovered blocks to a new file, prompts the user
 * for the file path and name.
 * ========================================================= */
void writeRecoveredFile() {
	char* response = NULL;
	int32_t flags = O_WRONLY | O_CREAT;
	int32_t permissions = S_IRUSR | S_IWUSR;
	int32_t file = -1;

	printf("\nFile Recovered!\n");
	printf("Recovered %d blocks.\n", recoveredBlocks->m_numItems);
	printf("\nWrite recovered file back to new location? (y/n)");

	// determine if user wants to recover file
	readUserInput(&response);
	if (strcmp(response, "y") == 0) {

		// get the output location
		do {
			printf("Please enter full recovery path: ");
			readUserInput(&response);
			file = open(response, flags, permissions);

			if (file < 0) {
				fprintf(stderr, "Failed to open file path.\n");
				fprintf(stderr, "Please try again.\n");
			}

		} while(file < 0);

		// write the recovered blocks to a new file
		writeBlocks(file);
	}

	free(response);
}

/* ============================================================
 * Writes the recovered blocks to the given file descriptor
 * reading/writing out one at a time.
 * ========================================================= */
void writeBlocks(int32_t outFile) {
	uint8_t buffer[blockSize];
	uint64_t sizeWritten = 0;
	uint32_t current_progress = 0;

	recordVolumeSize();
	printf("Writing data to file...\n");

	for (uint32_t i = 0; i < recoveredBlocks->m_numItems; i++) {
		printProgress(&current_progress, i, recoveredBlocks->m_numItems);

		blockEntry* block = getItem(&recoveredBlocks, i);
		safeRead(deviceID, block->m_addr, buffer, blockSize);

		// for the very last block trim the end to match
		// the actual file by reading primary volume descriptor
		// for volume size.
		if (i + 1 == recoveredBlocks->m_numItems) {
			uint64_t difference = volumeSize - sizeWritten;

			if (difference > 0 && difference < blockSize) {
				safeWrite(outFile, buffer, difference);
				sizeWritten += difference;
				break;
			}
		}

		safeWrite(outFile, buffer, blockSize);
		sizeWritten += (uint64_t)blockSize;
	}
	printf("\nWrote %ld total bytes.\n", sizeWritten);
}

/* ============================================================
 * Reads the primary vlume descriptor of the recovered file
 * to get the file volume size recorded in the file header.
 * ========================================================== */
void recordVolumeSize() {
	blockEntry* first = getItem(&recoveredBlocks, 0);
	uint64_t primaryDescAddr = first->m_addr + 0x8000;
	uint8_t buffer[2048]; // vol. desc are always 2048 bytes
	safeRead(deviceID, primaryDescAddr, buffer, 2048);

	uint32_t offsetVolSize = 80;
	uint32_t offsetBlockSize = 128;
	uint32_t logicalSizeInBlocks = *(uint32_t*)(buffer + offsetVolSize);
	uint16_t logicalBlockSize = *(uint16_t*)(buffer + offsetBlockSize);

	printf("Volume Space Size: 0x%x\n", logicalSizeInBlocks);
	printf("Logical Block Size: 0x%x\n", logicalBlockSize);
	volumeSize = (uint64_t)logicalSizeInBlocks 
		* (uint64_t)logicalBlockSize;
}

/* ============================================================
 * Maps out each block that is either a potential first block
 * for a deleted file adding it to a list, or adds a tuple
 * of (block number, total size pointed to) to a different list
 * in preperation for putting data blocks back together.
 * 
 * Parameters:
 * 	buffer - the buffer holding block data.
 *  addr - the address the block was read from.
 *  blockNum - the block number being mapped.
 * ========================================================= */
void mapBlocks(
	const uint8_t* buffer, 
	uint64_t addr,
	uint32_t blockNum
) {
	uint32_t isMatch = isLikelyFirstBlock(buffer, addr);
	if (isMatch) {
		// direct block - m_size field contains flag for
		// which header was found - either MBR or volume
		// descriptor or both.
		blockEntry first = {addr, blockNum, isMatch};
		addItem(&firstBlocks, &first);

	} else if (isIndirectBlock((uint32_t*)buffer, blockSize >> 2)) {
		// skip mapping size, may be useful optimization later
		// but recovery works without
		uint32_t size = 1;
		// map out how many data blocks the indirect block points to
		// uint32_t size = mapSize(buffer);

		blockEntry indirect = {addr, blockNum, size};
		addItem(&indirectBlocks, &indirect);
	}
}

/* ============================================================
 * Determines whether or not the given block is an indirect
 * file block based on heuristics; indirect blocks will store
 * many consecutive block numbers every 4 bytes, and any empty
 * space after will be zeroed.
 * 
 * Parameters:
 * 	block - a buffer containing the contents of the block to check
 *  size - the size of the block in 4-byte ints
 * 
 * Returns:
 * 	returns a 1 if the block is likely an indirect block,
 *  or 0 if not.
 * ========================================================= */
int32_t isIndirectBlock(const uint32_t* block, uint32_t size) {
	uint32_t consecutiveNums = 0;
	uint32_t blockAddress = *block;

	if (blockAddress == 0 || blockAddress > totalBlocks)
		return 0;

	// go through the first ~5 or so addresses
	for (uint32_t i = 1; i < 6; i++) {
		const uint32_t* addr = block + i;

		if (*addr > totalBlocks)
			return 0;

		// check for consecutive block address
		else if (*addr == blockAddress + 1) {
			blockAddress++;
			consecutiveNums++;
		}

		// if address is zero then the remainder of the block
		// must also be zero (if indirect block is only partially filled)
		else if (*addr == 0) 
			return verifyTrailingZeroes(addr, block + size);

		// if a few consequtive addresses were already found
		// then assume fragmentation in indirect block
		else if (consecutiveNums >= 3)
			return 1;

		// otherwise zero the count and keep going
		else consecutiveNums = 0;
	}
	return consecutiveNums > 3;
}

/* ============================================================
 * Returns whether the given address range is completely zeroed
 * or if other non-zero bytes are found.
 * 
 * Parameters:
 * 	start - The start address of the range.
 *  end - The end address of the range.
 * 
 * Returns:
 * 	returns a 1 if the range is zeroed, 0 if non-zeros found.
 * ========================================================= */
int32_t verifyTrailingZeroes(
	const uint32_t* start, 
	const uint32_t* end
) {
	const uint32_t* current = start;
	while (current < end) {
		if (*(current++) != 0) {
			return 0;
		}
	}
	return 1;
}

/* ============================================================
 * Returns whether the given block is likely the starting block
 * for an .iso file. This is the only function that would
 * change for recovering other file types.
 * 
 * Parameters:
 * 	block - a buffer containing the data read in from the block
 * 	blockSize - the size of the buffer
 *  addr - the address the buffer was read from
 * 
 * Returns:
 * 	- returns a bitflag in the 1 position if one
 *    indicator is found
 * 
 * 	- returns a bitflag in the 2 position if a different
 *    indicator is found
 * 
 * - returns a 1 if the block is very likely to be a
 *   first block match 0 otherwise.
 * ========================================================= */
uint32_t isLikelyFirstBlock(const uint8_t* block, uint64_t addr) {
	uint8_t buf[blockSize];
	safeRead(deviceID, addr + 0x8000, buf, blockSize);

	// look for an MBR
	pMbr mbr = allocateMBR();
	extractMBR(block, mbr);
	uint32_t hasMBR = mbr->_mbr_signature == MBR_SIGNATURE;
	
	// assume first 32k bytes are contiguous and
	// find the primary volume descriptor
	uint32_t isDescriptor = hasISOSignature(buf);
	uint32_t isPrimaryDesc = isDescriptor && (*buf == 0x01);

	return isPrimaryDesc || (isDescriptor && hasMBR);
}

/* ============================================================
 * Returns whether the given block has a volume descriptor
 * header at byte 1.
 * 
 * Parameters:
 * 	block - a buffer containing the data read in from the block.
 * 
 * Returns:
 * 	- returns a 1 if the block has the signature, 0 otherwise.
 * ========================================================= */
uint32_t hasISOSignature(const uint8_t* block) {
	const char* location = (const char*) &block[1];
	return (strncmp(location, ISO_SIGNATURE, 5) == 0);
}

/* ============================================================
 * Returns the total size of the data blocks pointed to by the
 * provided indirect block.
 * 
 * Parameters:
 * 	block - a buffer containing data from the indirect block.
 * 
 * Returns:
 * 	- returns a 1 if the block has the signature, 0 otherwise.
 * ========================================================= */
uint64_t mapSize(const uint8_t* block) {
	uint32_t sizeInInts = blockSize >> 2;
	uint32_t* current = (uint32_t*)block;
	uint32_t* end = (uint32_t*)block + sizeInInts;
	uint64_t mappedSize = 0;

	// buffer to read block pointed to and address of first entry
	// in this indirect block
	uint8_t buffer [blockSize];
	uint64_t nextAddr = partition_addr + ((*current) * blockSize);

	// go through each block address in the indirect block
	// and read/map that block
	while (current < end && *current++) {
		safeRead(deviceID, nextAddr, buffer, blockSize);

		// if another indirect is found 
		// (meaning this one is a double/triple)
		// then map that block's size and add to this size
		if (isIndirectBlock((uint32_t*)buffer, blockSize >> 2))
			mappedSize += mapSize(buffer);
		else
			mappedSize += blockSize;

		nextAddr += blockSize;
	}
	return mappedSize;
}
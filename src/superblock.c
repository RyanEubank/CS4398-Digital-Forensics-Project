#include <stdlib.h>
#include <stdio.h>

#include "safeio.h"
#include "superblock.h"

/* ============================================================
 * Reads the given device to obtain superblock data from the
 * specified partition.
 * 
 * Arguments:
 * 	device - the descriptor of the device to read from.
 *  partAddr - the address of the superblock to read from.
 * 
 * Returns:
 * 	Returns a newly allocated pointer to a superblock struct.
 * ========================================================= */
pSBlock readSuperblock(int32_t device, uint64_t partAddr) {
	pSBlock superblock = (pSBlock) malloc(sizeof(SuperBlock));
	if (superblock == NULL)
		exit_err("Failed to allocate Superblock");

	safeRead(device, partAddr, (uint8_t*)superblock, 1024);
	return superblock;
}

/* ============================================================
 * Reads the given device to obtain superblock data from the
 * specified partition and prints the data. Displays an error
 * if the superblock is invalid, i.e Not a linux partition.
 * 
 * Arguments:
 * 	device - the descriptor of the device to read from.
 *  index - the number of the partition to read.
 * ========================================================= */
void printSuperblock(int32_t device, uint32_t index) {
	printf("PRINT SB\n");
	printf("Not yet implemented.\n");
}

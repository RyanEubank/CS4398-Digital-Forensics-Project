#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "safeio.h"

/* ============================================================
 * Returns whether the given address range is completely zeroed
 * or if other non-zero bytes are found.
 * 
 * Parameters:
 * 	block - the buffer to print
 *  size - the size of the buffer in ints
 *  addr - the address the buffer was read from
 * ========================================================= */
void printBlock(uint32_t* block, uint32_t size, uint64_t addr) {
	for (uint32_t k = 0; k < size; k++) {
		// print new line every 8th value (every 64 bytes)
		if (k % 4 == 0)
			printf("\n0x%0lx | ", addr + (k << 2));
		printf("0x%08x ", *(block + k));
	}
	printf("\n\n");
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
void awaitUser() {
	char* line = NULL;
	readUserInput(&line);
	free(line);
}
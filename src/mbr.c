#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "mbr.h"
#include "safeio.h"

#define SECTOR_SIZE 512
#define DISK_SIGNATURE_OFFSET 440
#define NULL_PAD_OFFSET 444
#define PART_TABLE_OFFSET 446
#define MBR_SIGNATURE_OFFSET 510

#define BLOCK_SIZE_OFFSET 24

/* ============================================================
 * Allocates space and returns a pointer to an mbr struct.

 * Returns:
 * 	Returns a newly allocated pointer to an mbr struct.
 * ========================================================= */
pMbr allocateMBR() {
	pMbr mbr = (pMbr)malloc(sizeof(struct _mbr));
	if (mbr == NULL)
		exit_err("Failed to allocate MBR");
	return mbr;
}

/* ============================================================
 * Reads the specified device and scans the master boot record
 * to retrieve and return a struct containing the mbr info.
 * 
 * Parameters:
 *	device_id - an open file descriptor to the device.
 *
 * Returns:
 * 	Returns a newly allocated pointer to an mbr struct
 *  containing the scanned info.
 * ========================================================= */
pMbr readMBR(int32_t device_id) {
	uint8_t mbr_buffer[SECTOR_SIZE];
	safeRead(device_id, 0, mbr_buffer, SECTOR_SIZE);
	pMbr mbr = allocateMBR();
	extractMBR(mbr_buffer, mbr);
	return mbr;
}

/* ============================================================
 * Copies the information from the buffer into an allocated
 * MBR struct.
 * 
 * Parameters:
 * 	buffer - the buffer read from the device
 * 	mbr - a pointer to an mbr struct to copy the fields to
 * ========================================================= */
void extractMBR(const uint8_t* buffer, pMbr mbr) {
	// must copy field by field because byte alignment
	// padding makes mbr struct > 512 bytes
	// cannot cast the buffer pointer directly
	memcpy(
		&mbr->_boot_code, 
		&buffer, 
		sizeof(mbr->_boot_code)
	);
	memcpy(
		&mbr->_disk_signature, 
		&buffer[DISK_SIGNATURE_OFFSET], 
		sizeof(mbr->_disk_signature)
	);
	memcpy(
		&mbr->_null_padding, 
		&buffer[NULL_PAD_OFFSET], 
		sizeof(mbr->_null_padding)
	);
	memcpy(
		&mbr->_partition_table, 
		&buffer[PART_TABLE_OFFSET], 
		sizeof(mbr->_partition_table)
	);
	memcpy(
		&mbr->_mbr_signature, 
		&buffer[MBR_SIGNATURE_OFFSET], 
		sizeof(mbr->_mbr_signature)
	);
}

/* ============================================================
 * Reads the given partition table to calculate the address
 * given by the LBA.
 * 
 * Arguments:
 * 	partitionTable - a pointer to a parition table struct
 * 
 * Returns:
 * 	Returns a 64 bit address for the start of the partition,
 *  or 0 if the partition entry is null in the MBR (partition
 *  does not exist) or index out of range.
 * ========================================================= */
uint64_t getPartAddr(const pMbr mbr, int32_t index) {
	if (index < 0 || index > 3)
		return 0;

	pTable partition = &mbr->_partition_table[index];
	if (partition->_lba == 0) 
		return 0;

	// lba already in correct byte order
	// ntohl not required
	return partition->_lba * SECTOR_SIZE;
}

/* ============================================================
 * Prints information from the boot sector of the given device.
 * Displays an error if the MBr block is invalid.
 * 
 * Arguments:
 * 	device - descriptor of the device to read from.
 * ========================================================= */
void printMBR(int32_t device) {
	printf("PRINT MBR\n");
	printf("Not yet implemented.\n");
}
#ifndef MBR_H
#define MBR_H

#include <stdint.h>

#define SECTOR_SIZE 512
#define MBR_SIGNATURE 0xaa55

typedef struct _partition_table {
	uint8_t _boot_flag;
	uint8_t _chs_begin[3];
	uint8_t _type_code;
	uint8_t _chs_end[3];
	uint32_t _lba;
	uint32_t _num_sectors;
} PartitionTable, *pTable;

typedef struct _mbr {
	uint8_t _boot_code[440];
	uint32_t _disk_signature;
	uint16_t _null_padding;
	struct _partition_table _partition_table[4];
	uint16_t _mbr_signature;
} Mbr, *pMbr;

pMbr allocateMBR();
pMbr readMBR(int32_t);
void extractMBR(const uint8_t*, pMbr);
uint64_t getPartAddr(pMbr, int32_t);
void printMBR(int32_t);


#endif
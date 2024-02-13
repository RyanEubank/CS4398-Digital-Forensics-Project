#ifndef SCAN_H
#define SCAN_H

#include <stdint.h>

#define ALL_BLOCKS 1 << 0
#define ALLOCATED_ONLY 1 << 1
#define UNALLOCATED_ONLY 1 << 2

typedef void (*process)(const uint8_t*, uint64_t, uint32_t);
uint64_t scanPartitionAndProcess(int32_t, process, uint32_t);

extern int32_t deviceID;
extern uint64_t partition_addr;
extern uint32_t blockSize;
extern uint32_t totalBlocks;

#endif
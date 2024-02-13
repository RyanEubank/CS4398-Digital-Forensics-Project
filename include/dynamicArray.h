#ifndef DYN_ARR_H
#define DYN_ARR_H

#include <stdint.h>

typedef struct {
	uint64_t m_capacity;
	uint32_t m_numItems;
	uint32_t m_elementSize;
	void* m_buffer;
} dynamicArray;

void init(dynamicArray** arr, uint32_t count, uint32_t elementSize);
void addItem(dynamicArray** arr, void* item);
void* getItem(dynamicArray** arr, uint32_t index);

#endif

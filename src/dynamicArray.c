#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "dynamicArray.h"
#include "safeio.h"

#define FAILED_ALLOC "Failed to allocate memory.\n"

void ensureCapacity(dynamicArray** arr) {
	uint32_t currentSize = (*arr)->m_numItems * (*arr)->m_elementSize;
	if (currentSize == (*arr)->m_capacity) {
		dynamicArray* copy = NULL;

		uint32_t sizeBytes = offsetof(dynamicArray, m_buffer) 
			+ (*arr)->m_capacity;

		init(&copy, (*arr)->m_numItems << 1, (*arr)->m_elementSize);

		// copy new capacity before overwriting
		uint64_t capacity = copy->m_capacity;
		memcpy(copy, *arr, sizeBytes);
		copy->m_capacity = capacity;
		
		free(*arr);
		*arr = copy;
	}
}

void init(dynamicArray** arr, uint32_t count, uint32_t elementSize) {
	uint32_t size = offsetof(dynamicArray, m_buffer) 
		+ (count * elementSize);

	*arr = malloc(size);
	if (!(*arr))
		exit_err(FAILED_ALLOC);
	memset(*arr, 0, size);

	(*arr)->m_elementSize = elementSize;
	(*arr)->m_numItems = 0;
	(*arr)->m_capacity = (uint64_t)count * (uint64_t)elementSize;
}

void addItem(dynamicArray** arr, void* item) {
	ensureCapacity(arr);
	void* address = getItem(arr, (*arr)->m_numItems);

	if (*(uint8_t*)address != 0) {
		fprintf(stderr, "Overwriting array memory!");
		exit(EXIT_FAILURE);
	}

	memcpy(address, item, (*arr)->m_elementSize);
	(*arr)->m_numItems++;
}

void* getItem(dynamicArray** arr, uint32_t index) {
	return ((uint8_t*)(&(*arr)->m_buffer)) + (index * (*arr)->m_elementSize);
}
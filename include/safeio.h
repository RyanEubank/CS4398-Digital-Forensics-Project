#ifndef SAFEIO_H
#define SAFEIO_H

#include <stdint.h>
#include <fcntl.h>

void exit_err(const char*);
int32_t safeOpen(const char*, int32_t, int32_t);
void safeSeek(int32_t, uint64_t, int32_t);
void safeRead(int32_t, uint64_t, uint8_t*, uint32_t);
void safeWrite(int32_t, uint8_t*, uint32_t);
void readUserInput(char**);
void printProgress(	uint32_t*, uint32_t, uint32_t);

#endif
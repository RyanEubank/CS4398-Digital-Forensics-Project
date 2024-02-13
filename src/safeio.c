#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "safeio.h"

/* ============================================================
 * Prints a custom error message based on errno and
 * exits the program with a failure code.
 * 
 * Parameters:
 *	msg - the error message to print.
 * ========================================================= */
void exit_err(const char* msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

/* ============================================================
 * Opens the specified device for read-only use. If the device
 * cannot be opened an error is displayed and the program exits.
 * 
 * Parameters:
 *	device - the path to the device to open and read.
 *  flags - file open mode (read, write, or both)
 *  mode - specified only if new file is created in flags
 * 
 * Returns:
 * 	returns the file descriptor after successfuly opening 
 * 	the device.
 * ========================================================= */
int32_t safeOpen(const char* device, int32_t flags, int32_t mode) {
	int32_t device_id = open(device, flags, mode);
	if (device_id < 0)
		exit_err("Failed to open device");
	return device_id;
}

/* ============================================================
 * Reads a block from the device starting from the given
 * address offset into a character array. Exits the program if
 * there is an error.
 * 
 * Arguments:
 * 	device_id - the file descriptor to read from
 *  addr - the address offset to read from
 * 	buffer - the character buffer to read data into
 *  size - the number of bytes to read from the device
 * ========================================================= */
void safeRead(
	int32_t device_id, 
	uint64_t addr, 
	uint8_t* buffer,
	uint32_t size
) {
	safeSeek(device_id, addr, SEEK_SET);
	if (read(device_id, buffer, size) < 0)
		exit_err("Failed to read device");
}

/* ============================================================
 * Writes the given buffer to the specified file, checking for
 * write errors.
 * 
 * Arguments:
 * 	file - the file descriptor to write to
 * 	buffer - the character buffer to write data from
 *  size - the number of bytes to write
 * ========================================================= */
void safeWrite(int32_t file, uint8_t* buffer, uint32_t size) {
	if (write(file, buffer, size) < 0)
		exit_err("Failed to write recovered file.");
}

/* ============================================================
 * Sets the file offset on the given descriptor to the
 * specified address from the starting location 'whence'
 * Exits the program if the call fails.
 * 
 * Arguments:
 * 	device_id - the file descriptor to set
 *  addr - the address offset to go to
 * 	whence - the relative location to calculate the address from
 * ========================================================= */
void safeSeek(int32_t device_id, uint64_t addr, int32_t whence) {
	if (lseek(device_id, addr, whence) < 0) {
		fprintf(stderr, "Bad Address: 0x%lx", addr);
		exit_err("Failed to reposition read location offset");
	}
}

/* ============================================================
 * Reads a line of input from stdin, checking for syscall 
 * failure before returning.
 * 
 * Arguments:
 * 	line - address to the character buffer to read input into.
 * ========================================================= */
void readUserInput(char** line) {
	size_t n; // allocated size of buffer
	int32_t read;

	// read input and check getline success
	if ((read = getline(line, &n, stdin)) == -1)
		exit_err("Failed to read input");

	// remove newline character from the end
	(*line)[read - 1] = '\0';
}

/* ============================================================
 * Prints the current operation's progress as a percentage in
 * increments of 1% - only the next 1% progress will result
 * in printing to stdout to minimize printing for every block
 * processed.
 * 
 * Parameters:
 * 	current - pointer to the current progress
 *  index - the current block being scanned
 *  total - the total number of blocks to scan
 * ========================================================= */
void printProgress(
	uint32_t* current, 
	uint32_t index, 
	uint32_t total
) { 
	uint32_t percent = ((double)index / (double)total) * 100;
	if (percent > *current) {
		*current = percent;
		printf("\rPercent done: %d%%", *current);
		if (fflush(stdout) != 0)
			exit_err("Failed to flush stdout");
	}
}
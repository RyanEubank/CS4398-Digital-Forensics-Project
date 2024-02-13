#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbr.h"
#include "recover.h"
#include "safeio.h"
#include "superblock.h"

#define USAGE "Usage: ./scan_drive.exe </dev/sdx> [options]\n"
#define HELP_MSG "Try ./scan_drive.exe -help for more info.\n"

enum Process {
	PRINT_MBR,
	PRINT_SUPERBLOCK,
	RECOVER
};

enum Process flag;

uint32_t validateArgs(uint32_t, const char**);
uint32_t validateOptions(const char**);
uint32_t getPartitionIndex(const char*);
void scan_drive(const char**);
uint32_t getScanType(const char**);
void printHelp();

/* ============================================================
 *							MAIN
 * ========================================================= */
int32_t main(int32_t argc, const char** argv) {
	if (!validateArgs(argc, argv)) {
		fprintf(stderr, USAGE);
		fprintf(stderr, HELP_MSG);
		exit(EXIT_FAILURE);
	}
	scan_drive(argv);
	return EXIT_SUCCESS;
}

/* ============================================================
 * Checks that the arguents provided to the program were
 * valid. If invalid a usage error is printed to stdout and
 * the program exits.
 * 
 * Arguments:
 * 	argc - the number of arguments given to the program.
 *  argv - string array containing the command line arguments
 * 
 * Returns:
 * 	returns 1 if the arguments are valid, zero otherwise.
 * ========================================================= */
uint32_t validateArgs(uint32_t argc, const char** argv) {
	if (argc < 2)
		return 0;

	if (strncmp(argv[1], "-help", 5) == 0) {
		printHelp();
		exit(EXIT_SUCCESS);
	}

	// first program argument must be of the 
	// form '/dev/sdx' or '/dev/sdxx'
	int32_t isDevice = strlen(argv[1]) > 7
		&& strncmp("/dev/sd", argv[1], 7) == 0;

	if (isDevice != 1) {
		fprintf(stderr, "Unrecognized device name: %s\n", argv[1]);
		return 0;
	}

	// any remaining arguments must specify an option of
	// the form '-x'
	return validateOptions(argv);
}

/* ============================================================
 * Prints a help message to the console.
 * ========================================================= */
void printHelp() {
	printf("scan_drive.exe is a program designed to read any block\n");
	printf("device to obtain info on its MBR and ext partitions to recover files.\n");
	printf("\n ----------------------------- OPTIONS -----------------------------\n");
	printf("r - scans the drive to try and reconstruct and recover deleted files.\n\n");
	printf("    Currently only works with .iso files.\n");
	printf("    Any files found will prompt the user for where to write the recovery to.\n");
	printf("    Additionally, user can specify a scan type argument as follows:\n");
	printf("    'all' - scans all blocks during recovery,\n"); 
	printf("    'free' - scans only unallocated blocks,\n");
	printf("    'used' - scans only already allocated blocks.\n\n");
	printf("p - prints info on the MBR or superblock.\n\n");
	printf("    Must specify either type as 'mbr' or 'sb' for which to print as an argument.\n");
	printf("    Example: $ ./scan_drive.exe /dev/sdx -p mbr\n");
	printf("    Will print MBR info.\n\n");
}

/* ============================================================
 * Checks the remaining arguments after the device name to
 * ensure program options were passed in correctly.
 * 
 * Parameters:
 *  argv - string array containing the command line arguments
 * 
 * Returns:
 *  returns a 1 if optional arguments are valid (or don't exist)
 *  returns a 0 otherwise.
 * ========================================================= */
uint32_t validateOptions(const char** argv) {
	flag = RECOVER; //default option

	if (argv[2] != NULL) {
		if (strncmp(argv[2], "-r", 2) == 0)
			return 1;
		if (strncmp(argv[2], "-p", 2) == 0) {
			if (argv[3] == NULL)
				fprintf(stderr, "Unrecognized print argument.\n");
			else if (strncmp(argv[3], "mbr", 3) == 0)
				flag = PRINT_MBR;
			else if (strncmp(argv[3], "sb", 2) == 0)
				flag = PRINT_SUPERBLOCK;
			else 
				return 0;
			return 1;
		}
	}
	return 1;
}

/* ============================================================
 * Reads the arguments passed to the program after validation
 * to open the device and parses program options to call the
 * appropriate module.
 * 
 * Parameters:
 * 	argv - the array of string arguments passed to the program.
 * ========================================================= */
void scan_drive(const char** argv) {

	// obtain the pathname to the full device 
	// (ignoring partition # if included)
	// ex: truncate /dev/sdb1 to /dev/sdb
	char deviceName[8];
	memset(deviceName, 0, sizeof(deviceName));
	strncpy(deviceName, argv[1], 8);

	int32_t device = safeOpen(deviceName, O_RDONLY, 0);
	int32_t index = (flag == PRINT_MBR) 
		? 0
		: getPartitionIndex(argv[1]);

	// perform processsing based on program arguments
	switch(flag) {
	case (PRINT_MBR):
		printMBR(device);
		break;
	case (PRINT_SUPERBLOCK):
		printSuperblock(device, index);
		break;
	case (RECOVER):
		uint32_t scanType = getScanType(argv);
		recoverFiles(device, index, scanType);
		break;
	};

	close(device);
}

/* ============================================================
 * Parses the name of the specified device to detemine
 * which partition to open and read. If no partition is
 * specified then the first primary partition is returned
 * by default.
 * 
 * Parameters:
 * 	deviceName - the path name to the device passed in the
 * 		program arguments.
 * 
 * Returns:
 * 	The index of the specified partition or 0 by default if not 
 *  given.
 * ========================================================= */
uint32_t getPartitionIndex(const char* deviceName) {
	if (deviceName[8] == '\0') {
		printf("No partition specified - Reading from Partition 1.\n");
		return 0;
	} else {
		uint32_t part_num = atoi(&deviceName[8]);
		printf("Reading from partition %d.\n", part_num);
		return part_num - 1;
	}
}

/* ============================================================
 * Parses the type of scan to perform from the program arguments.
 * 
 * Parameters:
 * 	deviceName - the path name to the device passed in the
 * 	argv - the program arguments passed on the command line.
 * 
 * Returns:
 * 	A flag inidicating what type of scan should be done:
 * 	- ALL_BLOCKS will scan and include every block in recovery
 *  - ALLOCATED_ONLY will skip free (unallocated) blocks
 *  - UNALLOCATED_ONLY will skip used (allocated) blocks
 * ========================================================= */
uint32_t getScanType(const char** argv) {
	// default to unallocated is not specified
	if (argv[2] == NULL) {
		printf("No scan type specified - Defaulting to all blocks.\n");
		return ALL_BLOCKS;
	}

	// scan type can be specified by
	// "scan_drive.exe -r /dev/sdxx -t <scan_type>"
	if (strncmp(argv[2], "-r", 2) == 0 && argv[3] != NULL) {
		uint32_t type = UNALLOCATED_ONLY;

		if (strncmp(argv[3], "all", 3) == 0) {
			printf("Selected to scan all blocks.\n");
			type = ALL_BLOCKS;
		}
		if (strncmp(argv[3], "free", 4) == 0) {
			printf("Selected to scan unallocated blocks.\n");
			type = UNALLOCATED_ONLY;
		}
		if (strncmp(argv[3], "used", 4) == 0) {
			printf("Selected to scan allocated blocks.\n");
			type = ALLOCATED_ONLY;
		}
		return type;
	} else {
		fprintf(stderr, "Unrecognized scan type.\n");
		fprintf(stderr, USAGE);
		fprintf(stderr, HELP_MSG);
		exit(EXIT_FAILURE);
	}
}
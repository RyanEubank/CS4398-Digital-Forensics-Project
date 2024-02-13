#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <stdint.h>

#define SUPERBLOCK_SIGNATURE 0xef53

typedef struct _super_block {
	uint32_t _inode_count;
	uint32_t _fs_size_blocks;
	uint32_t _reserved_blocks;
	uint32_t _free_blocks;
	uint32_t _free_inodes;
	uint32_t _first_data_block;
	uint32_t _block_size;
	int32_t _fragment_size;
	uint32_t _blocks_per_group;
	uint32_t _frag_per_group;
	uint32_t _inodes_per_group;
	uint32_t _mount_time;
	uint32_t _write_time;
	uint16_t _mount_count;
	uint16_t _max_mnt_count;
	uint16_t _magic_sig;
	uint16_t _status;
	uint16_t _errors;
	uint16_t _minor_revision_lvl;
	uint32_t _last_chk_time;
	uint32_t _last_chk_interval;
	uint32_t _create_os;
	uint32_t _revision_lvl;
	uint16_t _def_resuid;
	uint16_t _def_resgid;
	uint32_t _first_inode;
	uint16_t _inode_size;
	uint16_t _group_num;
	uint32_t _compat_features;
	uint32_t _incompat_features;
	uint32_t _ro_features;
	uint8_t _uuid[16];
	uint8_t _volume_name[16];
	uint8_t _last_mounted_path[64];
	uint32_t _algorithm_bitmap;
	uint8_t _prealloc_blocks;
	uint8_t _prealloc_dir_blocks;
	uint16_t _alignment;
	uint32_t _null_padding[204];
} SuperBlock, *pSBlock;

pSBlock readSuperblock(int32_t, uint64_t);
void printSuperblock(int32_t, uint32_t);

#endif
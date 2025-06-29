#ifndef FILESYSTEM_CORE_H
#define FILESYSTEM_CORE_H

#include <time.h> 

// --- CONSTANTES ---
#define MAGIC_NUMBER 0xDA7A        
#define MAX_FILENAME_LENGTH 28     

// --- ESTRUTURAS DE DADOS ---
typedef struct {
    unsigned int magic_number;
    unsigned int total_blocks;
    unsigned int total_inodes;
    unsigned int block_size;
    unsigned int inode_bitmap_start_block;
    unsigned int block_bitmap_start_block;
    unsigned int inode_table_start_block;
    unsigned int data_blocks_start_block;
} Superblock;

typedef struct {
    unsigned int mode; // 0 = arquivo, 1 = diretório
    unsigned int link_count;
    unsigned int size_in_bytes;
    time_t creation_time;
    time_t modification_time;
    time_t last_access_time;
    unsigned int direct_blocks[12];
    unsigned int single_indirect_block;
    unsigned int double_indirect_block;
} Inode;

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    unsigned int inode_number;
} DirEntry;

// Declarações das funções
void fs_format(unsigned int disk_size, unsigned int block_size);
int fs_mount(); 
void fs_write_inode(unsigned int inode_num, const Inode* inode_data);
void fs_read_inode(unsigned int inode_num, Inode* inode_buffer);
int fs_alloc_inode();
int fs_alloc_block();
void fs_free_inode(int inode_num);
void fs_free_block(int block_num);

Superblock fs_get_superblock_info();

#endif

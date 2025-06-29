#include "filesystem_core.h"
#include "gerenciador_de_disco.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int g_verbose_mode;

static Superblock sb_g;
static int is_mounted = 0;

/*
 * Retorna uma cópia do superbloco atualmente carregado em memória.
 * input: nenhum.
 * output: A struct Superblock com os dados do sistema de arquivos.
 */
Superblock fs_get_superblock_info() {
    return sb_g;
}

/*
 * Escreve os dados de um i-node na tabela de i-nodes no disco.
 * input:
 * inode_num - O número do i-node a ser escrito.
 * inode_data - Um ponteiro para a struct Inode contendo os dados.
 * output: nenhum.
 */
void fs_write_inode(unsigned int inode_num, const Inode* inode_data) {
    if (!is_mounted) return;
    if (g_verbose_mode) printf("   [Verbose] Escrevendo i-node %u no disco...\n", inode_num);

    unsigned int inodes_per_block = sb_g.block_size / sizeof(Inode);
    unsigned int block_offset = inode_num / inodes_per_block;
    unsigned int target_block = sb_g.inode_table_start_block + block_offset;
    unsigned int index_in_block = inode_num % inodes_per_block;
    
    Inode* block_buffer = (Inode*) malloc(sb_g.block_size);
    disk_read_block(target_block, block_buffer);
    block_buffer[index_in_block] = *inode_data;
    disk_write_block(target_block, block_buffer);
    free(block_buffer);
}

/*
 * Lê os dados de um i-node da tabela de i-nodes no disco.
 * input:
 * inode_num - O número do i-node a ser lido.
 * inode_buffer - Um ponteiro para uma struct Inode onde os dados serão armazenados.
 * output: nenhum.
 */
void fs_read_inode(unsigned int inode_num, Inode* inode_buffer) {
    if (!is_mounted) return;
    if (g_verbose_mode) printf("   [Verbose] Lendo i-node %u do disco...\n", inode_num);
    
    unsigned int inodes_per_block = sb_g.block_size / sizeof(Inode);
    unsigned int block_offset = inode_num / inodes_per_block;
    unsigned int target_block = sb_g.inode_table_start_block + block_offset;
    unsigned int index_in_block = inode_num % inodes_per_block;
    
    Inode* block_buffer = (Inode*) malloc(sb_g.block_size);
    disk_read_block(target_block, block_buffer);
    *inode_buffer = block_buffer[index_in_block];
    free(block_buffer);
}

/*
 * Monta o sistema de arquivos, lendo o superbloco e preparando para operações.
 * input: nenhum.
 * output: 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_mount() {
    if(is_mounted) return 0;
    
    if (disk_mount() != 0) {
        return -1;
    }

    unsigned int temp_block_size = 4096;
    char* temp_buffer = (char*) malloc(temp_block_size);
    
    disk_set_block_size(temp_block_size);
    if (disk_read_block(0, temp_buffer) != 0) {
        fprintf(stderr, "Erro: Falha ao ler o superbloco do disco.\n");
        free(temp_buffer);
        disk_unmount();
        return -1;
    }

    memcpy(&sb_g, temp_buffer, sizeof(Superblock));
    free(temp_buffer);

    if (sb_g.magic_number != MAGIC_NUMBER) {
        fprintf(stderr, "Erro: Magic number invalido! O disco pode nao estar formatado ou esta corrompido.\n");
        disk_unmount();
        return -1;
    }
    
    disk_set_block_size(sb_g.block_size);
    is_mounted = 1;
    printf("Sistema de arquivos montado com sucesso.\n");
    return 0;
}

/*
 * Formata o disco, inicializando o superbloco, bitmaps, tabela de i-nodes e diretório raiz.
 * input:
 * disk_size - O tamanho total do disco em bytes.
 * block_size - O tamanho de cada bloco em bytes.
 * output: nenhum.
 */
void fs_format(unsigned int disk_size, unsigned int block_size) {
    printf("Iniciando a formatação lógica do sistema de arquivos...\n");

    disk_format(disk_size, block_size);
    
    if(fs_mount() != 0) {
         fprintf(stderr, "Erro crítico: não foi possível montar o disco para formatação.\n");
         return;
    }

    unsigned int total_blocks = disk_size / block_size;
    unsigned int total_inodes = total_blocks / 4; 
    unsigned int inode_bitmap_blocks = (total_inodes / 8 + block_size - 1) / block_size;
    unsigned int block_bitmap_blocks = (total_blocks / 8 + block_size - 1) / block_size;
    unsigned int inode_table_blocks = (total_inodes * sizeof(Inode) + block_size - 1) / block_size;

    sb_g.magic_number = MAGIC_NUMBER;
    sb_g.total_blocks = total_blocks;
    sb_g.total_inodes = total_inodes;
    sb_g.block_size = block_size;
    sb_g.inode_bitmap_start_block = 1;
    sb_g.block_bitmap_start_block = sb_g.inode_bitmap_start_block + inode_bitmap_blocks;
    sb_g.inode_table_start_block = sb_g.block_bitmap_start_block + block_bitmap_blocks;
    sb_g.data_blocks_start_block = sb_g.inode_table_start_block + inode_table_blocks;
    
    disk_write_block(0, &sb_g);
    if (g_verbose_mode) printf("   [Verbose] Superbloco gravado no disco.\n");

    char* zero_buffer = (char*) calloc(block_size, 1);
    for (unsigned int i = 1; i < sb_g.data_blocks_start_block; i++) {
        disk_write_block(i, zero_buffer);
    }
    free(zero_buffer);
    if (g_verbose_mode) printf("   [Verbose] Blocos de metadados zerados.\n");

    printf("Criando o diretorio raiz (/) ...\n");

    int root_inode_num = fs_alloc_inode(); 
    int root_data_block_num = fs_alloc_block();
    
    Inode root_inode;
    root_inode.mode = 1; 
    root_inode.link_count = 2;
    root_inode.size_in_bytes = block_size;
    root_inode.creation_time = time(NULL);
    root_inode.modification_time = time(NULL);
    root_inode.last_access_time = time(NULL);
    root_inode.direct_blocks[0] = root_data_block_num;
    for(int i = 1; i < 12; i++) root_inode.direct_blocks[i] = 0;
    root_inode.single_indirect_block = 0;
    root_inode.double_indirect_block = 0;

    fs_write_inode(root_inode_num, &root_inode);

    DirEntry* dir_entries = (DirEntry*) calloc(1, block_size);
    strcpy(dir_entries[0].name, ".");
    dir_entries[0].inode_number = root_inode_num;
    strcpy(dir_entries[1].name, "..");
    dir_entries[1].inode_number = root_inode_num;
    disk_write_block(root_data_block_num, dir_entries);
    free(dir_entries);

    printf("Diretorio raiz criado com sucesso no i-node %d e bloco de dados %u.\n", root_inode_num, root_data_block_num);
    
    disk_unmount(); 
    is_mounted = 0;
}

/*
 * Aloca o primeiro i-node livre no bitmap de i-nodes.
 * input: nenhum.
 * output: O número do i-node alocado, ou -1 em caso de falha.
 */
int fs_alloc_inode() {
    if (!is_mounted) return -1;
    if (g_verbose_mode) printf("   [Verbose] Procurando i-node livre no bitmap...\n");

    unsigned char* bitmap_buffer = (unsigned char*) malloc(sb_g.block_size);
    unsigned int inodes_per_block = sb_g.block_size * 8;
    unsigned int total_bitmap_blocks = (sb_g.total_inodes + inodes_per_block -1) / inodes_per_block;

    for (unsigned int block_idx = 0; block_idx < total_bitmap_blocks; block_idx++) {
        unsigned int current_block = sb_g.inode_bitmap_start_block + block_idx;
        disk_read_block(current_block, bitmap_buffer);

        for (unsigned int byte_idx = 0; byte_idx < sb_g.block_size; byte_idx++) {
            if (bitmap_buffer[byte_idx] != 0xFF) {
                for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
                    if (!(bitmap_buffer[byte_idx] & (1 << (7 - bit_idx)))) {
                        bitmap_buffer[byte_idx] |= (1 << (7 - bit_idx));
                        disk_write_block(current_block, bitmap_buffer);
                        int inode_num = (block_idx * inodes_per_block) + (byte_idx * 8) + bit_idx;
                        free(bitmap_buffer);
                        if (g_verbose_mode) printf("   [Verbose] I-node %d alocado.\n", inode_num);
                        return inode_num;
                    }
                }
            }
        }
    }

    free(bitmap_buffer);
    return -1;
}

/*
 * Aloca o primeiro bloco de dados livre no bitmap de blocos.
 * input: nenhum.
 * output: O número do bloco alocado, ou -1 em caso de falha.
 */
int fs_alloc_block() {
    if (!is_mounted) return -1;
    if (g_verbose_mode) printf("   [Verbose] Procurando bloco de dados livre no bitmap...\n");

    unsigned char* bitmap_buffer = (unsigned char*) malloc(sb_g.block_size);
    unsigned int bits_per_block = sb_g.block_size * 8;
    
    for (unsigned int block_num = sb_g.data_blocks_start_block; block_num < sb_g.total_blocks; block_num++) {
        unsigned int block_idx_in_bitmap = block_num / bits_per_block;
        unsigned int byte_idx_in_bitmap = (block_num % bits_per_block) / 8;
        unsigned int bit_idx_in_byte = block_num % 8;

        unsigned int current_bitmap_block = sb_g.block_bitmap_start_block + block_idx_in_bitmap;
        
        disk_read_block(current_bitmap_block, bitmap_buffer);
        
        if (!(bitmap_buffer[byte_idx_in_bitmap] & (1 << (7 - bit_idx_in_byte)))) {
            bitmap_buffer[byte_idx_in_bitmap] |= (1 << (7 - bit_idx_in_byte));
            disk_write_block(current_bitmap_block, bitmap_buffer);
            free(bitmap_buffer);
            if (g_verbose_mode) printf("   [Verbose] Bloco de dados %d alocado.\n", block_num);
            return block_num;
        }
    }

    free(bitmap_buffer);
    return -1;
}

/*
 * Libera um i-node no bitmap, marcando-o como livre (bit = 0).
 * input:
 * inode_num - O número do i-node a ser liberado.
 * output: nenhum.
 */
void fs_free_inode(int inode_num) {
    if (!is_mounted || inode_num < 0 || (unsigned int)inode_num >= sb_g.total_inodes) return;
    if (g_verbose_mode) printf("   [Verbose] Liberando i-node %d no bitmap...\n", inode_num);

    unsigned char* bitmap_buffer = (unsigned char*) malloc(sb_g.block_size);
    unsigned int bits_per_block = sb_g.block_size * 8;
    
    unsigned int block_idx_in_bitmap = inode_num / bits_per_block;
    unsigned int byte_idx_in_bitmap = (inode_num % bits_per_block) / 8;
    unsigned int bit_idx_in_byte = inode_num % 8;

    unsigned int target_bitmap_block = sb_g.inode_bitmap_start_block + block_idx_in_bitmap;

    disk_read_block(target_bitmap_block, bitmap_buffer);
    bitmap_buffer[byte_idx_in_bitmap] &= ~(1 << (7 - bit_idx_in_byte));
    disk_write_block(target_bitmap_block, bitmap_buffer);

    free(bitmap_buffer);
}

/*
 * Libera um bloco de dados no bitmap, marcando-o como livre (bit = 0).
 * input:
 * block_num - O número do bloco a ser liberado.
 * output: nenhum.
 */
void fs_free_block(int block_num) {
    if (!is_mounted || block_num < 0 || (unsigned int)block_num >= sb_g.total_blocks) return;
    if (g_verbose_mode) printf("   [Verbose] Liberando bloco de dados %d no bitmap...\n", block_num);

    unsigned char* bitmap_buffer = (unsigned char*) malloc(sb_g.block_size);
    unsigned int bits_per_block = sb_g.block_size * 8;
    
    unsigned int block_idx_in_bitmap = block_num / bits_per_block;
    // <<< CORREÇÃO 1: A variável se chamava byte_idx_in_block, mas foi usada como byte_idx_in_bitmap. Corrigido. >>>
    unsigned int byte_idx_in_bitmap = (block_num % bits_per_block) / 8; 
    unsigned int bit_idx_in_byte = block_num % 8;

    unsigned int target_bitmap_block = sb_g.block_bitmap_start_block + block_idx_in_bitmap;

    disk_read_block(target_bitmap_block, bitmap_buffer);
    bitmap_buffer[byte_idx_in_bitmap] &= ~(1 << (7 - bit_idx_in_byte));
    disk_write_block(target_bitmap_block, bitmap_buffer);

    free(bitmap_buffer);
}

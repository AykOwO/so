#ifndef GERENCIADOR_DE_DISCO_H
#define GERENCIADOR_DE_DISCO_H

//Declarações das funções do gerenciador de disco
int disk_format(unsigned int disk_size, unsigned int block_size);
int disk_mount();
int disk_unmount();
int disk_read_block(unsigned int block_num, void* buffer);
int disk_write_block(unsigned int block_num, const void* buffer);
void disk_set_block_size(unsigned int block_size);

#endif

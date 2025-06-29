#include "gerenciador_de_disco.h"
#include <stdio.h>

#define DISK_PATH "dados/meu_so.disk"

static FILE* disk_file = NULL;
static unsigned int block_size_g = 0; 

/*
 * Formata o arquivo de disco virtual, criando-o e alocando seu tamanho.
 * input: 
 * disk_size - O tamanho total do disco em bytes.
 * block_size - O tamanho de cada bloco em bytes.
 * output: 
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int disk_format(unsigned int disk_size, unsigned int block_size) {
    FILE* file = fopen(DISK_PATH, "wb"); 
    if (!file) {
        perror("Erro ao criar o arquivo de disco");
        return -1;
    }
    if (fseek(file, disk_size - 1, SEEK_SET) != 0) {
        perror("Erro ao posicionar no final do disco para formatação");
        fclose(file);
        return -1;
    }
    if (fwrite("\0", 1, 1, file) != 1) {
        perror("Erro ao escrever o byte nulo para alocar espaço");
        fclose(file);
        return -1;
    }
    fclose(file);
    block_size_g = block_size;
    return 0;
}

/*
 * Monta o disco, abrindo o arquivo de disco para leitura e escrita.
 * input: nenhum.
 * output: 0 em caso de sucesso, -1 se o arquivo não puder ser aberto.
 */
int disk_mount() {
    if (disk_file) {
        return 0;
    }
    disk_file = fopen(DISK_PATH, "r+b"); 
    if (!disk_file) {
        perror("Erro ao montar o disco");
        return -1;
    }
    return 0;
}

/*
 * Desmonta o disco, fechando o arquivo de disco.
 * input: nenhum.
 * output: 0.
 */
int disk_unmount() {
    if (disk_file) {
        fclose(disk_file);
        disk_file = NULL;
    }
    return 0;
}

/*
 * Define o tamanho do bloco usado internamente para cálculos de deslocamento.
 * input: 
 * block_size - O tamanho do bloco em bytes.
 * output: nenhum.
 */
void disk_set_block_size(unsigned int block_size) {
    block_size_g = block_size;
}

/*
 * Lê um único bloco de dados do disco.
 * input:
 * block_num - O número do bloco a ser lido.
 * buffer - O ponteiro para onde os dados lidos serão armazenados.
 * output: 0 em caso de sucesso, -1 em caso de erro.
 */
int disk_read_block(unsigned int block_num, void* buffer) {
    if (!disk_file || block_size_g == 0) return -1;
    long offset = block_num * block_size_g;
    if (fseek(disk_file, offset, SEEK_SET) != 0) {
        perror("Erro de fseek na leitura");
        return -1;
    }
    if (fread(buffer, block_size_g, 1, disk_file) != 1) {
        if (!feof(disk_file)) {
            perror("Erro de fread");
            return -1;
        }
    }
    return 0;
}

/*
 * Escreve o conteúdo de um buffer em um único bloco do disco.
 * input:
 * block_num - O número do bloco onde os dados serão escritos.
 * buffer - O ponteiro para os dados a serem escritos.
 * output: 0 em caso de sucesso, -1 em caso de erro.
 */
int disk_write_block(unsigned int block_num, const void* buffer) {
    if (!disk_file || block_size_g == 0) return -1;
    long offset = block_num * block_size_g;
    if (fseek(disk_file, offset, SEEK_SET) != 0) {
        perror("Erro de fseek na escrita");
        return -1;
    }
    if (fwrite(buffer, block_size_g, 1, disk_file) != 1) {
        perror("Erro de fwrite");
        return -1;
    }
    return 0;
}

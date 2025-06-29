#include "file_operations.h"
#include "gerenciador_de_disco.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int g_verbose_mode;

// --- Protótipos de Funções Auxiliares (Estáticas) ---
static int find_inode_by_path(const char* path, Inode* result_inode);
static int find_entry_in_dir(int dir_inode_num, const char* name, DirEntry* result_entry);
static int add_entry_to_dir(int parent_inode_num, const char* new_entry_name, int new_inode_num);


/*
 * Lista o conteúdo de um diretório.
 * input: 
 * path - Caminho para o diretório ou arquivo a ser listado.
 * output: 
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_ls(const char* path) {
    printf("Listando conteudo de: %s\n", path);
    printf("----------------------------------\n");
    
    Superblock sb = fs_get_superblock_info();
    
    Inode target_inode;
    int inode_num = find_inode_by_path(path, &target_inode);

    if (inode_num < 0) {
        fprintf(stderr, "ls: nao foi possivel acessar '%s': Arquivo ou diretorio nao encontrado\n", path);
        return -1;
    }

    if (target_inode.mode != 1) { 
        const char* filename = strrchr(path, '/');
        printf("%s\n", filename ? filename + 1 : path);
        return 0;
    }

    DirEntry* dir_entries = (DirEntry*) malloc(sb.block_size);
    unsigned int entries_per_block = sb.block_size / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        unsigned int block_num = target_inode.direct_blocks[i];
        if (block_num == 0) break;

        disk_read_block(block_num, dir_entries);

        for (unsigned int j = 0; j < entries_per_block; j++) {
            if (dir_entries[j].name[0] != '\0') {
                printf("%s\n", dir_entries[j].name);
            }
        }
    }

    free(dir_entries);
    printf("----------------------------------\n");
    return 0;
}

/*
 * Cria um novo diretório no caminho especificado.
 * input:
 * path - O caminho absoluto do diretório a ser criado.
 * output:
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_mkdir(const char* path) {
    char path_copy[1024];
    strncpy(path_copy, path, 1023);
    path_copy[1023] = '\0';

    char* new_dir_name = strrchr(path_copy, '/');
    char* parent_path;
    if (new_dir_name == path_copy) {
        parent_path = "/";
        new_dir_name++;
    } else {
        *new_dir_name = '\0';
        new_dir_name++;
        parent_path = path_copy;
    }

    Inode parent_inode;
    int parent_inode_num = find_inode_by_path(parent_path, &parent_inode);
    if (parent_inode_num < 0) {
        fprintf(stderr, "mkdir: nao foi possivel criar o diretorio '%s': Diretorio pai nao existe\n", path);
        return -1;
    }

    DirEntry existing_entry;
    if (find_entry_in_dir(parent_inode_num, new_dir_name, &existing_entry) == 0) {
        fprintf(stderr, "mkdir: nao foi possivel criar o diretorio '%s': Arquivo ou diretorio ja existe\n", path);
        return -1;
    }

    int new_inode_num = fs_alloc_inode();
    int new_block_num = fs_alloc_block();
    if (new_inode_num < 0 || new_block_num < 0) {
        fprintf(stderr, "mkdir: nao ha espaco livre no disco.\n");
        return -1;
    }

    Inode new_inode;
    new_inode.mode = 1;
    new_inode.link_count = 2;
    new_inode.size_in_bytes = fs_get_superblock_info().block_size;
    new_inode.creation_time = time(NULL);
    new_inode.modification_time = time(NULL);
    new_inode.last_access_time = time(NULL);
    new_inode.direct_blocks[0] = new_block_num;
    for(int i = 1; i < 12; i++) new_inode.direct_blocks[i] = 0;
    new_inode.single_indirect_block = 0;
    new_inode.double_indirect_block = 0;
    fs_write_inode(new_inode_num, &new_inode);

    Superblock sb = fs_get_superblock_info();
    DirEntry* new_dir_block = (DirEntry*) calloc(1, sb.block_size);
    strcpy(new_dir_block[0].name, ".");
    new_dir_block[0].inode_number = new_inode_num;
    strcpy(new_dir_block[1].name, "..");
    new_dir_block[1].inode_number = parent_inode_num;
    disk_write_block(new_block_num, new_dir_block);
    free(new_dir_block);

    if (add_entry_to_dir(parent_inode_num, new_dir_name, new_inode_num) != 0) {
        fprintf(stderr, "mkdir: erro ao adicionar entrada no diretorio pai (pode estar cheio).\n");
        return -1;
    }

    printf("Diretorio '%s' criado com sucesso.\n", path);
    return 0;
}

/*
 * Escreve o conteúdo de um arquivo do sistema hospedeiro para o sistema simulado.
 * input:
 * simulated_path - O caminho de destino no sistema simulado.
 * real_path - O caminho de origem no sistema hospedeiro.
 * output:
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_write(const char* simulated_path, const char* real_path) {
    FILE* real_file = fopen(real_path, "rb");
    if (!real_file) {
        perror("Nao foi possivel abrir o arquivo real");
        return -1;
    }
    fseek(real_file, 0, SEEK_END);
    long real_file_size = ftell(real_file);
    fseek(real_file, 0, SEEK_SET);

    char path_copy[1024];
    strncpy(path_copy, simulated_path, 1023);
    path_copy[1023] = '\0';
    char* new_file_name = strrchr(path_copy, '/');
    char* parent_path;
    if (new_file_name == path_copy) { parent_path = "/"; new_file_name++; }
    else { *new_file_name = '\0'; new_file_name++; parent_path = path_copy; }

    Inode parent_inode;
    int parent_inode_num = find_inode_by_path(parent_path, &parent_inode);
    if (parent_inode_num < 0) {
        fprintf(stderr, "write: Diretorio pai '%s' nao encontrado.\n", parent_path);
        fclose(real_file);
        return -1;
    }

    int new_inode_num = fs_alloc_inode();
    if (new_inode_num < 0) {
        fprintf(stderr, "write: Nao ha i-nodes livres.\n");
        fclose(real_file);
        return -1;
    }
    Inode new_inode;
    new_inode.mode = 0; // 0 = arquivo regular
    new_inode.link_count = 1;
    new_inode.size_in_bytes = real_file_size;
    new_inode.creation_time = time(NULL);
    new_inode.modification_time = time(NULL);
    new_inode.last_access_time = time(NULL);
	for(int i = 0; i < 12; i++) new_inode.direct_blocks[i] = 0;

    Superblock sb = fs_get_superblock_info();
    char* buffer = (char*) malloc(sb.block_size);
    long bytes_remaining = real_file_size;
    int block_count = 0;

    while(bytes_remaining > 0 && block_count < 12) {
        size_t bytes_to_read = (bytes_remaining > (long)sb.block_size) ? sb.block_size : bytes_remaining;
        fread(buffer, 1, bytes_to_read, real_file);
        
        int new_block_num = fs_alloc_block();
        if (new_block_num < 0) {
            fprintf(stderr, "write: Sem espaco em disco para alocar bloco.\n");
            fclose(real_file);
            free(buffer);
            return -1;
        }

        disk_write_block(new_block_num, buffer);
        new_inode.direct_blocks[block_count] = new_block_num;

        bytes_remaining -= bytes_to_read;
        block_count++;
    }
    
    fclose(real_file);
    free(buffer);
    fs_write_inode(new_inode_num, &new_inode);
    add_entry_to_dir(parent_inode_num, new_file_name, new_inode_num);

    printf("Arquivo '%s' escrito com sucesso.\n", simulated_path);
    return 0;
}

/*
 * Exibe o conteúdo de um arquivo simulado na saída padrão.
 * input:
 * path - O caminho para o arquivo a ser exibido.
 * output:
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_cat(const char* path) {
    Inode target_inode;
    if (find_inode_by_path(path, &target_inode) < 0) {
        fprintf(stderr, "cat: %s: Arquivo ou diretorio nao encontrado\n", path);
        return -1;
    }
    if (target_inode.mode != 0) {
        fprintf(stderr, "cat: %s: Nao e um arquivo\n", path);
        return -1;
    }
    if (target_inode.size_in_bytes == 0) {
        return 0;
    }

    Superblock sb = fs_get_superblock_info();
    char* buffer = (char*) malloc(sb.block_size);
    long bytes_remaining = target_inode.size_in_bytes;

    for(int i = 0; i < 12 && target_inode.direct_blocks[i] != 0; i++) {
        disk_read_block(target_inode.direct_blocks[i], buffer);
        size_t bytes_to_write = (bytes_remaining > (long)sb.block_size) ? sb.block_size : bytes_remaining;
        fwrite(buffer, 1, bytes_to_write, stdout);
        bytes_remaining -= bytes_to_write;
        if (bytes_remaining <= 0) break;
    }
    
    free(buffer);
    return 0;
}

/*
 * Remove um arquivo do sistema de arquivos.
 * input:
 * path - O caminho para o arquivo a ser removido.
 * output:
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_rm(const char* path) {
    char path_copy[1024];
    strncpy(path_copy, path, 1023);
    path_copy[1023] = '\0';
    char* file_to_rm_name = strrchr(path_copy, '/');
    char* parent_path;
    if (file_to_rm_name == path_copy) { parent_path = "/"; file_to_rm_name++; }
    else { *file_to_rm_name = '\0'; file_to_rm_name++; parent_path = path_copy; }

    Inode parent_inode;
    int parent_inode_num = find_inode_by_path(parent_path, &parent_inode);
    DirEntry entry_to_rm;
    if (find_entry_in_dir(parent_inode_num, file_to_rm_name, &entry_to_rm) != 0) {
        fprintf(stderr, "rm: %s: Arquivo nao encontrado.\n", path);
        return -1;
    }
    Inode inode_to_rm;
    fs_read_inode(entry_to_rm.inode_number, &inode_to_rm);
    if (inode_to_rm.mode != 0) {
        fprintf(stderr, "rm: %s: Nao e um arquivo. Use 'rmdir' para diretorios.\n", path);
        return -1;
    }

    for (int i = 0; i < 12; i++) {
        if (inode_to_rm.direct_blocks[i] != 0) {
            fs_free_block(inode_to_rm.direct_blocks[i]);
        }
    }
    fs_free_inode(entry_to_rm.inode_number);

    Superblock sb = fs_get_superblock_info();
    DirEntry* dir_buffer = (DirEntry*) malloc(sb.block_size);
    for (int i = 0; i < 12; i++) {
        if (parent_inode.direct_blocks[i] != 0) {
            disk_read_block(parent_inode.direct_blocks[i], dir_buffer);
            for (unsigned int j = 0; j < (sb.block_size / sizeof(DirEntry)); j++) {
                if (dir_buffer[j].inode_number == entry_to_rm.inode_number) {
                    dir_buffer[j].name[0] = '\0';
                    dir_buffer[j].inode_number = 0;
                    disk_write_block(parent_inode.direct_blocks[i], dir_buffer);
                    goto entry_removed;
                }
            }
        }
    }
entry_removed:
    free(dir_buffer);

    printf("Arquivo '%s' removido com sucesso.\n", path);
    return 0;
}

/*
 * Remove um diretório vazio do sistema de arquivos.
 * input:
 * path - O caminho do diretório a ser removido.
 * output:
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_rmdir(const char* path) {
    if (strcmp(path, "/") == 0) {
        fprintf(stderr, "rmdir: Nao e possivel remover o diretorio raiz.\n");
        return -1;
    }
    
    Inode target_inode;
    if (find_inode_by_path(path, &target_inode) < 0) {
        fprintf(stderr, "rmdir: %s: Diretorio nao encontrado.\n", path);
        return -1;
    }
    if (target_inode.mode != 1) {
        fprintf(stderr, "rmdir: %s: Nao e um diretorio.\n", path);
        return -1;
    }

    Superblock sb = fs_get_superblock_info();
    DirEntry* dir_buffer = (DirEntry*) malloc(sb.block_size);
    disk_read_block(target_inode.direct_blocks[0], dir_buffer);
    int entry_count = 0;
    for (unsigned int i = 0; i < sb.block_size / sizeof(DirEntry); i++) {
        if (dir_buffer[i].name[0] != '\0') {
            entry_count++;
        }
    }
    free(dir_buffer);
    if (entry_count > 2) {
        fprintf(stderr, "rmdir: %s: O diretorio nao esta vazio.\n", path);
        return -1;
    }

    char path_copy[1024];
    strncpy(path_copy, path, 1023);
	path_copy[1023] = '\0';
    char* dir_name = strrchr(path_copy, '/');
    char* parent_path;
    if (dir_name == path_copy) { parent_path = "/"; dir_name++; }
    else { *dir_name = '\0'; dir_name++; parent_path = path_copy; }
    
    Inode parent_inode;
    int parent_inode_num = find_inode_by_path(parent_path, &parent_inode);
    DirEntry entry;
    find_entry_in_dir(parent_inode_num, dir_name, &entry);

    if (g_verbose_mode) printf("Liberando bloco de dados %d e i-node %d para %s\n", target_inode.direct_blocks[0], entry.inode_number, path);
    fs_free_block(target_inode.direct_blocks[0]);
    fs_free_inode(entry.inode_number);

    printf("Diretorio '%s' removido com sucesso.\n", path);
    return 0;
}

/*
 * Renomeia um arquivo ou diretório dentro do mesmo diretório pai.
 * input:
 * old_path - O caminho original do arquivo/diretório.
 * new_path - O novo caminho para o arquivo/diretório.
 * output:
 * 0 em caso de sucesso, -1 em caso de erro.
 */
int fs_mv(const char* old_path, const char* new_path) {
    char old_path_copy[1024], new_path_copy[1024];
    strncpy(old_path_copy, old_path, 1023);
	old_path_copy[1023] = '\0';
    strncpy(new_path_copy, new_path, 1023);
	new_path_copy[1023] = '\0';
    
    char* old_name = strrchr(old_path_copy, '/');
    char* old_parent_path;
    if (old_name == old_path_copy) { old_parent_path = "/"; old_name++; }
    else { *old_name = '\0'; old_name++; old_parent_path = old_path_copy; }

    char* new_name = strrchr(new_path_copy, '/');
    char* new_parent_path;
    if (new_name == new_path_copy) { new_parent_path = "/"; new_name++; }
    else { *new_name = '\0'; new_name++; new_parent_path = new_path_copy; }

    if (strcmp(old_parent_path, new_parent_path) != 0) {
        fprintf(stderr, "mv: Mover entre diretorios diferentes ainda nao e suportado.\n");
        return -1;
    }

    Inode parent_inode;
    // <<< CORREÇÃO 2: A variável 'parent_inode_num' não era usada. Removida. >>>
    find_inode_by_path(old_parent_path, &parent_inode);
    
    Superblock sb = fs_get_superblock_info();
    DirEntry* dir_buffer = (DirEntry*) malloc(sb.block_size);
    for (int i = 0; i < 12; i++) {
        if (parent_inode.direct_blocks[i] != 0) {
            disk_read_block(parent_inode.direct_blocks[i], dir_buffer);
            for (unsigned int j = 0; j < sb.block_size / sizeof(DirEntry); j++) {
                if (strcmp(dir_buffer[j].name, old_name) == 0) {
                    strncpy(dir_buffer[j].name, new_name, MAX_FILENAME_LENGTH - 1);
					dir_buffer[j].name[MAX_FILENAME_LENGTH -1] = '\0';
                    disk_write_block(parent_inode.direct_blocks[i], dir_buffer);
                    printf("'%s' renomeado para '%s'.\n", old_path, new_path);
                    free(dir_buffer);
                    return 0;
                }
            }
        }
    }
    
    free(dir_buffer);
    fprintf(stderr, "mv: Nao foi possivel encontrar o arquivo de origem '%s'.\n", old_path);
    return -1;
}

/*
 * Verifica se um caminho corresponde a um diretório válido.
 * input:
 * path - O caminho a ser verificado.
 * output:
 * 0 se for um diretório, -1 caso contrário.
 */
int fs_check_path_is_dir(const char* path) {
    Inode target_inode;
    if (find_inode_by_path(path, &target_inode) < 0) {
        fprintf(stderr, "cd: %s: Arquivo ou diretorio nao encontrado\n", path);
        return -1;
    }
    if (target_inode.mode != 1) {
        fprintf(stderr, "cd: %s: Nao e um diretorio\n", path);
        return -1;
    }
    return 0;
}


// --- IMPLEMENTAÇÃO DAS FUNÇÕES AUXILIARES (ESTÁTICAS) ---

/*
 * Navega por um caminho absoluto para encontrar o i-node do arquivo/diretório final.
 * input:
 * path - O caminho absoluto a ser percorrido.
 * result_inode - Ponteiro para a struct Inode onde o resultado será armazenado.
 * output:
 * O número do i-node encontrado, ou -1 em caso de erro.
 */
static int find_inode_by_path(const char* path, Inode* result_inode) {
    if (strcmp(path, "/") == 0) {
        fs_read_inode(0, result_inode);
        return 0;
    }

    char path_copy[1024];
    strncpy(path_copy, path, 1023);
    path_copy[1023] = '\0';

    Inode current_inode;
    int current_inode_num = 0;
    fs_read_inode(current_inode_num, &current_inode);

    char* token = strtok(path_copy, "/");
    while (token != NULL) {
        DirEntry entry;
        if (find_entry_in_dir(current_inode_num, token, &entry) != 0) {
            return -1;
        }
        
        current_inode_num = entry.inode_number;
        fs_read_inode(current_inode_num, &current_inode);
        
        token = strtok(NULL, "/");
    }

    *result_inode = current_inode;
    return current_inode_num;
}

/*
 * Procura por uma entrada com um nome específico dentro de um diretório.
 * input:
 * dir_inode_num - O número do i-node do diretório onde a busca será feita.
 * name - O nome da entrada a ser procurada.
 * result_entry - Ponteiro para a struct DirEntry onde o resultado será armazenado.
 * output:
 * 0 se a entrada for encontrada, -1 caso contrário.
 */
static int find_entry_in_dir(int dir_inode_num, const char* name, DirEntry* result_entry) {
    Inode dir_inode;
    fs_read_inode(dir_inode_num, &dir_inode);

    Superblock sb = fs_get_superblock_info();
    DirEntry* dir_entries_buffer = (DirEntry*) malloc(sb.block_size);
    unsigned int entries_per_block = sb.block_size / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        if (dir_inode.direct_blocks[i] == 0) continue;

        disk_read_block(dir_inode.direct_blocks[i], dir_entries_buffer);

        for (unsigned int j = 0; j < entries_per_block; j++) {
            if (dir_entries_buffer[j].name[0] != '\0' && strcmp(dir_entries_buffer[j].name, name) == 0) {
                *result_entry = dir_entries_buffer[j];
                free(dir_entries_buffer);
                return 0;
            }
        }
    }

    free(dir_entries_buffer);
    return -1;
}

/*
 * Adiciona uma nova entrada de diretório a um diretório pai.
 * input:
 * parent_inode_num - O número do i-node do diretório pai.
 * new_entry_name - O nome da nova entrada.
 * new_inode_num - O número do i-node da nova entrada.
 * output:
 * 0 em caso de sucesso, -1 se o diretório pai estiver cheio.
 */
static int add_entry_to_dir(int parent_inode_num, const char* new_entry_name, int new_inode_num) {
    Inode parent_inode;
    fs_read_inode(parent_inode_num, &parent_inode);
    
    Superblock sb = fs_get_superblock_info();
    DirEntry* dir_entries_buffer = (DirEntry*) malloc(sb.block_size);
    unsigned int entries_per_block = sb.block_size / sizeof(DirEntry);

    for (int i = 0; i < 12; i++) {
        unsigned int block_num = parent_inode.direct_blocks[i];
        if (block_num == 0) {
            // Lógica para alocar um novo bloco para o diretório se necessário iria aqui.
            // Para este projeto, assumimos que o primeiro bloco tem espaço.
            continue;
        }

        disk_read_block(block_num, dir_entries_buffer);
        for (unsigned int j = 0; j < entries_per_block; j++) {
            if (dir_entries_buffer[j].name[0] == '\0') {
                strncpy(dir_entries_buffer[j].name, new_entry_name, MAX_FILENAME_LENGTH - 1);
                dir_entries_buffer[j].name[MAX_FILENAME_LENGTH - 1] = '\0';
                dir_entries_buffer[j].inode_number = new_inode_num;
                
                disk_write_block(block_num, dir_entries_buffer);
                free(dir_entries_buffer);

                parent_inode.modification_time = time(NULL);
                fs_write_inode(parent_inode_num, &parent_inode);
                return 0;
            }
        }
    }

    free(dir_entries_buffer);
    return -1;
}

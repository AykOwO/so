#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "filesystem_core.h"
#include "file_operations.h"
#include "gerenciador_de_disco.h"

#define DISK_PATH "dados/meu_so.disk"
#define DISK_SIZE (10 * 1024 * 1024)
#define BLOCK_SIZE 4096

// Flag global para o modo verboso, acessível por outros módulos via 'extern'.
int g_verbose_mode = 0;

// Armazena o caminho do diretório de trabalho atual.
static char current_working_directory[1024];

void run_shell(FILE* input_stream);
void ensure_data_directory_exists();
void build_full_path(const char* path, char* full_path_buffer);

/*
 * Ponto de entrada principal do programa.
 * input:
 * argc - Número de argumentos da linha de comando.
 * argv - Vetor de strings com os argumentos.
 * output:
 * 0 em caso de sucesso, 1 em caso de erro.
 */
int main(int argc, char* argv[]) {
    if (argc > 2) {
        fprintf(stderr, "Uso: %s [arquivo_de_script]\n", argv[0]);
        return 1;
    }

    ensure_data_directory_exists();

    if (access(DISK_PATH, F_OK) != 0) {
        printf("Arquivo de disco nao encontrado. Formatando um novo...\n");
        fs_format(DISK_SIZE, BLOCK_SIZE);
        printf("Formatacao concluida.\n\n");
    }

    printf("--- Montando o Sistema de Arquivos ---\n");
    if (fs_mount() != 0) {
        fprintf(stderr, "Nao foi possivel montar o sistema de arquivos. Encerrando.\n");
        return 1;
    }
    printf("--------------------------------------\n\n");
    
    strcpy(current_working_directory, "/");

    FILE* input_stream = stdin;
    if (argc == 2) {
        printf("Executando em Modo em Lote a partir de '%s'...\n", argv[1]);
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            perror("Nao foi possivel abrir o arquivo de script");
            disk_unmount();
            return 1;
        }
    }

    run_shell(input_stream);

    if (input_stream != stdin) {
        fclose(input_stream);
    }

    printf("\n--- Desmontando o Sistema de Arquivos ---\n");
    disk_unmount();

    return 0;
}

/*
 * Garante que o diretório "dados" exista para armazenar o arquivo de disco.
 * input: nenhum.
 * output: nenhum.
 */
void ensure_data_directory_exists() {
    struct stat st = {0};
    if (stat("dados", &st) == -1) {
        mkdir("dados", 0700);
    }
}

/*
 * Constrói um caminho absoluto a partir de um caminho possivelmente relativo.
 * input:
 * path - O caminho de entrada (absoluto ou relativo).
 * full_path_buffer - O buffer onde o caminho absoluto resultante será armazenado.
 * output: nenhum (modifica o buffer passado como argumento).
 */
void build_full_path(const char* path, char* full_path_buffer) {
    if (path[0] == '/') {
        strcpy(full_path_buffer, path);
    } else {
        if (strcmp(current_working_directory, "/") == 0) {
            sprintf(full_path_buffer, "/%s", path);
        } else {
            sprintf(full_path_buffer, "%s/%s", current_working_directory, path);
        }
    }
}

/*
 * Executa o loop principal do shell, lendo e processando comandos.
 * input:
 * input_stream - O fluxo de entrada de onde os comandos serão lidos (stdin ou um arquivo).
 * output: nenhum.
 */
void run_shell(FILE* input_stream) {
    char line_buffer[1024];
    char command[100];
    char arg1[512], arg2[512];

    if (input_stream == stdin) {
        printf("Bem-vindo ao simulador de Sistema de Arquivos!\n");
        printf("Comandos: ls, mkdir, cd, write, cat, rm, rmdir, mv, verbose, exit\n\n");
    }

    while (1) {
        if (input_stream == stdin) {
            printf("meu_fs:%s$ ", current_working_directory);
        }
        
        if (fgets(line_buffer, sizeof(line_buffer), input_stream) == NULL) {
            break;
        }
        if (input_stream != stdin) {
            printf("Executando: %s", line_buffer);
        }
        line_buffer[strcspn(line_buffer, "\n")] = 0;

        arg1[0] = '\0';
        arg2[0] = '\0';
        int num_args = sscanf(line_buffer, "%s %s %s", command, arg1, arg2);

        if (num_args <= 0) continue;

        if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "ls") == 0) {
            char path[1024];
            build_full_path(num_args < 2 ? "." : arg1, path);
            fs_ls(path);
        } else if (strcmp(command, "mkdir") == 0) {
            if (num_args < 2) { fprintf(stderr, "mkdir: operando faltando\n"); }
            else { char path[1024]; build_full_path(arg1, path); fs_mkdir(path); }
        } else if (strcmp(command, "cd") == 0) {
            if (num_args < 2) { fprintf(stderr, "cd: operando faltando\n"); }
            else { 
                char path[1024]; 
                build_full_path(arg1, path); 
                if (fs_check_path_is_dir(path) == 0) {
                    strcpy(current_working_directory, path);
                    if (strlen(current_working_directory) > 1 && current_working_directory[strlen(current_working_directory) - 1] == '/') {
                        current_working_directory[strlen(current_working_directory) - 1] = '\0';
                    }
                }
            }
        } else if (strcmp(command, "write") == 0) {
            if (num_args < 3) { fprintf(stderr, "Uso: write <arq_simulado> <arq_real>\n"); }
            else { char path[1024]; build_full_path(arg1, path); fs_write(path, arg2); }
        } else if (strcmp(command, "cat") == 0) {
            if (num_args < 2) { fprintf(stderr, "cat: operando faltando\n"); }
            else { char path[1024]; build_full_path(arg1, path); fs_cat(path); }
        } else if (strcmp(command, "rm") == 0) {
            if (num_args < 2) { fprintf(stderr, "rm: operando faltando\n"); }
            else { char path[1024]; build_full_path(arg1, path); fs_rm(path); }
        } else if (strcmp(command, "rmdir") == 0) {
            if (num_args < 2) { fprintf(stderr, "rmdir: operando faltando\n"); }
            else { char path[1024]; build_full_path(arg1, path); fs_rmdir(path); }
        } else if (strcmp(command, "mv") == 0) {
            if (num_args < 3) { fprintf(stderr, "Uso: mv <origem> <destino>\n"); }
            else { 
                char old_p[1024], new_p[1024]; 
                build_full_path(arg1, old_p);
                build_full_path(arg2, new_p);
                fs_mv(old_p, new_p);
            }
        } else if (strcmp(command, "verbose") == 0) {
            if (num_args < 2) { fprintf(stderr, "Uso: verbose <on|off>\n"); }
            else {
                if (strcmp(arg1, "on") == 0) { g_verbose_mode = 1; printf("Modo verboso ativado.\n"); }
                else if (strcmp(arg1, "off") == 0) { g_verbose_mode = 0; printf("Modo verboso desativado.\n"); }
                else { fprintf(stderr, "Uso: verbose <on|off>\n"); }
            }
        }
        else {
            fprintf(stderr, "Comando desconhecido: '%s'\n", command);
        }
    }
}

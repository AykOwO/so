#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "filesystem_core.h"

//Declaração das funções
int fs_ls(const char* path);
int fs_mkdir(const char* path);
int fs_check_path_is_dir(const char* path);
int fs_write(const char* simulated_path, const char* real_path);
int fs_cat(const char* path);
int fs_rm(const char* path);
int fs_rmdir(const char* path);
int fs_mv(const char* old_path, const char* new_path);

#endif

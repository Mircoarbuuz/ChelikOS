#ifndef FAT_H
#define FAT_H

#include <stdint.h>

typedef struct {
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t root_dir_cluster;
    uint32_t sectors_per_cluster;
} fat32_filesystem_t;

int fat32_mount(int drive, fat32_filesystem_t *fs);
void fat32_list_root_dir(fat32_filesystem_t *fs, void (*callback)(const char *name, uint8_t attributes));
int fat32_create_file(fat32_filesystem_t *fs, const char *filename);
int fat32_delete_file(fat32_filesystem_t *fs, const char *filename);
int fat32_read_file(fat32_filesystem_t *fs, const char *filename, char *buffer, uint32_t max_len);

#endif
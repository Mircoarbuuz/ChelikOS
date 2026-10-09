#ifndef CHELIK_FAT_H
#define CHELIK_FAT_H

#include "../int.h"
#include "../def.h"

// Master Boot Record (MBR) Partition Entry (16 bytes)
typedef struct {
    uint8_t  drive_attributes;
    uint8_t  chs_start[3];
    uint8_t  partition_type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) mbr_partition_t;

// Master Boot Record (Sector 0)
typedef struct {
    uint8_t boot_code[446];
    mbr_partition_t partitions[4];
    uint16_t boot_signature;
} __attribute__((packed)) mbr_t;

// FAT32 BIOS Parameter Block (BPB) & Extended Boot Record
typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint8_t  volume_id[4];
    uint8_t  volume_label[11];
    uint8_t  file_system_type[8];
} __attribute__((packed)) fat32_bpb_t;

// FAT32 Directory Entry (32 bytes)
typedef struct {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

// Mount context structure
typedef struct {
    uint32_t partition_lba;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t sectors_per_cluster;
    uint32_t root_cluster;
} fat32_filesystem_t;

// Function Prototypes
int fat32_mount(uint32_t base_lba, fat32_filesystem_t *fs);
int fat32_read_file(fat32_filesystem_t *fs, const char *filename, uint8_t *buffer);
int fat32_list_root_dir(fat32_filesystem_t *fs, void (*callback)(const char *name, uint8_t attributes));
#endif
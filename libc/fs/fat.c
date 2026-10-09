#include "fat.h"

extern int disk_read_sectors(uint32_t lba, uint32_t count, uint8_t *buffer);

static void format_fat_name(const char *src, uint8_t *dest) {
    int i;
    for (i = 0; i < 11; i++) dest[i] = ' ';

    int dest_idx = 0;
    for (i = 0; src[i] != '\0' && dest_idx < 8; i++) {
        if (src[i] == '.') break;
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dest[dest_idx++] = c;
    }

    const char *ext = src;
    while (*ext != '\0' && *ext != '.') ext++;
    if (*ext == '.') {
        ext++;
        dest_idx = 8;
        for (i = 0; ext[i] != '\0' && i < 3; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            dest[dest_idx++] = c;
        }
    }
}

int fat32_mount(uint32_t base_lba, fat32_filesystem_t *fs) {
    uint8_t sector_buf[512];

    if (disk_read_sectors(base_lba, 1, sector_buf) != 0) {
        return -1;
    }

    uint32_t target_lba = base_lba;
    fat32_bpb_t *bpb = (fat32_bpb_t *)sector_buf;

    int is_fat32_vbr = (bpb->bytes_per_sector == 512 && 
                        bpb->sectors_per_cluster != 0 && 
                        bpb->fat_size_32 != 0);

    if (!is_fat32_vbr) {
        mbr_t *mbr = (mbr_t *)sector_buf;
        if (mbr->boot_signature == 0xAA55) {
            uint8_t p_type = mbr->partitions[0].partition_type;
            if ((p_type == 0x0B || p_type == 0x0C || p_type == 0x1B || p_type == 0x1C) &&
                mbr->partitions[0].lba_start != 0) {
                target_lba = mbr->partitions[0].lba_start;
                if (disk_read_sectors(target_lba, 1, sector_buf) != 0) {
                    return -1;
                }
                bpb = (fat32_bpb_t *)sector_buf;
            }
        }
    }

    if (bpb->bytes_per_sector != 512) {
        return -2;
    }

    fs->partition_lba = target_lba;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->root_cluster = bpb->root_cluster;
    fs->fat_start_lba = target_lba + bpb->reserved_sector_count;
    fs->data_start_lba = fs->fat_start_lba + (bpb->num_fats * bpb->fat_size_32);

    return 0;
}

int fat32_read_file(fat32_filesystem_t *fs, const char *filename, uint8_t *buffer) {
    uint8_t target_name[11];
    format_fat_name(filename, target_name);

    uint32_t dir_cluster = fs->root_cluster;
    uint8_t sector_buf[512];
    fat32_dir_entry_t *found_entry = 0;

    while (dir_cluster < 0x0FFFFFF8 && !found_entry) {
        uint32_t cluster_lba = fs->data_start_lba + (dir_cluster - 2) * fs->sectors_per_cluster;

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            if (disk_read_sectors(cluster_lba + s, 1, sector_buf) != 0) {
                return -3; 
            }

            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)sector_buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) {
                    dir_cluster = 0x0FFFFFF8; 
                    break;
                }
                if (entries[i].name[0] == 0xE5) continue; 

                if (entries[i].attributes == 0x0F) continue;
                if ((entries[i].attributes & 0x08) != 0) continue;
                if ((entries[i].attributes & 0x10) != 0) continue;

                int match = 1;
                for (int j = 0; j < 11; j++) {
                    if (entries[i].name[j] != target_name[j]) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    found_entry = &entries[i];
                    break;
                }
            }
            if (found_entry) break;
        }

        if (!found_entry && dir_cluster < 0x0FFFFFF8) {
            uint32_t fat_sector = fs->fat_start_lba + (dir_cluster * 4) / 512;
            uint32_t fat_offset = (dir_cluster * 4) % 512;

            if (disk_read_sectors(fat_sector, 1, sector_buf) != 0) {
                return -4;
            }
            dir_cluster = (*(uint32_t *)&sector_buf[fat_offset]) & 0x0FFFFFFF;
        }
    }

    if (!found_entry) {
        return -2; 
    }

    uint32_t file_size = found_entry->file_size;
    if (file_size == 0) {
        return 0; 
    }

    uint32_t file_cluster = ((uint32_t)found_entry->first_cluster_high << 16) | found_entry->first_cluster_low;
    uint32_t bytes_remaining = file_size;
    uint8_t *dest = buffer;

    while (file_cluster < 0x0FFFFFF8 && bytes_remaining > 0) {
        uint32_t cluster_lba = fs->data_start_lba + (file_cluster - 2) * fs->sectors_per_cluster;

        for (uint32_t s = 0; s < fs->sectors_per_cluster && bytes_remaining > 0; s++) {
            if (disk_read_sectors(cluster_lba + s, 1, sector_buf) != 0) {
                return -6; 
            }

            uint32_t bytes_to_copy = (bytes_remaining > 512) ? 512 : bytes_remaining;
            for (uint32_t b = 0; b < bytes_to_copy; b++) {
                *dest++ = sector_buf[b];
            }
            bytes_remaining -= bytes_to_copy;
        }

        uint32_t fat_sector = fs->fat_start_lba + (file_cluster * 4) / 512;
        uint32_t fat_offset = (file_cluster * 4) % 512;

        if (disk_read_sectors(fat_sector, 1, sector_buf) != 0) {
            return -7; 
        }

        file_cluster = (*(uint32_t *)&sector_buf[fat_offset]) & 0x0FFFFFFF;
    }

    return file_size;
}

int fat32_list_root_dir(fat32_filesystem_t *fs, void (*callback)(const char *name, uint8_t attributes)) {
    uint32_t dir_cluster = fs->root_cluster;
    uint8_t sector_buf[512];

    while (dir_cluster < 0x0FFFFFF8) {
        uint32_t cluster_lba = fs->data_start_lba + (dir_cluster - 2) * fs->sectors_per_cluster;

        for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
            if (disk_read_sectors(cluster_lba + s, 1, sector_buf) != 0) {
                return -1; 
            }

            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)sector_buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) {
                    return 0; 
                }
                if (entries[i].name[0] == 0xE5) continue; 
                if (entries[i].attributes == 0x0F) continue; 
                if ((entries[i].attributes & 0x08) != 0) continue; 

                char name_buf[13];
                int idx = 0;
                for (int j = 0; j < 8; j++) {
                    if (entries[i].name[j] != ' ') {
                        name_buf[idx++] = entries[i].name[j];
                    }
                }
                
                if (entries[i].name[8] != ' ') {
                    name_buf[idx++] = '.';
                    for (int j = 8; j < 11; j++) {
                        if (entries[i].name[j] != ' ') {
                            name_buf[idx++] = entries[i].name[j];
                        }
                    }
                }
                name_buf[idx] = '\0';

                if (callback) {
                    callback(name_buf, entries[i].attributes);
                }
            }
        }

        uint32_t fat_sector = fs->fat_start_lba + (dir_cluster * 4) / 512;
        uint32_t fat_offset = (dir_cluster * 4) % 512;

        if (disk_read_sectors(fat_sector, 1, sector_buf) != 0) {
            return -2;
        }
        dir_cluster = (*(uint32_t *)&sector_buf[fat_offset]) & 0x0FFFFFFF;
    }

    return 0;
}
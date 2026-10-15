#include "fat.h"
#include "../string.h"

// External disk read/write functions provided by your disk driver
extern int disk_read_sector(uint32_t sector, uint8_t *buffer);
extern int disk_write_sector(uint32_t sector, const uint8_t *buffer);

int fat32_mount(int drive, fat32_filesystem_t *fs) {
    (void)drive;
    uint8_t boot_sector[512];
    if (disk_read_sector(0, boot_sector) != 0) {
        return -1;
    }

    // Basic FAT32 BPB parsing offsets
    uint16_t bytes_per_sec = *(uint16_t *)&boot_sector[11];
    uint8_t sec_per_clust = boot_sector[13];
    uint16_t reserved_sec = *(uint16_t *)&boot_sector[14];
    uint8_t num_fats = boot_sector[16];
    uint32_t fat_size32 = *(uint32_t *)&boot_sector[36];
    uint32_t root_clust = *(uint32_t *)&boot_sector[44];

    if (bytes_per_sec != 512) {
        return -1; // Only 512-byte sectors supported
    }

    fs->fat_start_sector = reserved_sec;
    fs->data_start_sector = reserved_sec + (num_fats * fat_size32);
    fs->root_cluster = root_clust;
    fs->sectors_per_cluster = sec_per_clust ? sec_per_clust : 1;

    return 0;
}

static uint32_t cluster_to_sector(fat32_filesystem_t *fs, uint32_t cluster) {
    return fs->data_start_sector + (cluster - 2) * fs->sectors_per_cluster;
}

int fat32_list_root_dir(fat32_filesystem_t *fs, void (*callback)(const char *name, uint8_t attributes)) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) {
        return -1;
    }

    // Parse directory entries (32 bytes each)
    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00) break; // End of directory
        if (entry[0] == 0xE5) continue; // Deleted file

        // Skip long file name entries and volume IDs
        if (entry[11] == 0x0F || (entry[11] & 0x08)) continue;

        char name_buf[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (entry[j] == ' ') break;
            name_buf[idx++] = entry[j];
        }
        if (entry[8] != ' ') {
            name_buf[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (entry[j] == ' ') break;
                name_buf[idx++] = entry[j];
            }
        }
        name_buf[idx] = '\0';

        callback(name_buf, entry[11]);
    }
    return 0;
}

int fat32_create_file(fat32_filesystem_t *fs, const char *filename) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) {
        return -1;
    }

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            for (int j = 0; j < 11; j++) entry[j] = ' ';
            
            int name_len = 0;
            while (filename[name_len] != '\0' && filename[name_len] != '.' && name_len < 8) {
                entry[name_len] = filename[name_len];
                name_len++;
            }
            if (filename[name_len] == '.') {
                name_len++;
                int ext_len = 0;
                while (filename[name_len + ext_len] != '\0' && ext_len < 3) {
                    entry[8 + ext_len] = filename[name_len + ext_len];
                    ext_len++;
                }
            }

            entry[11] = 0x20; // Archive attribute
            *(uint16_t *)&entry[20] = 0; // High cluster
            *(uint16_t *)&entry[26] = 0; // Low cluster
            *(uint32_t *)&entry[28] = 0; // File size

            disk_write_sector(root_sector, sector_buf);
            return 0;
        }
    }
    return -1; // Directory full
}

int fat32_create_dir(fat32_filesystem_t *fs, const char *dirname) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) {
        return -1;
    }

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            for (int j = 0; j < 11; j++) entry[j] = ' ';
            
            int name_len = 0;
            while (dirname[name_len] != '\0' && name_len < 8) {
                entry[name_len] = dirname[name_len];
                name_len++;
            }

            entry[11] = 0x10; // Directory attribute
            *(uint16_t *)&entry[20] = 0;
            *(uint16_t *)&entry[26] = 0;
            *(uint32_t *)&entry[28] = 0;

            disk_write_sector(root_sector, sector_buf);
            return 0;
        }
    }
    return -1;
}

int fat32_delete_file(fat32_filesystem_t *fs, const char *filename) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) {
        return -1;
    }

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name_buf[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (entry[j] == ' ') break;
            name_buf[idx++] = entry[j];
        }
        if (entry[8] != ' ') {
            name_buf[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (entry[j] == ' ') break;
                name_buf[idx++] = entry[j];
            }
        }
        name_buf[idx] = '\0';

        if (strcmp(name_buf, filename) == 0) {
            entry[0] = 0xE5; // Mark as deleted
            disk_write_sector(root_sector, sector_buf);
            return 0;
        }
    }
    return -1; // File not found
}

int fat32_read_file(fat32_filesystem_t *fs, const char *filename, char *buffer, uint32_t max_len) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) {
        return -1;
    }

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name_buf[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (entry[j] == ' ') break;
            name_buf[idx++] = entry[j];
        }
        if (entry[8] != ' ') {
            name_buf[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (entry[j] == ' ') break;
                name_buf[idx++] = entry[j];
            }
        }
        name_buf[idx] = '\0';

        if (strcmp(name_buf, filename) == 0) {
            uint32_t file_size = *(uint32_t *)&entry[28];
            uint32_t cluster = *(uint16_t *)&entry[26];
            
            if (file_size == 0) {
                buffer[0] = '\0';
                return 0;
            }

            uint32_t data_sector = cluster_to_sector(fs, cluster);
            uint8_t file_sector[512];
            if (disk_read_sector(data_sector, file_sector) != 0) {
                return -1;
            }

            uint32_t bytes_to_copy = file_size < max_len ? file_size : max_len - 1;
            for (uint32_t k = 0; k < bytes_to_copy; k++) {
                buffer[k] = (char)file_sector[k];
            }
            buffer[bytes_to_copy] = '\0';
            return bytes_to_copy;
        }
    }
    return -1; // File not found
}
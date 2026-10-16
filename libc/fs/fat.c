#include "fat.h"
#include "../string.h"

extern int disk_read_sector(uint32_t sector, uint8_t *buffer);
extern int disk_write_sector(uint32_t sector, const uint8_t *buffer);

static int string_case_equals(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2) return 0;
        s1++; s2++;
    }
    return *s1 == *s2;
}

int fat32_mount(int drive, fat32_filesystem_t *fs) {
    (void)drive;
    uint8_t boot_sector[512];
    if (disk_read_sector(0, boot_sector) != 0) {
        return -1;
    }

    uint16_t bytes_per_sec = *(uint16_t *)&boot_sector[11];
    uint8_t sec_per_clust = boot_sector[13];
    uint16_t reserved_sec = *(uint16_t *)&boot_sector[14];
    uint8_t num_fats = boot_sector[16];
    uint32_t fat_size32 = *(uint32_t *)&boot_sector[36];
    uint32_t root_clust = *(uint32_t *)&boot_sector[44];

    if (bytes_per_sec != 512) return -1;

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

    if (disk_read_sector(root_sector, sector_buf) != 0) return -1;

    char lfn_buf[260];
    int has_lfn = 0;
    // Clear lfn_buf manually to avoid missing string.h dependencies
    for (int i = 0; i < 260; i++) lfn_buf[i] = '\0';

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) {
            has_lfn = 0;
            for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
            continue;
        }

        // Handle LFN entry (Attribute 0x0F)
        if (entry[11] == 0x0F) {
            int seq = entry[0] & 0x3F;
            if (seq > 0 && seq <= 20) {
                int idx = (seq - 1) * 13;
                
                // Extract chars from LFN entry layout
                uint16_t chars[13];
                chars[0] = *(uint16_t*)&entry[1];
                chars[1] = *(uint16_t*)&entry[3];
                chars[2] = *(uint16_t*)&entry[5];
                chars[3] = *(uint16_t*)&entry[7];
                chars[4] = *(uint16_t*)&entry[9];
                
                chars[5] = *(uint16_t*)&entry[14];
                chars[6] = *(uint16_t*)&entry[16];
                chars[7] = *(uint16_t*)&entry[18];
                chars[8] = *(uint16_t*)&entry[20];
                chars[9] = *(uint16_t*)&entry[22];
                chars[10] = *(uint16_t*)&entry[24];
                
                chars[11] = *(uint16_t*)&entry[28];
                chars[12] = *(uint16_t*)&entry[30];

                for (int c = 0; c < 13; c++) {
                    if (idx + c < 259) {
                        if (chars[c] == 0x0000 || chars[c] == 0xFFFF) {
                            lfn_buf[idx + c] = '\0';
                        } else {
                            lfn_buf[idx + c] = (chars[c] < 128) ? (char)chars[c] : '_';
                        }
                    }
                }
                has_lfn = 1;
            }
            continue;
        }

        // Skip volume IDs
        if (entry[11] & 0x08) {
            has_lfn = 0;
            for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
            continue;
        }

        // Determine filename to use (LFN if available, else 8.3 short name)
        const char *final_name;
        char short_name[13];

        if (has_lfn && lfn_buf[0] != '\0') {
            final_name = lfn_buf;
        } else {
            int idx = 0;
            for (int j = 0; j < 8; j++) {
                if (entry[j] == ' ') break;
                short_name[idx++] = entry[j];
            }
            if (entry[8] != ' ') {
                short_name[idx++] = '.';
                for (int j = 8; j < 11; j++) {
                    if (entry[j] == ' ') break;
                    short_name[idx++] = entry[j];
                }
            }
            short_name[idx] = '\0';
            final_name = short_name;
        }

        callback(final_name, entry[11]);

        // Reset LFN state for next entry
        has_lfn = 0;
        for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
    }
    return 0;
}

int fat32_create_file(fat32_filesystem_t *fs, const char *filename) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) return -1;

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            for (int j = 0; j < 11; j++) entry[j] = ' ';
            
            int name_len = 0;
            while (filename[name_len] != '\0' && filename[name_len] != '.' && name_len < 8) {
                char c = filename[name_len];
                if (c >= 'a' && c <= 'z') c -= 32;
                entry[name_len] = c;
                name_len++;
            }
            if (filename[name_len] == '.') {
                name_len++;
                int ext_len = 0;
                while (filename[name_len + ext_len] != '\0' && ext_len < 3) {
                    char c = filename[name_len + ext_len];
                    if (c >= 'a' && c <= 'z') c -= 32;
                    entry[8 + ext_len] = c;
                    ext_len++;
                }
            }

            entry[11] = 0x20; 
            *(uint16_t *)&entry[20] = 0;
            *(uint16_t *)&entry[26] = 0;
            *(uint32_t *)&entry[28] = 0;

            disk_write_sector(root_sector, sector_buf);
            return 0;
        }
    }
    return -1;
}

int fat32_create_dir(fat32_filesystem_t *fs, const char *dirname) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) return -1;

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            for (int j = 0; j < 11; j++) entry[j] = ' ';
            
            int name_len = 0;
            while (dirname[name_len] != '\0' && name_len < 8) {
                char c = dirname[name_len];
                if (c >= 'a' && c <= 'z') c -= 32;
                entry[name_len] = c;
                name_len++;
            }

            entry[11] = 0x10;
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

    if (disk_read_sector(root_sector, sector_buf) != 0) return -1;

    char lfn_buf[260];
    int has_lfn = 0;
    for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) {
            has_lfn = 0;
            for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
            continue;
        }

        if (entry[11] == 0x0F) {
            int seq = entry[0] & 0x3F;
            if (seq > 0 && seq <= 20) {
                int idx = (seq - 1) * 13;
                uint16_t chars[13];
                chars[0] = *(uint16_t*)&entry[1];  chars[1] = *(uint16_t*)&entry[3];
                chars[2] = *(uint16_t*)&entry[5];  chars[3] = *(uint16_t*)&entry[7];
                chars[4] = *(uint16_t*)&entry[9];  chars[5] = *(uint16_t*)&entry[14];
                chars[6] = *(uint16_t*)&entry[16]; chars[7] = *(uint16_t*)&entry[18];
                chars[8] = *(uint16_t*)&entry[20]; chars[9] = *(uint16_t*)&entry[22];
                chars[10] = *(uint16_t*)&entry[24];chars[11] = *(uint16_t*)&entry[28];
                chars[12] = *(uint16_t*)&entry[30];

                for (int c = 0; c < 13; c++) {
                    if (idx + c < 259) {
                        if (chars[c] == 0x0000 || chars[c] == 0xFFFF) lfn_buf[idx + c] = '\0';
                        else lfn_buf[idx + c] = (chars[c] < 128) ? (char)chars[c] : '_';
                    }
                }
                has_lfn = 1;
            }
            continue;
        }

        char short_name[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (entry[j] == ' ') break;
            short_name[idx++] = entry[j];
        }
        if (entry[8] != ' ') {
            short_name[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (entry[j] == ' ') break;
                short_name[idx++] = entry[j];
            }
        }
        short_name[idx] = '\0';

        int match = 0;
        if (has_lfn && lfn_buf[0] != '\0' && string_case_equals(lfn_buf, filename)) {
            match = 1;
        } else if (string_case_equals(short_name, filename)) {
            match = 1;
        }

        if (match) {
            entry[0] = 0xE5; // Mark short entry as deleted
            disk_write_sector(root_sector, sector_buf);
            return 0;
        }

        has_lfn = 0;
        for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
    }
    return -1;
}

int fat32_read_file(fat32_filesystem_t *fs, const char *filename, char *buffer, uint32_t max_len) {
    uint8_t sector_buf[512];
    uint32_t root_sector = cluster_to_sector(fs, fs->root_cluster);

    if (disk_read_sector(root_sector, sector_buf) != 0) return -1;

    char lfn_buf[260];
    int has_lfn = 0;
    for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';

    for (int i = 0; i < 512; i += 32) {
        uint8_t *entry = &sector_buf[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) {
            has_lfn = 0;
            for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
            continue;
        }

        if (entry[11] == 0x0F) {
            int seq = entry[0] & 0x3F;
            if (seq > 0 && seq <= 20) {
                int idx = (seq - 1) * 13;
                uint16_t chars[13];
                chars[0] = *(uint16_t*)&entry[1];  chars[1] = *(uint16_t*)&entry[3];
                chars[2] = *(uint16_t*)&entry[5];  chars[3] = *(uint16_t*)&entry[7];
                chars[4] = *(uint16_t*)&entry[9];  chars[5] = *(uint16_t*)&entry[14];
                chars[6] = *(uint16_t*)&entry[16]; chars[7] = *(uint16_t*)&entry[18];
                chars[8] = *(uint16_t*)&entry[20]; chars[9] = *(uint16_t*)&entry[22];
                chars[10] = *(uint16_t*)&entry[24];chars[11] = *(uint16_t*)&entry[28];
                chars[12] = *(uint16_t*)&entry[30];

                for (int c = 0; c < 13; c++) {
                    if (idx + c < 259) {
                        if (chars[c] == 0x0000 || chars[c] == 0xFFFF) lfn_buf[idx + c] = '\0';
                        else lfn_buf[idx + c] = (chars[c] < 128) ? (char)chars[c] : '_';
                    }
                }
                has_lfn = 1;
            }
            continue;
        }

        char short_name[13];
        int idx = 0;
        for (int j = 0; j < 8; j++) {
            if (entry[j] == ' ') break;
            short_name[idx++] = entry[j];
        }
        if (entry[8] != ' ') {
            short_name[idx++] = '.';
            for (int j = 8; j < 11; j++) {
                if (entry[j] == ' ') break;
                short_name[idx++] = entry[j];
            }
        }
        short_name[idx] = '\0';

        int match = 0;
        if (has_lfn && lfn_buf[0] != '\0' && string_case_equals(lfn_buf, filename)) {
            match = 1;
        } else if (string_case_equals(short_name, filename)) {
            match = 1;
        }

        if (match) {
            uint32_t file_size = *(uint32_t *)&entry[28];
            uint32_t cluster = *(uint16_t *)&entry[26];
            
            if (file_size == 0) {
                buffer[0] = '\0';
                return 0;
            }

            uint32_t data_sector = cluster_to_sector(fs, cluster);
            uint8_t file_sector[512];
            if (disk_read_sector(data_sector, file_sector) != 0) return -1;

            uint32_t bytes_to_copy = file_size < max_len ? file_size : max_len - 1;
            for (uint32_t k = 0; k < bytes_to_copy; k++) {
                buffer[k] = (char)file_sector[k];
            }
            buffer[bytes_to_copy] = '\0';
            return bytes_to_copy;
        }

        has_lfn = 0;
        for (int k = 0; k < 260; k++) lfn_buf[k] = '\0';
    }
    return -1;
}
#include "process.h"

static process_t process_table[MAX_PROCESSES];
static int next_pid = 1;

void process_manager_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_STATE_UNUSED;
    }
    // Initialize the main shell process
    process_create("shell");
}

int process_create(const char* name) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_UNUSED || process_table[i].state == PROCESS_STATE_TERMINATED) {
            process_table[i].pid = next_pid++;
            process_table[i].state = (process_table[i].pid == 1) ? PROCESS_STATE_RUNNING : PROCESS_STATE_READY;
            
            // Copy name safely
            int j = 0;
            while (name[j] && j < 31) {
                process_table[i].name[j] = name[j];
                j++;
            }
            process_table[i].name[j] = '\0';
            return process_table[i].pid;
        }
    }
    return -1; // Process table full
}

int process_kill(int pid) {
    // Prevent killing the root shell (PID 1)
    if (pid <= 1) return -1; 

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid && process_table[i].state != PROCESS_STATE_UNUSED) {
            process_table[i].state = PROCESS_STATE_TERMINATED;
            return 0; // Success
        }
    }
    return -1; // PID not found
}

void process_list_all(void (*callback)(int pid, const char* state, const char* name)) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_STATE_UNUSED && process_table[i].state != PROCESS_STATE_TERMINATED) {
            const char* state_str = "UNKNOWN";
            if (process_table[i].state == PROCESS_STATE_RUNNING) state_str = "RUNNING";
            else if (process_table[i].state == PROCESS_STATE_READY) state_str = "READY";
            
            callback(process_table[i].pid, state_str, process_table[i].name);
        }
    }
}
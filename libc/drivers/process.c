#include "process.h"

#define MAX_PROCESSES 16

static process_t process_table[MAX_PROCESSES];
static int current_process_index = 0;
static int next_pid = 1;
static int multitasking_enabled = 0;

extern void switch_to(uint64_t *old_rsp, uint64_t new_rsp);

// --- THE EXIT TRAMPOLINE WRAPPER ---
static void process_exit_wrapper(void) {
    process_t *curr = &process_table[current_process_index];
    
    // Execute the actual task function
    if (curr->entry_point) {
        curr->entry_point();
    }
    
    // Terminate instead of returning to 0x0
    process_kill(curr->pid);
    
    while (1) {
        process_yield(); // Never return
    }
}

void process_manager_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_STATE_TERMINATED;
    }
    process_table[0].pid = next_pid++;
    process_table[0].name[0] = 'k'; 
    process_table[0].name[1] = 'e';
    process_table[0].name[2] = 'r'; 
    process_table[0].name[3] = 'n';
    process_table[0].name[4] = 'e'; 
    process_table[0].name[5] = 'l';
    process_table[0].state = PROCESS_STATE_RUNNING;
    current_process_index = 0;
    multitasking_enabled = 1;
}

int process_create(const char *name, void (*entry_point)(void)) {
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_TERMINATED) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    process_t *p = &process_table[slot];
    p->pid = next_pid++;
    p->entry_point = entry_point; // Store entry point for wrapper
    
    int j = 0;
    while (name[j] != '\0' && j < 31) {
        p->name[j] = name[j];
        j++;
    }
    p->name[j] = '\0';

    p->state = PROCESS_STATE_READY;

    uint64_t *stack_top = (uint64_t *)(p->stack + sizeof(p->stack));
    
    // Point the return address of switch_to directly to our exit wrapper
    *(--stack_top) = (uint64_t)process_exit_wrapper;

    // Push initial values for 6 callee-saved registers (rbp, rbx, r12-r15)
    for (int i = 0; i < 6; i++) {
        *(--stack_top) = 0;
    }

    p->rsp = (uint64_t)stack_top;
    return p->pid;
}

void process_yield(void) {
    if (!multitasking_enabled) return;

    int prev_index = current_process_index;
    int next_index = prev_index;

    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int candidate = (prev_index + i) % MAX_PROCESSES;
        if (process_table[candidate].state == PROCESS_STATE_READY) {
            next_index = candidate;
            break;
        }
    }

    if (prev_index == next_index) return;

    process_table[prev_index].state = PROCESS_STATE_READY;
    process_table[next_index].state = PROCESS_STATE_RUNNING;
    current_process_index = next_index;

    switch_to(&process_table[prev_index].rsp, process_table[next_index].rsp);
}

void process_list_all(void (*callback)(int pid, const char *state, const char *name)) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_STATE_TERMINATED) {
            const char *state_str = "READY";
            if (process_table[i].state == PROCESS_STATE_RUNNING) state_str = "RUNNING";
            callback(process_table[i].pid, state_str, process_table[i].name);
        }
    }
}

int process_kill(int pid) {
    if (pid <= 1) return -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            process_table[i].state = PROCESS_STATE_TERMINATED;
            return 0;
        }
    }
    return -1;
}
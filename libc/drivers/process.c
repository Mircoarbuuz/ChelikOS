#include "process.h"

#define MAX_PROCESSES 32

static process_t process_table[MAX_PROCESSES];
static int current_process_index = 0;
static int next_pid = 1;
static int multitasking_enabled = 0;

static void process_exit_wrapper(void) {
    process_t *curr = &process_table[current_process_index];
    if (curr->entry_point) {
        curr->entry_point();
    }
    process_kill(curr->pid);
    while (1) {
        // Yield execution permanently if stuck
        __asm__ volatile ("int $0x20"); // Trigger timer/yield manually if needed
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
    __asm__ volatile ("cli"); // Disable interrupts during table modification
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_TERMINATED) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        __asm__ volatile ("sti");
        return -1; // Table full (32 reached)
    }

    process_t *p = &process_table[slot];
    p->pid = next_pid++;
    p->entry_point = entry_point;
    
    int j = 0;
    while (name[j] != '\0' && j < 31) {
        p->name[j] = name[j];
        j++;
    }
    p->name[j] = '\0';

    p->state = PROCESS_STATE_READY;

    uint64_t *stack_top = (uint64_t *)(p->stack + sizeof(p->stack));
    *(--stack_top) = (uint64_t)process_exit_wrapper;

    // Zero out initial registers for a clean context frame
    for (int i = 0; i < 15; i++) {
        *(--stack_top) = 0;
    }

    p->rsp = (uint64_t)stack_top;
    __asm__ volatile ("sti");
    return p->pid;
}

/**
 * Core preemptive scheduler tick. 
 * Called directly from your timer interrupt handler (isr.asm / pit.c).
 * Saves current stack pointer, picks the next READY task, and returns its stack pointer.
 */
uint64_t process_schedule(uint64_t current_rsp) {
    if (!multitasking_enabled) return current_rsp;

    // Save current process stack pointer
    process_table[current_process_index].rsp = current_rsp;

    if (process_table[current_process_index].state == PROCESS_STATE_RUNNING) {
        process_table[current_process_index].state = PROCESS_STATE_READY;
    }

    // Round-robin selection for next ready process
    int prev_index = current_process_index;
    int next_index = prev_index;

    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int candidate = (prev_index + i) % MAX_PROCESSES;
        if (process_table[candidate].state == PROCESS_STATE_READY) {
            next_index = candidate;
            break;
        }
    }

    process_table[next_index].state = PROCESS_STATE_RUNNING;
    current_process_index = next_index;

    // Return the stack pointer of the newly scheduled process
    return process_table[next_index].rsp;
}

// Retained for manual yielding if desired by shell loops
void process_yield(void) {
    if (!multitasking_enabled) return;
    __asm__ volatile ("int $0x20"); // Software interrupt to trigger timer tick / reschedule
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
    __asm__ volatile ("cli");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            process_table[i].state = PROCESS_STATE_TERMINATED;
            __asm__ volatile ("sti");
            return 0;
        }
    }
    __asm__ volatile ("sti");
    return -1;
}
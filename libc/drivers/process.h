#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    int pid;
    char name[32];
    process_state_t state;
    uint64_t rsp;
    void (*entry_point)(void); // Stored entry point for the exit wrapper
    uint8_t stack[4096];
} process_t;

void process_manager_init(void);
int process_create(const char *name, void (*entry_point)(void));
void process_yield(void);
void process_list_all(void (*callback)(int pid, const char *state, const char *name));
int process_kill(int pid);

#endif
#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES 16

typedef enum {
    PROCESS_STATE_UNUSED = 0,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_READY,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    int pid;
    process_state_t state;
    char name[32];
} process_t;

void process_manager_init(void);
int process_create(const char* name);
int process_kill(int pid);
void process_list_all(void (*callback)(int pid, const char* state, const char* name));

#endif
#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct process {
    char** argv; 
    int argc;
    pid_t pid;
    bool completed;
    bool stopped;
    bool background;
    int status;
    struct termios tmodes;
    int stdin2, stdout2, stderr2;
    struct process* next;
    struct process* prev;
} process;

process* first_process; // pointer to the first process that is launched 
process* last_process;

void launch_process(process* p);
void put_process_in_background (process* p, int cont);
void put_process_in_foreground (process* p, int cont);
void execv_error(int e, char* file);
void signal_behaviour(int a);
void print_process_list(void) ;
process *find_process(pid_t pid);
process *most_recent_bg();
#endif

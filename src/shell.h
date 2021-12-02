#ifndef _SHELL_H_
#define _SHELL_H_

#include <termios.h>
#include "sys/types.h"
#include "process.h"

int shell_terminal; //File descripter for the terminal.
int shell_is_interactive; //1 if shell_terminal is a valid terminal. 0 otherwise
struct termios shell_tmodes; //terminal options for the shell
pid_t shell_pgid; //The shell's process id
size_t shell_pathmax;
int shell(int argc, char *argv[]);
int awake(process *p);
#endif

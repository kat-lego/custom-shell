#include "process.h"
#include "shell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <termios.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "parse.h"



void signal_behaviour(int a){
    if(a == 0){
        signal(SIGINT,SIG_IGN);
        signal(SIGTSTP,SIG_IGN);
        signal(SIGTTIN,SIG_IGN);
        signal(SIGTTOU,SIG_IGN);
        signal(SIGCHLD,SIG_IGN);
        
    }else{
        signal(SIGINT,SIG_DFL);
        signal(SIGTSTP,SIG_DFL);
        signal(SIGTTIN,SIG_DFL);
        signal(SIGTTOU,SIG_DFL);
        signal(SIGCHLD,SIG_DFL);
    }
}

/**
 *  Executes the process p.
 *  If the shell is in interactive mode and the process is a foreground process,
 *  then p should take control of the terminal.
 *
 */
void launch_process(process *p) {
    /** TODO **/
    int id = fork();
    p->pid = id;
    if(id == 0){
        if(shell_is_interactive){
          //set pgid
          pid_t pid = getpid ();
          pid_t pgid = pid;
          setpgid (pid, pgid);
          if (!p->background){
            //change forground process group
            if (tcsetpgrp (shell_terminal, pgid)){
              perror("tcsetpgrp() error");
              exit(1);
            }
          }
          

          //set signal to default
          signal_behaviour(1);
          
        }

        //set up stdin
        if(p->stdin2 != -1){
            dup2(p->stdin2,STDIN_FILENO);
            close(p->stdin2);
        }

        //set stdout
        if(p->stdout2 != -1){
            dup2(p->stdout2,STDOUT_FILENO);
            close(p->stdout2);
        }

        if(strchr(p->argv[0],'/') != NULL){
          execv(p->argv[0],p->argv);
        }else{
          char* dir = malloc(shell_pathmax);
          char* path = getenv("PATH");
          tok_t* t = getToks(path);
          int i=0;
          while(t[i]!=NULL){
            strcpy(dir,"");
            strcat(dir,t[i]);
            strcat(dir, "/");
            strcat(dir,p->argv[0]);
            execv(dir,p->argv);
            i++;
          }
        }
        execv_error(errno, p->argv[0]);
        exit(1);
  }else if(id<0){
    perror ("fork");
    exit (1);
  }else{
    if (shell_is_interactive){
      setpgid (p->pid, id);
    }

    if (!shell_is_interactive){
      waitpid(p->pid,NULL,WUNTRACED);
    }else if (p->background){
      put_process_in_background (p, 0);
    }else{
      put_process_in_foreground (p, 0);
    }

  }
}

/**
 *  Put a process in the foreground. This function assumes that the shell
 *  is in interactive mode. If the cont argument is true, send the process
 *  group a SIGCONT signal to wake it up.
 *
 */
void put_process_in_foreground (process *p, int cont) {
    /** TODO **/
  tcsetpgrp (shell_terminal, p->pid);

  if (cont){
    awake(p);
  }

  waitpid(p->pid,NULL, WUNTRACED);

  tcsetpgrp (shell_terminal, shell_pgid);

  tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);

}

/**
 *  Put a process in the background. If the cont argument is true, send
 *  the process group a SIGCONT signal to wake it up. 
 *
 */
void put_process_in_background (process *p, int cont) {
    /** TODO **/
    fprintf(stdout, "%s running on PID %d in the background\n", p->argv[0], p->pid);

}



void execv_error(int e, char* file){
  switch(e){
    case ENOENT:
      fprintf(stderr, "Could not execute '%s': No such file or directory\n",file);

  }
}

process *find_process(pid_t pid){
  process *p = first_process;
  while(p!= NULL && p->pid != pid)p=p->next;

  return p;
}

/**
 *  Prints the list of processes.
 *
 */
void print_process_list(void) {
    process* curr = first_process;
    while(curr) {
        if(!curr->completed) {
            printf("\n");
            printf("PID: %d\n",curr->pid);
            printf("Name: %s\n",curr->argv[0]);
            printf("Status: %d\n",curr->status);
            printf("Completed: %s\n",(curr->completed)?"true":"false");
            printf("Stopped: %s\n",(curr->stopped)?"true":"false");
            printf("Background: %s\n",(curr->background)?"true":"false");
            printf("Prev: %lx\n",(unsigned long)curr->prev);
            printf("Next: %lx\n",(unsigned long)curr->next);
        }
        curr=curr->next;
    }
}

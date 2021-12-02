#define _POSIX_SOURCE

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define INPUT_STRING_SIZE 80
#define CNORMAL "\x1B[0m"
#define CYELLOW "\x1B[33m"

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_help(tok_t arg[]);
int cmd_quit(tok_t arg[]);
int cmd_pwd();
int cmd_cd(tok_t arg[]);
int cmd_wait();
int cmd_fg(tok_t arg[]);

char* get_pwd();
int format_dir(char* dir,char* path);
int print_pwd();

/**
 *  Built-In Command Lookup Table Structures
 */
typedef int cmd_fun_t (tok_t args[]); // cmd functions take token array and return int
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;
/** TODO: add more commands (pwd etc.) to this lookup table! */
fun_desc_t cmd_table[] = {
    {cmd_help, "help", "show this help menu"},
    {cmd_quit, "quit", "quit the command shell"},
    {cmd_pwd, "pwd", "print present working directory"},
    {cmd_cd, "cd", "change working directory"},
    {cmd_wait, "wait", "waits until all background jobs have terminated before returning to the prompt."},
    {cmd_fg, "fg", "fg [pid]: Move the process with id pid to the foreground"},
};

/**
 *  Determine whether cmd is a built-in shell command
 *
 */
int lookup(char cmd[]) {
    unsigned int i;
    for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    }
    return -1;
}

/**
 *  Print the help table (cmd_table) for built-in shell commands
 *
 */
int cmd_help(tok_t arg[]) {
    unsigned int i;
    for (i = 0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
        printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
    }
    return 1;
}

/**
 *  Quit the terminal
 *
 */
int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    exit(0);
    return 1;
}

int cmd_pwd(){
  char* cwd = get_pwd();
  fprintf(stdout, "%s\n", cwd);
  free(cwd);
  return 1;
}

char OLDPWD[PATH_MAX];
int cmd_cd(tok_t arg[]){

  if(arg[1]==NULL){
    char dir[PATH_MAX];

    if(arg[0][0] == '-'){
      if(OLDPWD[0] == '\0'){
         fprintf(stderr,"%s\n", "OLDPWD not set");
         return 1;
      }else{
        strcpy(dir,OLDPWD);
      }
    }else{
      format_dir(dir,arg[0]);
    }
    char pwd[PATH_MAX];
    getcwd(pwd,PATH_MAX);
    chdir(dir);
    switch (errno) {
        case 0:
          strcpy(OLDPWD,pwd);
          break;
        case EACCES:
          fprintf(stderr,"%s\n", "search permission is denied for one of the components of path");
          break;
        case ENAMETOOLONG:
          fprintf(stderr,"%s\n", "path is too long.");
          break;
        case ENOENT:
          fprintf(stderr,"%s\n", "path does not exist.");
          break;
        default:
          fprintf(stderr,"%s:%d\n","Error Code", errno);
      }
  }else{
    fprintf(stderr, "%s\n", "too many arguments");
  }

  return 1;
}

int cmd_wait(){
  process *p = first_process;
  while(p!=NULL){
    waitpid(p->pid,NULL,0);
    p = p->next;
  }
  return 0;
}

int cmd_fg(tok_t arg[]){
  if(arg[0] == NULL){
    process *p = last_process;
    while(p!=NULL){
      if(!p->completed){
        put_process_in_foreground(p,1);
        break;
      }
      p = p->prev;
    }
    if(!p){
      fprintf(stderr,"fg: current: no such job\n");
    }
    
  }else if(arg[1] == NULL){
    pid_t pid = atoi(arg[0]);
    process *p;
    if((p = find_process(pid)) == NULL){
      fprintf(stderr,"fg: %d: no such job\n", pid);
      return 1;
    }
    // fprintf(stdout ,"%d\n" ,p->pid);
    put_process_in_foreground(p,1);

  }else{
    fprintf(stderr, "%s\n", "too many arguments");
  }
  return 0;
}


/**
 *  Initialise the shell
 *
 */
void init_shell() {
    // Check if we are running interactively
    shell_terminal = STDIN_FILENO;

    // Note that we cannot take control of the terminal if the shell is not interactive
    shell_is_interactive = isatty(shell_terminal);

    if( shell_is_interactive ) {

        // force into foreground
        while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
            kill( - shell_pgid, SIGTTIN);

        signal_behaviour(0);
        shell_pgid = getpid();
        // Put shell in its own process group
        if(setpgid(shell_pgid, shell_pgid) < 0){
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Take control of the terminal
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);
    }
    shell_pathmax = PATH_MAX;
    /** TODO */
    // ignore signals
}

int awake(process *p){
    if (kill (- p->pid, SIGCONT) < 0){
      perror ("kill (SIGCONT)");
      return 0;
    }
    return 1;
}

void update_process_status(){
  process *p = first_process;

  while(p){
    if(kill(p->pid, 0 )!= 0){
      p->completed = 1;
    }
    p=p->next;
  }
}


/**
 * Add a process to our process list
 *
 */
void add_process(process* p)
{
    /** TODO **/
  process* tmp = first_process;
  if(tmp == NULL){
    first_process = p;
    first_process->next = NULL;
    first_process->prev = NULL;
    last_process = first_process;
  }else{
    p->prev = last_process;
    last_process->next = p;
    last_process = p;
  }
}

/**
 * Creates a process given the tokens parsed from stdin
 *
 */
process* create_process(tok_t* tokens)
{
    /** TODO **/
  process* p = malloc(sizeof(process*));

  int in = isDirectTok(tokens, "<");
  int out = isDirectTok(tokens, ">");
  int b = isDirectTok(tokens, "&");
  p->stdin2 = -1;
  p->stdout2 = -1;
  p->completed = 0;
  p->stopped = 0;
  if(b != -1){
    p->background = true;
    tokens[b] = NULL;
  }else{
    p->background = false;
  }

  // tcgetattr (shell_terminal, &p->tmodes);

  int end = MAXTOKS; //to figure out where the program arguments end

  if(in != -1){ // if there was a file input redirection
    if(tokens[in+1] == NULL){ // if there was no file name specified
      fprintf(stderr, "%s\n", "Unexpected token `newline");
      return NULL;
    }

    p->stdin2 = open(tokens[in+1],O_RDONLY);
    if(p->stdin2 == -1){ //if specified file was not found
      if(errno == ENOENT)fprintf(stderr,"File `%s' does not exit\n", tokens[in+1]);
      return NULL; 
    }

    end = in;
  }

  if(out != -1){
    if(tokens[out+1] == NULL){
      fprintf(stderr, "%s\n", "Unexpected token `newline");
      return NULL;
    }

    p->stdout2 = open(tokens[out+1],O_WRONLY|O_CREAT,0666);
    if(p->stdout2 == -1){
      if(errno == ENOENT)fprintf(stderr,"File `%s' does not exit\n", tokens[out+1]);
      return NULL; 
    }

    end = (out<end)? out : end;
  }

  if(end!=MAXTOKS)tokens[end] = NULL;

  int argc = 0;
  while(tokens[argc]!= NULL)argc++;

  p->argc = argc;
  p->argv = tokens;
  
  return p;
}


/**
 * Main shell loop
 *
 */
int shell (int argc, char *argv[]) {
    int lineNum = 0;
    pid_t pid = getpid();	// get current process's PID
    pid_t ppid = getppid();	// get parent's PID
    pid_t cpid;             // use this to store a child PID

    char *s = malloc(INPUT_STRING_SIZE+1); // user input string
    tok_t *t;			                   // tokens parsed from input
    // if you look in parse.h, you'll see that tokens are just C-style strings (char*)

    // perform some initialisation
    init_shell();

    fprintf(stdout, "%s running as PID %d under %d\n",argv[0],pid,ppid);
    /** TODO: replace "TODO" with the current working directory */
    print_pwd(lineNum);
    // Read input from user, tokenize it, and perform commands
    while ( ( s = freadln(stdin) ) ) {
        update_process_status();
        t = getToks(s);            // break the line into tokens
        int fundex = lookup(t[0]); // is first token a built-in command?
        // fprintf(stderr, "%s\n", t[1]);
        if( fundex >= 0 ) {
            cmd_table[fundex].fun(&t[1]); // execute built-in command
        } else {
            /** TODO: replace this statement to call a function that runs executables */
       
          process *p = create_process(t);
          add_process(p);
          if(p!=NULL)launch_process(p);
        }

        lineNum++;
        /** TODO: replace "TODO" with the current working directory */
        print_pwd(lineNum);

    }
    return 0;
}




int print_pwd(int lineNum){
  char* cwd = get_pwd();
  fprintf(stdout, CYELLOW "%d %s# " CNORMAL, lineNum, cwd);
  free(cwd);
  return 1;
}

//TODO: work on error codes and stuff
char* get_pwd(){
  size_t size = PATH_MAX;
  char* cwd = malloc(PATH_MAX);
  getcwd(cwd, size);
  return cwd;
}


int format_dir(char* dir,char* path){

  // char* dir = NULL;
  // dir = malloc(PATH_MAX);

  strcpy(dir,"");
  char* homedir = getenv("HOME");
  if(path == NULL || path[0] == '\0'){
    strcat(dir,"../");
  }else if(path[0] == '~'){
    strcat(dir,homedir);
    strcat(dir,path+1);
  }else{
    strcat(dir,path);
  }
  return 0;
}



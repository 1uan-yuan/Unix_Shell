#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <sys/wait.h>

#include "shell.hh"

int yyparse(void);

std::string shell_path;

void Shell::prompt() {
  if (!isatty(0)) {
    return;
  }
  char * env_prompt = getenv("PROMPT");
  if (env_prompt == NULL) {
    printf("myshell>");
  }
  else {
    printf(env_prompt);
  }
  fflush(stdout);
}

extern "C" void handler( int sig )
{
  if (sig == SIGINT) {
    printf("\n");
	  Shell::prompt();
  }
  else if (sig == SIGCHLD) {
    int pid;
    while ((pid = waitpid(-1, NULL, 0)) > 0) {
      // terminate all zombie children
      if (Shell::_currentCommand._background) {
        fprintf(stdout, "%d exited\n", pid);
      }
    }
  }
}

int main(int argc, char ** argv) {
  if (argv[0] != 0) {
    shell_path = std::string(argv[0]);
  }

  struct sigaction sa;
  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &sa, NULL)){
      perror("Sigaction failed!");
      exit(1);
  }

  if (sigaction(SIGCHLD, &sa, NULL)) {
      perror("Sigaction failed!");
      exit(1);
  }

  Shell::prompt();

  if (isatty(0)) {
    run_source(".shellrc");
  }
  yyparse();
  // fprintf(stderr, "yyparse here!");
}

Command Shell::_currentCommand;

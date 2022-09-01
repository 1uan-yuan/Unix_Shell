
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <limits.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include "y.tab.hh"

extern int status;
extern int last_process;
extern std::string shell_path;
extern std::string last_simple_arg;

extern char * read_line();
int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
  // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

std::string current_command;
std::vector<std::string> command_table;
int command_num;

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

void run_source(char * yytext) {
  FILE * FILE_A;
  FILE_A = fopen(yytext, "r");
  // fprintf(stderr, "FILE NAME: %s\n", yytext);
  if (FILE_A != NULL) {
    bool flag = false;
    if (yyin == NULL) {
      flag = true;
    }
    yypush_buffer_state(yy_create_buffer(FILE_A, YY_BUF_SIZE));
    BEGIN(0);
    yyparse();
    yypop_buffer_state();
    if (flag) {
      yyrestart(stdin);
    }
  }
}

%}

%s SOURCE_SC
%option noyywrap

%%

\n {
  command_table.push_back(current_command);
  command_num++;
  current_command = "";
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
  std::string tmp = std::string(yytext);
  if (!strcmp(tmp.c_str(), " ")) {
    current_command += " ";
  }
  else {
    current_command += "\t";
  }
}

">" {
  current_command += ">";
  return GREAT;
}

"<" {
  current_command += "<";
  return LESS;
}

"|" {
  current_command += "|";
  return PIPE;
}

">>" {
  current_command += ">>";
  return GREATGREAT;
}

">&" {
  current_command += ">&";
  return GREATAMPERSAND;
}

">>&" {
  current_command += ">>&";
  return GREATGREATAMPERSAND;
}

"&" {
  current_command += "&";
  return AMPERSAND;
}

"2>" {
  current_command += "2>";
  return TWOGREAT;
}

"exit" {
  return EXIT;
}

(\`[^\t\n\`][^\t\n\`]*\`)|(\$\([^\t\n\)][^\t\n\)]*\)) { // detected subshell
  // fprintf(stderr, "Entered subshell!");
  std::string sub_command = std::string(yytext);
  current_command += sub_command;
  if (sub_command[0] == '$') {
    // $(sub_command)
    sub_command = sub_command.substr(2, sub_command.size() - 3);
  }
  else {
    // `sub_command`
    sub_command = sub_command.substr(1, sub_command.size() - 2);
  }


  //    S  H  E  L  L
  //    |PIN[1]     |POUT[0]
  //    |           |  
  //    |command    |
  //    |exit       |output of command
  //    |           | 
  //    |PIN[0]     |POUT[1]
  //   S  U  B  S  H  E  L  L

  sub_command += "\nexit\n";
  // fprintf(stderr, "%s\n", sub_command.c_str());
  // fflush(stderr);
  
  // 1. Parent create two pipes
  int tmpin = dup(0); // save the default input
  int tmpout = dup(1); // save the default output
  int pin[2], pout[2];
  pipe(pin);
  pipe(pout);
  // redirect parent in and out to correct pipes

  // 4. Parent writes to pin[1]
  write(pin[1], sub_command.c_str(), sub_command.size());
  close(pin[1]);

  // 3. In child, redirect input from pin[0] and redirect output to pout[1]
  dup2(pin[0], 0);
  dup2(pout[1], 1);
  close(pin[0]);
  close(pout[1]);

  // 2. Parent create child process
  int ret;
  ret = fork();
  if (ret == 0) { // child process
    // 5. The child process runs the subshell
    execvp("/proc/self/exe", NULL);
    perror("The execv function went wrong!");
    exit(1);
  }
  else if (ret < 0) { // fork process went wrong
    perror("The fork process is wrong!");
    exit(0);
  }
  waitpid(ret, NULL, 0);

  dup2(tmpin, 0);
  dup2(tmpout, 1);
  close(tmpin);
  close(tmpout);
  // fprintf(stderr, "child finish\n");

  // 6. Parent read from pout[0]
  int max = 1024;
  char *buffer = (char *) malloc(max);
  int i = 0;
  while (read(pout[0], &buffer[i], 1) != 0) {
    // fprintf(stderr, "read %c\n", buffer[i]);
    // fflush(stderr);
    if (i == max) {
      max *= 2;
      buffer = (char *) realloc(buffer, max);
    }
    if(buffer[i] == '\n') {
      buffer[i] = ' ';
    }
    i++;
  }
  close(pout[0]);
  for(i = i - 1; i >=0 ; i--) {
    myunputc(buffer[i]);
  }
}

"source" {
  BEGIN(SOURCE_SC);
}

<SOURCE_SC>([^ \t\n\|\<\>\&\\]|\\[^ \n]*)([^ \t\n\|\<\>\&\\]|\\[^ \n]*)* {
  // code for source
  // fprintf(stderr, "Entered source!");
  current_command += std::string("source ") + std::string(yytext);
  run_source(yytext);
}

(\$\{[^}]*\})[^\t\n\>\<\|]*(\$\{[^}]*\})* { // Environment variable expansion
  // fprintf(stderr, "Entered environ!");
  std::string tmp = std::string(yytext);
  current_command += tmp;
  std::string answer;
  for (int i = 0; i < tmp.length(); i++) {
    // fprintf(stdout, "tmp: %s\n", tmp);
    // Environment Variable Expansion special cases
    // List different cases
    if ((tmp[i] == '$') && (tmp[i + 1] == '{')) { // match the pattern of "${"
      int mark; // mark represents the place of "}"
      for (int j = i; j < tmp.length(); j++) {
        if (tmp[j] == '}') {
          mark = j;
          break;
        }
      }
      // the length of the string in ${ string }
      int length = mark - i - 2;
      std::string target = tmp.substr(i + 2, length);
      // fprintf(stderr, "mark: %d\n", mark);
      // fprintf(stderr, "length: %d\n", length);
      // fprintf(stderr, "i: %d\n", i);
      // fprintf(stderr, "target: %s\n", target.c_str());
      if (!strcmp(target.c_str(), "$")) { // The PID of the shell process
        pid_t pid = getpid(); // PASSED!
        // fprintf(stdout, "pid: %d\n", pid);
        target = std::to_string(pid);
        answer += target;
      }
      else if (!strcmp(target.c_str(), "?")) { // The return code of the last executed command (ignoring commands sent to the background)
        target = std::to_string(status); // 221??????????????????? 225???????????????
        // fprintf(stderr, "shell.l status: %d\n", status);
        answer += target;
      }
      else if (!strcmp(target.c_str(), "!")) { // PID of the last process run in the background
        target = std::to_string(last_process);
        answer += target;
      }
      else if (!strcmp(target.c_str(), "_")) { // The last argument in the fully expanded previous command
       // target = command_table[command_num - 1]; // need to release undirect!!!!!!!!!!!!!!!!!!!
       // int mark_greater;
       // if ((mark_greater = target.find('>')) != std::string::npos) {
       //   if (target[mark_greater - 1] == ' ') {
            // target contains " >"
       //     target = target.substr(0, mark_greater - 1);
       //   }
       // }
       target = last_simple_arg;
       answer += target;
      }
      else if (!strcmp(target.c_str(), "SHELL")) { // The path of your shell executable
        char * path = realpath(shell_path.c_str(), NULL);
        target = std::string(path);
        free(path);
        answer += target;
      }
      else {
        // fprintf(stderr, "answer1: %s\n", answer.c_str());
        // fprintf(stderr, "target: %s\n", target.c_str());
        // fprintf(stderr, "new answer: %s\n", std::string(getenv(target.c_str())).c_str());
        answer += std::string(getenv(target.c_str()));
        // fprintf(stderr, "answer2: %s\n", answer.c_str());
      }
      i = i + length + 2;
    }
    else {
      answer.push_back(tmp[i]);
    }
  }
  yylval.cpp_string = new std::string(answer);
  return WORD;
}

\"[^"]*\" {
  /* Quotes */
  // fprintf(stderr, "Entered quotes!");
  std::string tmp = std::string(yytext);
  current_command += tmp;
  // fprintf(stderr, "old tmp: %s\n", tmp.c_str());
  int first_occurrence = tmp.find('"');
  // fprintf(stderr, "first occurrence: %d\n", first_occurrence);
  int last_occurrence = tmp.find_last_of('"');
  // fprintf(stderr, "last occurrence: %d\n", last_occurrence);
  int length = last_occurrence - first_occurrence - 1;
  tmp = tmp.substr(first_occurrence + 1, length);
  // fprintf(stderr, "new tmp: %s\n", tmp.c_str());
  yylval.cpp_string = new std::string(tmp);
  return WORD;
}

\~|\~\/ { // Tilde expansion: basic situation: ~ or ~/
  // fprintf(stderr, "Entered tilde expansion: basic!");
  std::string tmp = std::string(yytext);
  current_command += tmp;
  std::string home_directory;
  home_directory = getenv("HOME");
  if (!strcmp(home_directory.c_str(), "")) {
    home_directory = getpwuid(getuid())->pw_dir;
  }
  yylval.cpp_string = new std::string(home_directory);
  return WORD;
}

\~[^ \t\n\/][^ \t\n\/]*|\~\/[^ \t\n\/][^ \t\n\/]* { // Tilde expansion: second situation: ~george  or  ~/george
  // fprintf(stderr, "Entered tilde expansion: second!");
  std::string tmp = std::string(yytext);
  current_command += tmp;
  std::string home_directory;
  if (tmp.at(1) == '/') { // ~/george
    tmp = tmp.substr(2, tmp.size() - 2);
  }
  else { // ~george
    tmp = tmp.substr(1, tmp.size() - 1);
  }
  home_directory = getpwnam(tmp.c_str())->pw_dir;
  yylval.cpp_string = new std::string(home_directory);
  return WORD;
}

\~[^ \t\n\/][^ \t\n\/]*\/[^ \t\n\/][^ \t\n\/]*|\~\/[^ \t\n\/][^ \t\n\/]*\/[^ \t\n\/][^ \t\n\/]* { // Tilede expansion: final situation: ~george/dir or ~/george/dir
  // fprintf(stderr, "Entered tilde expansion: third!");
  std::string tmp = std::string(yytext);
  current_command += tmp;
  std::string home_directory;
  std::string given_directory;
  int position;
  if (tmp.at(1) == '/') { // ~/george/dir
    tmp = tmp.substr(2);
  }
  else { // ~george/dir
    tmp = tmp.substr(1);
  }
  // tmp = george/dir
  position = tmp.find('/');
  given_directory = tmp.substr(position); // given_directory = /dir
  tmp = tmp.substr(0, tmp.size() - given_directory.size()); // tmp = george
  home_directory = getpwnam(tmp.c_str())->pw_dir;
  home_directory += given_directory;
  yylval.cpp_string = new std::string(home_directory);
  return WORD;
}

([^ \t\n\|\<\>\&\\]|\\[^ \n]*)([^ \t\n\|\<\>\&\\]|\\[^ \n]*)* {
  // fprintf(stderr, "Entered word!");
  /* Assume that file names have only alpha chars */
  std::string tmp = std::string(yytext);
  // fprintf(stderr, "tmp: %s\n", tmp.c_str());
  current_command += tmp;
  for (int i = tmp.length() - 1; i >= 0; i--) {
    if (tmp[i] == '\\') {
      tmp.erase(i, 1);
      if ((i > 0) && (tmp[i - 1] == '\\')) {
        i--;
      }
    }
  }
  // fprintf(stderr, "tmp: %s\n", tmp.c_str());
  yylval.cpp_string = new std::string(tmp);
  return WORD;
}

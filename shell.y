
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>
#include <unistd.h>


#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN EXIT GREAT NEWLINE LESS PIPE GREATGREAT GREATAMPERSAND GREATGREATAMPERSAND AMPERSAND TWOGREAT

%{
//#define yylex yylex
#include <cstdio>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <sys/types.h>
#include <algorithm>
#include <assert.h>
#include "shell.hh"

void yyerror(const char * s);
int yylex();

std::vector<std::string> actual_array;

std::string convert_to_string(char * a) {
  std::string s = a;
  return s;
}

void expandWildcardsIfNecessary(std::string * arg)
{
  // fprintf(stderr, "Able to be here!\n");
  // fprintf(stderr, "arg: %s\n", arg->c_str());
  // Return if arg does not contain ‘*’ or ‘?’
  bool found;
  if (((arg->find('*')) != std::string::npos) || ((arg->find('?')) != std::string::npos)) {
    found = true;
    // fprintf(stderr, "Able to be here!\n");
  }
  else {
    found = false;
  }
  if (!found) {
    Command::_currentSimpleCommand->insertArgument(arg);
    return;
  }

  // 1. Convert wildcard to regular expression
  // Convert “*” -> “.*”
  // “?” -> “.”
  // “.” -> “\.” and others you need
  // Also add ^ at the beginning and $ at the end to match
  // the beginning ant the end of the word.
  // Allocate enough space for regular expression
  char * reg = (char*) malloc(2 * strlen(arg->c_str()) + 10);
  char a[arg->length()];
  strcpy(a, arg->c_str());
  char * r = reg;
  int r_num = 0;
  int a_num = 0;
  r[r_num] = '^';
  r_num++; // match beginning of line
  // fprintf(stderr, "a: %c\n", *a);
  // fprintf(stderr, "arg: %s\n", arg->c_str());

  while (a[a_num]) {
    // fprintf(stderr, "a[%d]: %c\n", a_num, *a);
    if (a[a_num] == '*') { r[r_num] = '.'; r_num++; r[r_num] = '*'; r_num++; }
    else if (a[a_num] == '?') { r[r_num]='.'; r_num++;}
    else if (a[a_num] == '.') { r[r_num]='\\'; r_num++; r[r_num]='.'; r_num++;}
    else { r[r_num]=a[a_num]; r_num++;}
    a_num++;
  }
  r[r_num]='$'; r_num++; r[r_num]='\0';// match end of line and add null char
  // fprintf(stderr, "r: %s\n", r);

  // 2. compile regular expression. See lab3-src/regular.cc
  regex_t re;	
	int result = regcomp(&re, r,  REG_EXTENDED|REG_NOSUB);
  if (result != 0) {
    perror("Does not match, result is not equal to 0!");
    return;
  }

  // 3. List directory and add as arguments the entries
  // that match the regular expression
  DIR * dir = opendir(".");
  if (dir == NULL) {
    perror("Opendir error!");
    return;
  }

  struct dirent * ent;
  std::vector<std::string> array;
  while ((ent = readdir(dir))!= NULL) {
    // Check if name matches
    regmatch_t match;
    if (regexec(&re, ent->d_name, 1, &match, 0 ) == 0 ) { //ent->d_name, r
      if ((ent->d_name[0] != '.') || (arg->find('.') == 0)) {
        // Add argument
        array.push_back(std::string(ent->d_name));
      }
    }
  }
  closedir(dir);
  std::sort(array.begin(), array.end());
  for (int i = 0; i < array.size(); i++) {
    Command::_currentSimpleCommand->insertArgument(new std::string(array[i]));
  }
}

void expandWildcard(std::string prefix, std::string suffix) {
  if (suffix == "") {
    actual_array.push_back(std::string(prefix));
    return;
  }

  if (prefix == "") {
    if (suffix[0] == '/') {
      expandWildcard("/", suffix.substr(1));
      return;
    }
    else {
      expandWildcard(".", suffix.substr(0));
      return;
    }
  }

  // fprintf(stderr, "the index of '/': %d\n", suffix.find('/'));
  std::string component;
  if (suffix.find('/') != std::string::npos) {
    component = suffix.substr(0, suffix.find('/') + 1);
  }
  else {
    component = suffix;
  }
  // fprintf(stderr, "component: %s\n", component.c_str());
  DIR * dir = opendir(prefix.c_str());
  if (prefix == ".") {
    prefix = "";
  }
  std::string * arg = new std::string(component);
  char * reg = (char*) malloc(2 * strlen(arg->c_str()) + 10);
  char a[arg->length()];
  strcpy(a, arg->c_str());
  char * r = reg;
  int r_num = 0;
  int a_num = 0;
  r[r_num] = '^';
  r_num++; // match beginning of line

  while (a[a_num]) {
    // fprintf(stderr, "a[%d]: %c\n", a_num, *a);
    if (a[a_num] == '*') { r[r_num] = '.'; r_num++; r[r_num] = '*'; r_num++; }
    else if (a[a_num] == '?') { r[r_num]='.'; r_num++;}
    else if (a[a_num] == '.') { r[r_num]='\\'; r_num++; r[r_num]='.'; r_num++;}
    else { r[r_num]=a[a_num]; r_num++;}
    a_num++;
  }
  r[r_num]='$'; r_num++; r[r_num]='\0';// match end of line and add null char
  // fprintf(stderr, "r: %s\n", r);

  // 2. compile regular expression. See lab3-src/regular.cc
  regex_t re;	
	int result = regcomp(&re, r, REG_EXTENDED|REG_NOSUB);
  if (result != 0) {
    perror("Does not match, result is not equal to 0!");
    return;
  }

  struct dirent * ent;
  std::string rest_of_c = suffix.substr(component.length());
  // fprintf(stderr, "rest_of_c: %s\n", rest_of_c.c_str());
  while ((ent = readdir(dir))!= NULL) {
    // // Check if name matches
    // if (advance(ent->d_name, expbuf) ) {
    //   // Entry matches. Add name of entry
    //   // that matches to the prefix and
    //   // call expandWildcard(..) recursively
    //   sprintf(newPrefix,”%s/%s”, prefix, ent->d_name);
    //   expandWildcard(newPrefix,suffix);
    // }
    // Check if name matches
    regmatch_t match;
    std::string new_component = std::string(ent->d_name);
    if (ent->d_type == DT_DIR && component.find('/') != std::string::npos) {
      new_component += "/";
    }
    // fprintf(stderr, "new component: %s\n", new_component.c_str());
    if (regexec(&re, new_component.c_str(), 1, &match, 0) == 0) { //ent->d_name, r
      if ((ent->d_name[0] != '.') || (arg->find('.') == 0)) {
        expandWildcard(prefix + new_component, rest_of_c); // how to get "rest of s"????????????????
      }
    }
  }
  free(r);
  free(arg);
  closedir(dir);
}

%}

%%

goal:
  command_list;

command_line:
  pipe_list io_modifier_list background_optional NEWLINE {
    // printf(" Yacc: Execute command\n");
    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    Shell::prompt();
  } /*accept empty cmd line*/
  | error NEWLINE{ yyerrok; }
  | EXIT {
    if (isatty(0)) {
      printf("Good Bye!!\n");
    }
    exit(1);
  }
  ;

command_list :
  command_line
  | command_list command_line
  ;

io_modifier_options:
  GREAT WORD {
    // printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile != NULL) {
      fprintf(stdout, "%s\n", "Ambiguous output redirect.");
    }
    Shell::_currentCommand._outFile = $2;
  }
  | GREATGREAT WORD {
    // printf("   Yacc: insert output \"%s\"\n", $2->c_str());
     if (Shell::_currentCommand._append != NULL) {
      fprintf(stdout, "%s\n", "Ambiguous output redirect.");
    }
    Shell::_currentCommand._append = $2;
  }
  | GREATAMPERSAND WORD {
    // printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile != NULL || Shell::_currentCommand._errFile != NULL) {
      fprintf(stdout, "%s\n", "Ambiguous output redirect.");
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
  }
  | GREATGREATAMPERSAND WORD {
    // printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._append != NULL || Shell::_currentCommand._errFile != NULL) {
      fprintf(stdout, "%s\n", "Ambiguous output redirect.");
    }
    Shell::_currentCommand._append = $2;
    Shell::_currentCommand._errFile = $2;
  }
  | LESS WORD {
    // printf("   Yacc: insert input \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._inFile != NULL) {
      fprintf(stdout, "%s\n", "Ambiguous output redirect.");
    }
    Shell::_currentCommand._inFile = $2;
  }
  | TWOGREAT WORD {
    // printf("   Yacc: insert input \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._errFile != NULL) {
      fprintf(stdout, "%s\n", "Ambiguous output redirect.");
    }
    Shell::_currentCommand._errFile = $2;
  }
  ;

io_modifier_list:
  io_modifier_list io_modifier_options
  | /* can be empty */
  ;

background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | /* can be empty */
  ;

pipe_list:
  cmd_and_args
  | pipe_list PIPE cmd_and_args
  ;

cmd_and_args:
  cmd arg_list { 
    //printf(" Yacc: current simple command %p\n", Command::_currentSimpleComm);
    Shell::_currentCommand.insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

cmd:
  WORD {
    // printf(" Yacc: insert command \"%s\"\n", $1->c_str());
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

arg_list:
  arg_list arg
  | /* can be empty */
  ;

arg:
  WORD {
    // Command::_currentSimpleCommand->insertArgument( $1 );
    // expandWildcardsIfNecessary($1);
    
    std::string arg = std::string($1->c_str());
    if (((arg.find('*')) != std::string::npos) || ((arg.find('?')) != std::string::npos)) {
      // fprintf(stderr, "Could be here!\n");
      expandWildcard("", std::string($1->c_str()));
      if (actual_array.size() == 0) {
        Command::_currentSimpleCommand->insertArgument( $1 );
      }
      std::sort(actual_array.begin(), actual_array.end());
      for (int i = 0; i < actual_array.size(); i++) {
        Command::_currentSimpleCommand->insertArgument(new std::string(actual_array[i]));
      }
      actual_array.clear();
    }
    else {
      Command::_currentSimpleCommand->insertArgument( $1 );
    }
  }
  ;
%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}



#if 0
main()
{
  yyparse();
}
#endif

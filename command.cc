/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <fcntl.h>

#include "command.hh"
#include "shell.hh"

int status;
int last_process;
std::string last_simple_arg;
// std::string last_command;
// std::vector<std::string> command_table;
// int command_num;

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _append = NULL;
    _background = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    bool diff_out = _errFile != _outFile;
    bool diff_append = _errFile != _append;

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile && diff_out && diff_append) {
        delete _errFile;
    }
    _errFile = NULL;

    if ( _append ) {
        delete _append;
    }
    _append = NULL;

    _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- -----------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Append       Background\n" );
    printf( "  ------------ ------------ ------------ ---------------------------\n" );
    printf( "  %-12s %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _append?_append->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

bool buildinFun_child(char ** parameter) {
    if (!strcmp(parameter[0], "printenv")) {
        int num = 0;
        while (environ[num] != NULL) {
            printf("%s\n", environ[num]);
            num++;
        }
        return true;
    }

    return false;
}

bool buildinFun_parent(char ** parameter) {
    if (!strcmp(parameter[0], "setenv")) {
        setenv(parameter[1], parameter[2], 1);
        // fprintf(stdout, "parameter[0]: %s; parameter[1]: %s; parameter[2]: %s\n", parameter[0], parameter[1], parameter[2]);
        return true;
    }

    if (!strcmp(parameter[0], "unsetenv")) {
        unsetenv(parameter[1]);
        return true;
    }

    if (!strcmp(parameter[0], "cd")) {
        if (parameter[1] != NULL) {
            int answer = chdir(parameter[1]);
            if (answer != 0) {
                fprintf(stderr, "cd: can't cd to %s\n", parameter[1]);
            }
            return true;
        }
        else {
            chdir(getenv("HOME"));
            return true;
        }
    }

    return false;
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    // Print contents of Command data structure
    // print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec
    int ret;
    int tmpin = dup(0); // save the default input
    int tmpout = dup(1); // save the default output
    int tmperr = dup(2); // save the default error output
    int fdin;
    int fdout;
    int fderr;
    if (_inFile) {
        fdin = open(_inFile->c_str(), O_RDONLY, 0400);
    }
    else {
        fdin = dup(tmpin);
    }

    if (_errFile) {
        fderr = open(_errFile->c_str(), O_CREAT | O_APPEND | O_WRONLY, 0600);
    }
    else {
        fderr = dup(tmperr);
    }

    dup2(fderr, 2);
    close(fderr);

    for (int i = 0; i < _simpleCommands.size(); i++) {
        // redirect input 
        dup2(fdin, 0);
        close(fdin);
        //setup output
        if (i == _simpleCommands.size() - 1) { 
            // Last simple command
            if (_outFile) {
                fdout = open(_outFile->c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
            }
            else if (_append) {
                fdout = open(_append->c_str(), O_APPEND | O_WRONLY, 0600);
            }
            else {
                // Use default output
                fdout = dup(tmpout);
            }
        }
        else {
            // Not last
            //simple command
            //create pipe
            int fdpipe[2];
            pipe(fdpipe);
            fdout = fdpipe[1];
            fdin = fdpipe[0];
        }// if/else
        
        // Redirect output
        dup2(fdout, 1);
        close(fdout);

        int num_of_arguments = _simpleCommands[i]->_arguments.size();
        char ** parameter = new char *[num_of_arguments + 1];
        // std::string arguments;
        for (int j = 0; j < num_of_arguments; j++) {
            parameter[j] = (char *) _simpleCommands[i]->_arguments[j]->c_str();
        }
        last_simple_arg = parameter[num_of_arguments - 1];
        parameter[num_of_arguments] = NULL;

        if (buildinFun_parent(parameter)) {
            dup2(tmpin, 0);
            dup2(tmpout, 1);
            dup2(tmperr, 2);
            close(tmpin);
            close(tmpout);
            close(tmperr);
            if (!_background) {
                int s;
                waitpid(ret, &s, 0);
                status = WEXITSTATUS(s);
            }

            if (_background) {
                last_process = ret;
            }

            clear();
            Shell::prompt();

            return;
        }

        ret = fork();

        if (ret == 0) { // child process
            if (buildinFun_child(parameter)) {
                exit(0);
            }
            execvp(parameter[0], parameter);
            perror("The execvp function went wrong!");
            exit(1);
        }
        else if (ret < 0) {
            perror("The fork process is wrong!");
            return;
        }

        delete parameter;
    }

    //restore in/out defaults
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    dup2(tmperr, 2);
    close(tmpin);
    close(tmpout);
    close(tmperr);
    int return_value = 0;
    if (!_background) {
        int s;
        return_value = waitpid(ret, &s, 0);
        // fprintf(stderr, "Here!");
        status = WEXITSTATUS(s);
        // fprintf(stderr, "command.cc status: %d\n", status);
    }
    if (_background) {
        last_process = ret;
    }
    if (return_value != 0) {
        char * error = getenv("ON_ERROR");
        if (error != NULL) {
            printf(error);
        }
    }
    // Clear to prepare for next command
    
    clear();

    // Print new prompt
    Shell::prompt();
    // fprintf(stderr, "There!");
}

SimpleCommand * Command::_currentSimpleCommand;

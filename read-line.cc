/*
 * CS252: Systems Programming
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <algorithm>
#include <sys/types.h>
#include <dirent.h>
#include <regex.h>
#include <stdlib.h>
#include <cstddef>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <string.h>

#define MAX_BUFFER_LINE 2048

extern "C" void tty_raw_mode(void);
extern "C" void exit_tty_mode(void);

// Buffer where line is stored
int line_length;
char line_buffer[MAX_BUFFER_LINE];
int mark;

// Simple history array
// This history does not change. 
// Yours have to be updated.
int history_index = 0;
std::vector<std::string> history;
int history_length;
std::string last_history;

void cursor_forward(void) {
  char * tmp = "\033[1C";
  write(1, tmp, strlen(tmp));
}

void cursor_backward(void) {
  char * tmp = "\033[1D";
  write(1, tmp, strlen(tmp));
}

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  tty_raw_mode();

  line_length = 0;

  mark = 0;

  // Read one line until enter is typed
  while (1) {
    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch>=32 && ch != 127) {
      // It is a printable character. 
      if (mark == line_length) {
        // Do echo
        write(1, &ch, 1);

        // If max number of character reached return.
        if (line_length==MAX_BUFFER_LINE-2) break; 

        // add char to buffer.
        line_buffer[line_length]=ch;
        line_length++;
        mark++;
      }
      else {
        // mark is at the middle of the array
        // we need to insert!
        char line_buffer_after[line_length - mark];
        int mark_for_after = 0;
        for (int i = mark; i < line_length; i++) {
          line_buffer_after[mark_for_after] = line_buffer[i];
          mark_for_after++;
        }
        line_buffer[mark] = ch;
        mark_for_after = 0;
        for (int i = mark + 1; i < line_length + 1; i++) {
          line_buffer[i] = line_buffer_after[mark_for_after];
          mark_for_after++;
        }

        // fprintf(stderr, "\nline_buffer: %s\n", line_buffer);

        for (int i = 0; i < line_length - mark; i++) {
          cursor_forward();
        }
        // remove all the characters in the screen
        char my_delete = 8;
        char space = ' ';
        // fprintf(stderr, "line length: %d\n", line_length);
        for (int i = 0; i < line_length; i++) {
          write(1, &my_delete, 1);
          write(1, &space, 1);
          write(1, &my_delete, 1);
        }
        // write all the new characters in the screen
        write(1, line_buffer, line_length + 1);
        for (int i = 0; i < line_length - mark; i++) {
          cursor_backward();
        }
        // fprintf(stderr, "new_line_buffer: %s\n", line_buffer);
        line_length++;
        mark++;
      }
    }
    else if (ch==10) {
      // <Enter> was typed. Return line
      
      // Print newline
      write(1, &ch, 1);
      if (line_length == 0) {
        break;
      }
      line_buffer[line_length] = '\0';
      history.push_back(std::string(line_buffer));
      history_index++;
      history_length++;
      // fprintf(stderr, "Already been pushed!\n");

      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if (ch == 127 || ch == 8) {
      if (line_length > 0 && mark == line_length) {
        // <backspace> was typed. Remove previous character read.

        // Go back one character
        ch = 8;
        write(1,&ch,1);

        // Write a space to erase the last character read
        ch = ' ';
        write(1,&ch,1);

        // Go back one character
        ch = 8;
        write(1,&ch,1);

        // Remove one character from buffer
        line_length--;
        mark--;
      }
      else if (mark > 0 && line_length > 0 && mark != line_length) {
        // mark is at the middle of the line

        // delete one char at mark
        for (int i = mark - 1; i < line_length - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }

        for (int i = 0; i < line_length - mark; i++) {
          cursor_forward();
        }
        // remove all the characters in the screen
        char my_delete = 8;
        char space = ' ';
        // fprintf(stderr, "line length: %d\n", line_length);
        for (int i = 0; i < line_length; i++) {
          write(1, &my_delete, 1);
          write(1, &space, 1);
          write(1, &my_delete, 1);
        }
        // write all the new characters in the screen
        write(1, line_buffer, line_length - 1);
        for (int i = 0; i < line_length - mark; i++) {
          cursor_backward();
        }

        line_length--;
        mark--;
      }
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1; 
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && ch2==65) {
        // fprintf(stderr, "history_index: %d history_length: %d\n", history_index, history_length);
        // Up arrow. Print prev line in history.

        // Erase old line
        // Print backspaces
        // if (history_index == -1) {
        //   continue;
        // }
        if (history_index == history_length) {
          line_buffer[line_length] = 0;
          last_history = std::string(line_buffer);
        }

        int i = 0;
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }	

        // Copy line from history
        if (history_index > 0) {
          history_index--;
          strcpy(line_buffer, history[history_index].c_str());
          line_length = strlen(line_buffer);
          mark = line_length;
          // fprintf(stderr, "history index: %d\n", history_index);
          // fprintf(stderr, "history length: %d\n", history_length);
        }
        // echo line
        // fprintf(stderr, "history_index: %d\n", history_index);
        write(1, line_buffer, line_length);
      }
      else if (ch1 == 91 && ch2 == 66) {
        // fprintf(stderr, "history_index: %d history_length: %d\n", history_index, history_length);
        // Down arrow. Print next line in history.

        // Erase old line
        // Print backspaces
        int i = 0;
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }
        //if (history_index == -1 && history[1].c_str() != NULL) {
          // fprintf(stderr, "history[1]: %s", history[1].c_str());
        //  strcpy(line_buffer, history[1].c_str());
        //  line_length = strlen(line_buffer);
        //   mark = line_length;
        //  history_index += 2;
        //  write(1, line_buffer, line_length);
        // }
        // else if (history_index == -1 && history[1].c_str() == NULL) {
        //   strcpy(line_buffer, last_history.c_str());
        //   line_length = strlen(line_buffer);
        //   mark = line_length;
        //   write(1, line_buffer, line_length);
        // }
        if (history_index < history_length - 1) {
          // fprintf(stderr, "this!\n");
          strcpy(line_buffer, history[history_index + 1].c_str());
          line_length = strlen(line_buffer);
          mark = line_length;
          // fprintf(stderr, "history index: %d\n", history_index);
          // fprintf(stderr, "history length: %d\n", history_length);
          history_index = history_index + 1;
          write(1, line_buffer, line_length);
        }
        else if (history_index == history_length - 1) {
          // fprintf(stderr, "that!\n");
          strcpy(line_buffer, last_history.c_str());
          line_length = strlen(line_buffer);
          mark = line_length;
          write(1, line_buffer, line_length);
          // line_length = 0;
          // mark = 0;
          history_index++;
        }
      } 
      else if (ch1 == 91 && ch2 == 68) {
        // left arrow key
        if (mark > 0) {
          mark--;
          cursor_backward();
        }
      }
      else if (ch1 == 91 && ch2 == 67) {
        // right arrow key
        if (mark < line_length) {
          mark++;
          cursor_forward();
        }
      }
      else if (ch1 == 91 && ch2 == 51) {
        char fourth;
        read(0, &fourth, 1);
        if (fourth == 126) {
          // delete button
          if (mark != line_length && mark >= 0) {
            // fprintf(stderr, "could be here!");
            for (int i = mark; i < line_length - 1; i++) {
              line_buffer[i] = line_buffer[i + 1];
            }
            for (int i = 0; i < line_length - mark; i++) {
              cursor_forward();
            }
            // remove all the characters in the screen
            char my_delete = 8;
            char space = ' ';
            // fprintf(stderr, "line length: %d\n", line_length);
            for (int i = 0; i < line_length; i++) {
              write(1, &my_delete, 1);
              write(1, &space, 1);
              write(1, &my_delete, 1);
            }
            // write all the new characters in the screen
            write(1, line_buffer, line_length - 1);
            for (int i = 0; i < line_length - mark - 1; i++) {
              cursor_backward();
            }

            line_length--;
            // mark--;
          }
        }
      }
    }
    else if (ch == 1) {
      // Home key (ctrl-A): The cursor moves to the beginning of the line
      for (int i = 0; i < mark; i++) {
        cursor_backward();
      }
      mark = 0;
    }
    else if (ch == 5) {
      // End key (ctrl-E): The cursor moves to the end of the line
      for (int i = mark; i < line_length; i++) {
        cursor_forward();
      }
      mark = line_length;
    }
    else if (ch == 9) {
      // path completion: <tab>

      // get all characters after latest space => chrs
      int lastest_space = 0;
      for (int i = line_length - 1; i >= 0; i--) {
        if (line_buffer[i] == ' ') {
          lastest_space = i;
          break;
        }
      }
      if (lastest_space == 0) {
        continue;
      }
      // fprintf(stderr, "lastest space: %d\n", lastest_space);
      std::string chrs;
      std::string command_name;
      for (int i = 0; i < lastest_space; i++) {
        // fprintf(stderr, "line_buffer[%d] = %c\n", i, line_buffer[i]);
        command_name.push_back(line_buffer[i]);
        // fprintf(stderr, "command_name: %s\n", command_name);
      }
      // fprintf(stderr, "command name: %s\n", command_name.c_str());
      chrs.push_back('^');
      for (int i = lastest_space + 1; i < line_length; i++) {
        chrs.push_back(line_buffer[i]);
      }
      chrs.push_back('.');
      chrs.push_back('*');
      chrs.push_back('$');
      chrs.push_back('\0');
      // fprintf(stderr, "chrs: %s\n", chrs.c_str());
      // expand this wildcard chars* => list
      regex_t re;
      int result = regcomp(&re, chrs.c_str(), REG_EXTENDED|REG_NOSUB);
      if (result != 0) {
        continue;
      }

      DIR * dir = opendir(".");
      if (dir == NULL) {
        perror("Opendir failed!");
        continue;
      }

      struct dirent * ent;
      std::vector<std::string> array;
      while ((ent = readdir(dir))!= NULL) {
        // Check if name matches
        regmatch_t match;
        if (regexec(&re, ent->d_name, 1, &match, 0 ) == 0 ) { //ent->d_name, r
          if ((ent->d_name[0] != '.') || (std::string(chrs).find('.') == 0)) {
            // Add argument
            array.push_back(std::string(ent->d_name));
          }
        }
      }
      closedir(dir);
      std::sort(array.begin(), array.end());
      // for (int i = 0; i < array.size(); i++) {
      //   fprintf(stderr, "array[%d]: %s\n", i, array[i].c_str());
      // }
      // find longest matching string in list => lms
      int min = MAX_BUFFER_LINE;
      for (int i = 0; i < array.size(); i++) {
        if (min > array[i].length()) {
          min = array[i].length();
        }
      }
      int lms_length = 0;
      for (int i = 0; i < min; i++) {
        for (int j = 0; j < array.size() - 1; j++) {
          // fprintf(stderr, "i: %d\nj: %d\n", i, j);
          if (array[j][i] != array[j + 1][i]) {
            lms_length = i - 1;
            break;
          }
        }
      }
      if (array.size() == 1) {
        lms_length = array[1].length();
      }
      std::string lms = array[0].substr(0, lms_length);
      // print lms to screen
      for (int i = 0; i < line_length; i++) {
        ch = 8;
        write(1,&ch,1);
      }

      // Print spaces on top
      for (int i = 0; i < line_length; i++) {
        ch = ' ';
        write(1,&ch,1);
      }

      // Print backspaces
      for (int i = 0; i < line_length; i++) {
        ch = 8;
        write(1,&ch,1);
      }

      std::string final_answer = command_name + " " + lms;
      write(1, final_answer.c_str(), final_answer.length());
      strcpy(line_buffer, final_answer.c_str());
      line_length = final_answer.length();
      mark = line_length;
    }
  }

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;

  exit_tty_mode();
  
  return line_buffer;
}


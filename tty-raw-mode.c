
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>

/* 
 * Sets terminal into raw mode. 
 * This causes having the characters available
 * immediately instead of waiting for a newline. 
 * Also there is no automatic echo.
 */
struct termios tty_attr;
void tty_raw_mode(void)
{
	tcgetattr(0,&tty_attr);

	struct termios new_tty_attr;
	tcgetattr(0,&new_tty_attr);
	/* Set raw mode. */
	new_tty_attr.c_lflag &= (~(ICANON|ECHO));
	new_tty_attr.c_cc[VTIME] = 0;
	new_tty_attr.c_cc[VMIN] = 1;
     
	tcsetattr(0,TCSANOW,&new_tty_attr);
}

void exit_tty_mode(void) {
	tcsetattr(0, TCSANOW, &tty_attr);
}
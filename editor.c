/**
 * Simple Text Editor from scratch specifically (at least for now) for UNIX based system
 *
 */

/*************** Includes  *****************/
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

/****************** Defines *****************/
#define CTRL_KEY(k) ((k) & 0x1f) // This macro's values varies from 0 - 31 or 0x00 - 0x1f regardless UPPER or LOWER cases.


/************** Global Data******************/
typedef struct{
	struct termios original_terminal_settings;
	int lines;
	int columns;
	int cursor_x;
	int cursor_y;

}global_state;

global_state globalState;

typedef struct{
	char *buffer;
	int buffer_length;
	
}append_buffer;


/************ Functions Declaration **************/
void enterRawMode();
void exitRawMode();
void die(char *err);
void clearScreen();
void refreshScreen();
void processUserKeys();
void getTerminalSize(int *lines, int *cols);
void abbendBufferInit(append_buffer *apBuffer);
void appendBufferAppend(append_buffer *apBuffer, char *s, int length);
void freeBuffer(append_buffer *apBuffer);
void drawTeldas();
void getCursorPosition(int *cursor_x, int *cursor_y);
char readUserKeys();


/******** INIT *********/
int main(){

	enterRawMode();
	getTerminalSize(&globalState.lines, &globalState.columns);

	while(1){
		refreshScreen();
		processUserKeys();
	}


	/* write(STDOUT_FILENO, "\x1b[2J", 4); // Clearing the terminal screen -> throws cursor down
	write(STDOUT_FILENO, "\x1b[1;1H", 6); // repositons the cursor the row 1 column 1 position

	getCursorPosition(&globalState.cursor_x, &globalState.cursor_y);
	printf("\r\nYour cursor is %d x and %d y", globalState.cursor_x, globalState.cursor_y); */
	return 0;
}




/******** Functions Definitions *********/

/*====== Raw Mode =====*/

void enterRawMode(){

	if(tcgetattr(STDIN_FILENO, &globalState.original_terminal_settings) == -1){
		die("tcgetattr failure at enterring raw mode");
	}
	//printf("\n%b\n", ECHO);
	//printf("%b", terminal_settings.c_lflag);
	struct termios terminal_settings = globalState.original_terminal_settings;

	terminal_settings.c_lflag &=  ~(ECHO | ICANON | IEXTEN | ISIG);// Disabling terminal ECHOING , CANONICAL, IEXTEN and ISIG Mode.
	terminal_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disabling Software Control Flow {CTRL-S, CTRL-Q}
	terminal_settings.c_oflag &= ~(OPOST); // Diabling post-processing of output like "\n" -> "\r\n" 
	terminal_settings.c_cflag |= CS8; // Setting character size to 8 bit per Byte.
	
	terminal_settings.c_cc[VMIN] = 0;  // Set minimum number of characters to 0
	terminal_settings.c_cc[VTIME] = 1;  // Set timeout to 0.1 seconds

	/**
	 * For more info on the above modes check out the link below or type `man tcgetattr` in the terminal.
	 * https://www.gnu.org/software/libc/manual/html_node/Local-Modes.html
	 * 
	 */
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal_settings) == -1){
		die("tcsetattr failure at enterring raw mode");

	}
	atexit(exitRawMode); // Takes function pointer as an argument
}

void exitRawMode(){

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &globalState.original_terminal_settings) == -1){
		die("tcsetattr failure at exitting raw mode");
	}	
}


/*====== User Data read & process =====*/

char readUserKeys(){
	int readReturn;
	char c;
	while((readReturn = read(STDIN_FILENO, &c, 1)) != 1){
		if(readReturn == -1){
			die("read failure in readUserKeys");
		}
	}
	return c;
}

void processUserKeys(){
	
	char c = readUserKeys();

	switch(c){
		case CTRL_KEY('q'):
			clearScreen();
			exit(4);
			break;
	}

}

/*====== Error Handlers =====*/
void die(char *err){
	
	perror(err);
	exit(1);

	// Clearing terminal on exit
	clearScreen();	
}

/*===== Clear & Refresh terminal =====*/
void clearScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4); // Clearing the terminal screen -> throws cursor down
	write(STDOUT_FILENO, "\x1b[1;1H", 6); // repositons the cursor the row 1 column 1 position

}

void refreshScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4); // Clearing the terminal screen -> throws cursor down
	write(STDOUT_FILENO, "\x1b[1;1H", 6); // repositons the cursor the row 1 column 1 position
	/**
	 * Clearing terminal display manually and then repositioning cursor to [1,1]
	 * for more info write `man console_codes` in your terminal
     */
	drawTeldas();
	//write(STDOUT_FILENO, "\x1b[1;1H", 6); // repositons the cursor the row 1 column 1 position
	

}

/*====== Getting terminal size ======*/
void getTerminalSize(int *lines, int *cols){
	/*
	 * For the sake of cross compatibility this function checks and uses multible approaches to get terminal size
	 * 1st is querying terminal properties via "ioctl" from <sys/ioctl.h> 
	 * 2nd and the most painful is moving the cursor to the bottom right and using fallback method
	 */
	struct winsize terminalSize;
	if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminalSize) == -1 || terminalSize.ws_col == 0) {
		write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12);
		getCursorPosition(cols, lines);
		
		
	} else {
		*cols = terminalSize.ws_col;
		*lines = terminalSize.ws_row;
		
	}
	
}

/*======= Getting Cursor Curent Position ========*/

void getCursorPosition(int *cursor_x, int *cursor_y){
	// Requesting "Device Status Report" with parameter 6 fr current cursor position
	write(STDOUT_FILENO, "\x1b[6n", 4);

	// printing the output 
	char buff[64];
	long unsigned int index = 0;

	while(index < sizeof(buff) - 1){
		if(read(STDIN_FILENO, &buff[index], 1) != 1){
			die("Cursor Position Report read failure");
		}else if(buff[index] == 'R'){
			index++;
			break;
		}
		index++;
	}
	buff[index] = '\0'; // setting termination point for the string
	
	// Parsing buffer and extracting the coordinates of the cursor
	for(long unsigned int i = 0; i <= sizeof(buff); i++){
		if(buff[0] == '\x1b' && buff[1] == '['){
			if(sscanf(&buff[2], "%d;%d", cursor_y, cursor_x) != 2){
				die("Error parsing current cursor position report");
			}
		}
	}
}

/*======= Terminal Outputs Handlers ========*/
void drawTeldas(){

	for(int i = 0; i < globalState.lines ; i++){
		if(i == globalState.lines - 1){
			write(STDOUT_FILENO, "~", 1);
		}else{
			write(STDOUT_FILENO, "~\r\n", 3);
		}
	}

}

/*======== Append Buffer handlers ========*/
void abbendBufferInit(append_buffer *apBuffer){
	apBuffer->buffer = NULL;
	apBuffer->buffer_length = 0;
}

void appendBufferAppend(append_buffer *apBuffer, char *s, int length){
	char *resized_buffer = realloc(apBuffer->buffer, apBuffer->buffer_length + length);  // ptr to the rellocated memory block
	if(resized_buffer == NULL){
		return;
	}
	memcpy(&resized_buffer[apBuffer->buffer_length], s, length);

	apBuffer->buffer = resized_buffer;
	apBuffer->buffer_length += length;


}
void freeBuffer(append_buffer *apBuffer){
	free(apBuffer);
}

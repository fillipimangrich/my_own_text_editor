#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

//define
#define CTRLKEY(k) ((k) & 0x1f)

//data
struct termios orig_termios;

//terminal

void die(const char *s){
    write(STDIN_FILENO, "\x1b[2J",4);
    write(STDIN_FILENO, "\x1b[H",3);

    perror(s); //print error message
    exit(1); //exit 
}

void disableRawMode(){
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios)==-1)
        die("tcsetattr");
}

void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &orig_termios) ==-1)//get terminal atributes
        die("tcgetattr");

    atexit(disableRawMode); //disable raw mode when the program exits

    struct termios raw = orig_termios;// make a copy of original attributes
    raw.c_oflag &= ~(OPOST); //disable output processing
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON ); //disable ctrl-s and ctrl-q | turn off ctrl-m
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // doesn't print what you are typing | disable canonical mode | turn off ctr-c and ctrl-z |turn off ctrl-v
    raw.c_cc[VMIN] = 0; //minimum of bytes
    raw.c_cc[VTIME] = 1; //time to wait

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) //set new atribute
        die("tcsetattr"); 

}
//output
void editorDrawsRows(){
    int y;
    for (y =0; y < 24; y++){
        write(STDOUT_FILENO, "~\r\n",3); //draw a ~ character on the left of the line
    }
}


void editorRefreshScreen(){
    write(STDIN_FILENO, "\x1b[2J",4);// write 4 bytes in terminal, x1b is the scape character, the other 3 bytes are [2J,
                                    // the J command (erase in display) with 2 argument clear all display
    write(STDIN_FILENO, "\x1b[H",3);//H command change the position of cursor

    editorDrawsRows();

    write(STDIN_FILENO, "\x1b[H", 3);
}

//input

char editorReadKey(){ //read keys and return
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c,1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

void editorProcessKeyPress(){
    char c = editorReadKey();

    switch(c){
        case CTRLKEY('q'): // ctrl key combinations
        write(STDIN_FILENO, "\x1b[2J",4);
        write(STDIN_FILENO, "\x1b[H",3);
        exit(0);
        break;
    }
}

//init
int main(){
    enableRawMode();

    while (1){
        editorRefreshScreen();
        editorProcessKeyPress();
    } 
    return 0;
}
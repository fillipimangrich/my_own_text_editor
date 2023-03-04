#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>


//define
#define CTRLKEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}
#define MYVIM_VERSION "0.0.1"


//data
struct editorConfig
{
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;
};

struct editorConfig editor_stage;

//terminal

void die(const char *s){
    write(STDIN_FILENO, "\x1b[2J",4);
    write(STDIN_FILENO, "\x1b[H",3);

    perror(s); //print error message
    exit(1); //exit 
}

void disableRawMode(){
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&editor_stage.orig_termios)==-1)
        die("tcsetattr");
}

void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &editor_stage.orig_termios) ==-1)//get terminal atributes
        die("tcgetattr");

    atexit(disableRawMode); //disable raw mode when the program exits

    struct termios raw = editor_stage.orig_termios;// make a copy of original attributes

    raw.c_oflag &= ~(OPOST); //disable output processing
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON ); //disable ctrl-s and ctrl-q | turn off ctrl-m
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // doesn't print what you are typing | disable canonical mode | turn off ctr-c and ctrl-z |turn off ctrl-v
    raw.c_cc[VMIN] = 0; //minimum of bytes
    raw.c_cc[VTIME] = 1; //time to wait

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) //set new atribute
        die("tcsetattr"); 

}

char editorReadKey(){ //read keys and return
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c,1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i =0; 

    if (write(STDOUT_FILENO, "x1b[6n",4) !=4) return -1; //n command (devide status report) recive the 6 argument to ask the cursor position

 
    while(i<sizeof(buf)-1){
        if (read(STDIN_FILENO, &buf[i],1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2],"%d;%d",rows,cols) !=2) return -1;
    editorReadKey();

    return 0;

}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;

    if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) ==-1 || ws.ws_col ==0){ // TIOCGWINSZ =  input/output control, get window size
        if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; //move the cursor to bottom right corner
        return getCursorPosition(rows, cols);
    }
    else{
        *cols = ws.ws_col;
        *rows= ws.ws_row;
        return 0;
    }
}
//append buffer

struct abuf{
    char *b;
    int len;    
};

void abAppend(struct  abuf *ab, const char *s, int len){
    char *new = realloc(ab->b,ab->len + len); //alloc the new string (size is the len of current string  + the new string)

    if (new == NULL) return;
    memcpy(&new[ab->len],s,len); //copy the string s and update the pointer and len
    ab->b = new;
    ab->len += len;
}

void abFree(struct  abuf *ab){
    free(ab->b); //destructor
}



//output
void editorDrawsRows(struct abuf *ab){
    int y;
    for (y =0; y < editor_stage.screen_rows; y++){
        if (y == editor_stage.screen_rows/3){
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "myvim editor -- version %s", MYVIM_VERSION);
            if (welcomelen > editor_stage.screen_cols) welcomelen = editor_stage.screen_cols;
            int padding = (editor_stage.screen_cols - welcomelen)/2;
            if (padding){
                abAppend(ab,"~",1);
                padding--;
            }
            while(padding--) abAppend(ab," ",1);
            abAppend(ab,welcome,welcomelen);
        }
        else{
            abAppend(ab,"~",1); //draw a ~ character on the left of the line
        }

        abAppend(ab, "\x1b[K",3); //erase in line

        if (y < editor_stage.screen_rows -1){
            abAppend(ab,"\r\n",2);
        }
    }
}


void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT; //init new buffer


    abAppend(&ab, "\x1b[?25l",6);//reset mode

    //abAppend(&ab, "\x1b[2J",4);// write 4 bytes in terminal, x1b is the scape character, the other 3 bytes are [2J,
    //                                 // the J command (erase in display) with 2 argument clear all display

    abAppend(&ab, "\x1b[H",3);//H command change the position of cursor

    editorDrawsRows(&ab);

    abAppend(&ab, "\x1b[H",3);
    abAppend(&ab, "\x1b[?25h",6);//set mode

    write(STDOUT_FILENO, ab.b,ab.len);

    abFree(&ab);    
}

//input

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
void initEditor(){
    if (getWindowSize(&editor_stage.screen_rows, &editor_stage.screen_cols) == -1) die("getWindowSize");
}

int main(){
    enableRawMode();
    initEditor();

    while (1){
        editorRefreshScreen();
        editorProcessKeyPress();
    } 
    return 0;
}
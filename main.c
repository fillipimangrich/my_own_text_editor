#define _OPEN_SYS_ITOA_EXT
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>


//define
#define CTRLKEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}
#define MYVIM_VERSION "0.0.1"

enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT ,
    ARROW_UP ,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

//data
typedef struct erow
{
    int size;
    char *chars;
}erow;


struct editorConfig
{
    int cx,cy;
    int screen_rows;
    int screen_cols;
    int numrows;
    erow *row;
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

int editorReadKey(){ //read keys and return
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c,1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    if (c == '\x1b'){ //parse the arrow key and return the corresponding letter to move the cursor
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if(seq[0] == '['){
            if (seq[1] >= '0' && seq[1] <='9'){
                if (read(STDIN_FILENO, &seq[2],1) !=1) return '\x1b';
                if (seq[2] == '~'){
                    switch (seq[1])
                    {   
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                } 
            } else {
                switch (seq[1]){
                    case 'A':return ARROW_UP;
                    case 'B':return ARROW_DOWN;
                    case 'C':return ARROW_RIGHT;
                    case 'D':return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        
        }else if(seq[0] == '0'){
            switch (seq[1])
            {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;    
            }
        }
        return '\x1b';
    }
    else{
        return c;
    }
    
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i =0; 

    if (write(STDOUT_FILENO, "\x1b[6n",4) !=4) return -1; //n command (devide status report) recive the 6 argument to ask the cursor position

 
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

/*** row operations ***/
void editorAppendRow(char *s, size_t len) {
  editor_stage.row = realloc(editor_stage.row, sizeof(erow) * (editor_stage.numrows + 1));
  int at = editor_stage.numrows;
  editor_stage.row[at].size = len;
  editor_stage.row[at].chars = malloc(len + 1);
  memcpy(editor_stage.row[at].chars, s, len);
  editor_stage.row[at].chars[len] = '\0';
  editor_stage.numrows++;
}

//file I/O

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  
  if (!fp){
    die("fopen");
  } 

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  linelen = getline(&line, &linecap, fp);
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
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
void editorDrawRows(struct abuf *ab){
    int y;
    for (y =0; y < editor_stage.screen_rows; y++){
        if (y >= editor_stage.numrows) {
            char snum[5];
            int num_len = snprintf(snum, sizeof(snum), "%d", y);
            if (editor_stage.numrows == 0 && y == editor_stage.screen_rows/3){
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "myvim editor -- version %s", MYVIM_VERSION);
                if (welcomelen > editor_stage.screen_cols) welcomelen = editor_stage.screen_cols;
                int padding = (editor_stage.screen_cols - welcomelen)/2;
                if (padding){
                    abAppend(ab, snum , 5);
                    padding--;
                }
                while(padding--) abAppend(ab," ",1);
                abAppend(ab,welcome,welcomelen);
            }
            else{
                abAppend(ab,snum,5); //draw a ~ character on the left of the line
            }
            } 
        else {
            int len = editor_stage.row[y].size;
            if (len > editor_stage.screen_cols) len = editor_stage.screen_cols;
            abAppend(ab, editor_stage.row[y].chars, len);
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

    editorDrawRows(&ab);

    char buf[32];

    snprintf(buf,sizeof(buf), "\x1b[%d;%dH", editor_stage.cy +1, editor_stage.cx+1); //change the cursor position to value stored in cx and cy
    abAppend(&ab,buf,strlen(buf));

    abAppend(&ab, "\x1b[?25h",6);//set mode

    write(STDOUT_FILENO, ab.b,ab.len);

    abFree(&ab);    
}

//input
void editorMoveCursor(int key){
    switch (key){
        case ARROW_LEFT:
            if (editor_stage.cx !=0){
                editor_stage.cx--;}
            break;
        case ARROW_RIGHT:
            if (editor_stage.cx != editor_stage.screen_cols-1){
                editor_stage.cx++;}
            break;
        case ARROW_UP:
            if (editor_stage.cy != 0){
                editor_stage.cy--;}
            break;
        case ARROW_DOWN:
            if (editor_stage.cy != editor_stage.screen_rows-1){
                editor_stage.cy++;}
            break;
    }
}

void editorProcessKeyPress(){
    int c = editorReadKey();

    switch(c){
        case CTRLKEY('q'): // ctrl key combinations
            write(STDIN_FILENO, "\x1b[2J",4);
            write(STDIN_FILENO, "\x1b[H",3);
            exit(0);
            break;
        case HOME_KEY: //move cursor to the start of line
            editor_stage.cx =0;
            break;
        case END_KEY: //move cursor to the end of line
            editor_stage.cx = editor_stage.screen_cols -1;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
        {
            int times = editor_stage.screen_rows;
            while (times--) // move cursor to top or bottom of the screen
            {
                editorMoveCursor(c == PAGE_UP ? ARROW_UP: ARROW_DOWN); 
            }
            
        }
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

//init
void initEditor(){
    editor_stage.cx =0;
    editor_stage.cy =0;
    editor_stage.numrows = 0;
    editor_stage.row = NULL;

    if (getWindowSize(&editor_stage.screen_rows, &editor_stage.screen_cols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  while (1) {
    editorRefreshScreen();
    editorProcessKeyPress();
  }
  return 0;
}
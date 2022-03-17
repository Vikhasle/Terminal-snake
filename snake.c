#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define HEAD '&'
#define BODY '#'
#define TAIL '*'

struct edit_conf {
    int screenrows;
    int screencols;
    int cx, cy;
    struct termios orig_termios;
};

struct snake {
    int pos_x, pos_y;
    int vel_x, vel_y;
    int len;
    char sym;
    struct snake* child;
    struct snake* parent;
};

struct apple {
    int pos_x, pos_y;
    char sym;
};

struct edit_conf conf;
struct snake snek;
struct apple apple;
struct snake* tail = &snek;
int score = 0;

// Restore the prev. term settings
void disable_raw()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &conf.orig_termios);
    printf("\e[?25h");
}

// Makes the terminal enter raw_mode
void enable_raw()
{
    if (tcgetattr(STDIN_FILENO, &conf.orig_termios) == -1)
        exit(1);
    // Make sure we exit raw mode
    atexit(disable_raw);
    struct termios raw = conf.orig_termios;
    tcgetattr(STDIN_FILENO, &raw);
    // turn off a lot of term. flags
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    // This sets the char size to 8 bits
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0; // Min amount of bytes for read to return
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Reads and returns a char from STDIN
char read_key()
{
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
}

int get_term_size(int* rows, int* cols)
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        return -1;
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

void move(char key)
{
    if (key == 'w' && snek.vel_y != 1) {
        snek.vel_y = -1;
        snek.vel_x = 0;
    } else if (key == 's' && snek.vel_y != -1) {
        snek.vel_y = 1;
        snek.vel_x = 0;
    } else if (key == 'a' && snek.vel_x != 1) {
        snek.vel_x = -1;
        snek.vel_y = 0;
    } else if (key == 'd' && snek.vel_x != -1) {
        snek.vel_x = 1;
        snek.vel_y = 0;
    }
}

void process_key()
{
    char key = read_key();
    switch (key) {
    case 'q':
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    case 'w':
    case 'a':
    case 's':
    case 'd':
        move(key);
        break;
    }
}

void rfrsh_screen()
{

    printf("\33[2J");
    printf("\033[32m\033[0;0H%d\033[0m", score);
    struct snake* ptr = &snek;
    while (ptr != NULL) {
        printf("\033[%d;%dH%c", ptr->pos_y, ptr->pos_x, ptr->sym);
        ptr = ptr->child;
    }
    printf("\033[31;5m\033[%d;%dH%c\033[0m", apple.pos_y, apple.pos_x, apple.sym);
    fflush(stdout);
}

int update()
{
    struct snake* ptr = &snek;

    while (ptr != NULL) {
        ptr->pos_x += ptr->vel_x;
        ptr->pos_y += ptr->vel_y;

        // Warping
        if (ptr->pos_x < 0)
            ptr->pos_x = conf.screencols;
        if (ptr->pos_y < 0)
            ptr->pos_y = conf.screenrows;
        if (ptr->pos_x > conf.screencols)
            ptr->pos_x = 0;
        if (ptr->pos_y > conf.screenrows)
            ptr->pos_y = 0;
        ptr = ptr->child;
    }
    ptr = tail;
    while (ptr != &snek) {
        if (snek.pos_x == ptr->pos_x && snek.pos_y == ptr->pos_y)
            return 0;
        ptr->vel_x = ptr->parent->vel_x;
        ptr->vel_y = ptr->parent->vel_y;
        ptr = ptr->parent;
    }

    if (snek.pos_x == apple.pos_x && snek.pos_y == apple.pos_y) {
        struct snake* new = malloc(sizeof(struct snake));
        new->pos_x = tail->pos_x;
        new->pos_y = tail->pos_y;
        new->vel_x = 0;
        new->vel_y = 0;
        new->sym = TAIL;
        if (tail != &snek)
            tail->sym = BODY;
        tail->child = new;
        new->parent = tail;
        tail = new;
        apple.pos_x = rand() % (conf.screencols - 1) + 1;
        apple.pos_y = rand() % (conf.screenrows - 1) + 1;
        score++;
    }

    return 1;
}

/*** INIT ***/

void initialize()
{
    printf("\e[?25l");
    conf.cx = 0;
    conf.cy = 0;

    get_term_size(&conf.screenrows, &conf.screencols);

    snek.pos_x = rand() % conf.screencols;
    snek.pos_y = rand() % conf.screenrows;
    snek.sym = HEAD; // Can easily animate this

    apple.pos_x = rand() % conf.screencols;
    apple.pos_y = rand() % conf.screenrows;
    apple.sym = '@';
}

void free_snake()
{
    struct snake* ptr = snek.child;

    while (ptr != NULL) {
        struct snake* tmp = ptr->child;
        free(ptr);
        ptr = tmp;
    }
}

int main()
{
    enable_raw();
    initialize();

    while (update()) {
        rfrsh_screen();
        process_key();
        usleep(80000);
    }

    disable_raw();
    free_snake();
    printf("\33[2J");
    printf("\033[0;0HYour final score was:\t%d\n", score);
    return 0;
}

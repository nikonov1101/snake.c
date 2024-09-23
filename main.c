#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <time.h>
#include <unistd.h> //STDIN_FILENO
#include <unistd.h>

#define DIR_TOP 1
#define DIR_RIGHT 2
#define DIR_DOWN 3
#define DIR_LEFT 4

#define clear() printf("\033[H\033[J")
// #define clear() printf("\n")


/*
 * the following terminology is used:
 *  > *draw* adds items to a buffer in memory;
 *  > *render* put the whole buffer on a screen;
 * */
char *fb;
int fb_size; // updates by update_winsize;

void fb_render();
void fb_draw_point(int, int, char);
void setup_timer();
void setup_keyboard();
char dirtoc(int dir);

void game_init();
void game_tick();

////// Global game state, initialized by gam_init //////
uint8_t direction = DIR_RIGHT;
uint16_t score = 0;

uint64_t reframe_speed_ms = 75;
uint16_t snake_sz = 3; // len of the snake, also size of xpos and ypos
uint16_t* xpos; 
uint16_t* ypos; // contains the coordinates of the snake itself
// debug buffer
char* debug_buf;

void timer_handler(int sig) {
  game_tick();
}

struct winsize w;
void update_winsize() {
  if (ioctl(0, TIOCGWINSZ, &w) < 0) {
    perror("handelr: failed to get win size\n");
  }
  fb_size = (w.ws_row - 1) * w.ws_col;
}

void fb_draw_point(int x, int y, char c) {
  int pos = y * w.ws_col + x;
  fb[pos] = c;
}

// fb_add_frame adds a frame to a global buffer
void fb_draw_frame() {
  for (int col = 0; col < w.ws_col - 1; col++) {
    fb_draw_point(col, 0, '=');
    fb_draw_point(col, w.ws_row - 2, '=');
  }
  for (int row = 0; row < w.ws_row; row++) {
    fb_draw_point(0, row, '+');
    fb_draw_point(w.ws_col - 1, row, '+');
  }
}


void fb_empty() {
  int pos = 0;
  for (int row = 0; row < w.ws_row - 1; row++) {
    for (int col = 0; col < w.ws_col; col++) {
      pos = row * w.ws_col + col;
      fb[pos] = ' '; // fill buffer with space, start with blank screen
    }
  }

  // draw a frame around the empty screen
  fb_draw_frame();
}

// fb_init allocates a malloc-s buffer data,
// fills it with empty characters,
// and a frame around the screen.
void fb_init() {
  update_winsize();
  // allocate buffer for each char on a screen
  fb = (char *)malloc(sizeof(char) * fb_size);
  bzero(fb, fb_size);
  // also allocade buffer for debug output in the status-bar
  debug_buf = (char *)malloc(sizeof(char) * 1025);
  bzero(debug_buf, 1025);

  fb_empty();
}

void fb_render() {
  int pos = 0;
  clear();
  for (int row = 0; row < w.ws_row - 1; row++) {
    for (int col = 0; col < w.ws_col; col++) {
      pos = row * w.ws_col + col;
      printf("%c", (char)fb[pos]);
    }
    printf("\n");
  }
  // we actually have a buffer one line smaller than a screen,
  // let's use it as a status bar. No \r\n here because the screen will be fully redrawn
  uint16_t x = xpos[snake_sz-1];
  uint16_t y = ypos[snake_sz-1];
  printf("head [%u:%u] {%d:%d} dir=%c score=%u | %s", x, y, w.ws_col, w.ws_row, dirtoc(direction), score,
         debug_buf);
}

uint8_t read_dir_key_blocking() {
  int userin = getchar();
  switch (userin) {
  case 'w':
    return DIR_TOP; // top
  case 'a':
    return DIR_LEFT; // left
  case 's':
    return DIR_DOWN; // down
  case 'd':
    return DIR_RIGHT; // right
  }
  // return the previous value
  return direction;
}

int main() {
  game_init();
  fb_init();
  setup_keyboard();

  setup_timer();
  game_tick(); // draw the first frame

  for (;;) {
    // just update a direction, everything else is done by a timer
    direction = read_dir_key_blocking();
  }
}

void setup_timer() {
  struct sigaction sa;
  struct sigevent sev;
  struct itimerspec its;
  timer_t timerid;

  // Set up the signal handler
  sa.sa_flags = SA_SIGINFO;
  sa.sa_handler = timer_handler;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // Set up the timer event to signal SIGALRM
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  sev.sigev_value.sival_ptr = &timerid;

  // Create the timer
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("timer_create");
    exit(EXIT_FAILURE);
  }

  its.it_value.tv_sec = 0;          // Initial timer expiration
  its.it_value.tv_nsec = reframe_speed_ms * 1000000; // 250ms
  its.it_interval.tv_sec = 0;       // Subsequent intervals
  its.it_interval.tv_nsec = reframe_speed_ms * 1000000;

  // Start the timer
  if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("timer_settime");
    exit(EXIT_FAILURE);
  }
}

void setup_keyboard() {
  struct termios term_io;

  // Get the current terminal attributes and store them in orig_termios
  tcgetattr(STDIN_FILENO, &term_io);

  // Disable canonical mode and echo
  term_io.c_lflag &= ~(ICANON | ECHO);

  // Set the terminal attributes with the modified settings
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_io);
}

char dirtoc(int dir) {
  switch (dir) {
  case DIR_TOP:
    return '^';
  case DIR_RIGHT:
    return '>';
  case DIR_DOWN:
    return 'v';
  case DIR_LEFT:
    return '<';
  }
  return '?';
}

void game_init() {
  direction = DIR_RIGHT;
  snake_sz = 5;

  xpos = (uint16_t *)malloc(sizeof(uint16_t) * fb_size);
  ypos = (uint16_t *)malloc(sizeof(uint16_t) * fb_size);
  bzero(xpos, fb_size);
  bzero(ypos, fb_size);

  // set the initial position
  for (uint16_t i = 0; i<snake_sz; i++) {
    xpos[i] = 7+i;
    ypos[i] = 5;
  }
}

void game_move() {
  // move snake one position forward 
  for (uint16_t i = 0; i< snake_sz-1; i++) {
    xpos[i] = xpos[i+1];
    ypos[i] = ypos[i+1];
  }

  // update latest {x,y} according to current direction
  uint16_t xx = xpos[snake_sz-1];
  uint16_t yy = ypos[snake_sz-1];
  switch (direction) {
    case DIR_TOP: // top
      if (yy-- == 1) {
        // -2 because of border+status bar at the bottom
        yy = w.ws_row - 3;
      }
      break;
    case DIR_DOWN: // down
      if (yy++ == w.ws_row - 3) {
        yy = 1;
      }
      break;
    case DIR_RIGHT: // right
      if (xx++ == w.ws_col - 2) {
        xx = 1;
      }
      break;
    case DIR_LEFT: // left
      if (xx-- == 1) {
        xx = w.ws_col - 2;
      }
      break;
    }
  xpos[snake_sz-1] = xx;
  ypos[snake_sz-1] = yy;
}

void game_draw_snake() {
  // draw body
  for (uint64_t i=0; i<snake_sz; i++) {
     fb_draw_point(xpos[i], ypos[i], '#');
  }

  // draw head using another char
  fb_draw_point(xpos[snake_sz-1], ypos[snake_sz-1], '@');
}

void game_tick() {
  game_move();

  fb_empty();
  game_draw_snake();
  fb_render();
}


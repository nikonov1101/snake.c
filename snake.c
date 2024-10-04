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

#include "screenbuf.h"

#define DIR_TOP 1
#define DIR_RIGHT 2
#define DIR_DOWN 3
#define DIR_LEFT 4

int game_init();
void game_setup_timer();
void game_setup_keyboard();
void game_tick();
void game_grow();
void game_gen_loot();

char dirtoc(int dir);

////// Global game state, initialized by game_init //////
uint8_t direction = DIR_RIGHT;
uint16_t score = 0;

uint64_t reframe_speed_ms = 72;
uint16_t snake_sz; // len of the snake, also size of xpos and ypos
uint16_t *xpos;
uint16_t *ypos; // contains the coordinates of the snake itself

uint16_t loot_x;
uint16_t loot_y;

uint16_t randn(uint16_t min, uint16_t max) {
  return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

// ticks every reframe_speed_ms milliseconds,
// so moves the snake.
void game_timer_handler(int sig) { game_tick(); }

uint8_t read_direction_key() {
  int userin = getchar();
  switch (userin) {
  case 'w':
    return DIR_TOP;
  case 'a':
    return DIR_LEFT;
  case 's':
    return DIR_DOWN;
  case 'd':
    return DIR_RIGHT;
  }
  // return the previous value
  return direction;
}

static screen game_screen;

int main() {
  if (screen_init(&game_screen) != 0) {
    perror("screen: init failed\n");
    return 1;
  }

  // allocate buffer for each char on a screen
  game_screen.buf = (char *)malloc(sizeof(char) * game_screen.size);
  if (game_screen.buf == NULL) {
    perror("screen: alloc failed\n");
    return -1;
  }

  screen_flush(&game_screen);
  screen_render(&game_screen);

  if (game_init() == -1) {
    perror("game init failed");
    return -1;
  }

  game_setup_keyboard();
  game_setup_timer();

  game_tick(); // draw the first frame
  for (;;) {
    // just update a direction, everything else is done by a timer
    direction = read_direction_key();
  }
}

void game_setup_timer() {
  struct sigaction sa;
  struct sigevent sev;
  struct itimerspec its;
  timer_t timerid;

  // Set up the signal handler
  sa.sa_flags = SA_SIGINFO;
  sa.sa_handler = game_timer_handler;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // Set up the timer event to signal SIGUSR1
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGUSR1;
  sev.sigev_value.sival_ptr = &timerid;

  // Create the timer
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("timer_create");
    exit(EXIT_FAILURE);
  }

  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = reframe_speed_ms * 1000000;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = reframe_speed_ms * 1000000;

  // Start the timer
  if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("timer_settime");
    exit(EXIT_FAILURE);
  }
}

// switch terminal into a raw mode,
// so keypresses are delivered immediately, without pressing ENTER
// (I've chatGPTed this, sorry).
void game_setup_keyboard() {
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

int game_init() {
  // get game_gen_loot
  srand(time(NULL));

  direction = DIR_RIGHT;
  snake_sz = 7;

  xpos = (uint16_t *)malloc(sizeof(uint16_t) * game_screen.size);
  if (xpos == NULL) {
    perror("xpos: malloc failed");
    return -1;
  }

  ypos = (uint16_t *)malloc(sizeof(uint16_t) * game_screen.size);
  if (xpos == NULL) {
    perror("ypos: malloc failed");
    return -1;
  }

  bzero(xpos, sizeof(uint16_t) * game_screen.size);
  bzero(ypos, sizeof(uint16_t) * game_screen.size);

  // set the initial position
  for (uint16_t i = 0; i < snake_sz; i++) {
    xpos[i] = 7 + i;
    ypos[i] = 5;
  }

  game_gen_loot();
  return 0;
}

void game_gen_loot() {
  // todo: do not generate an item on the snake itself
  loot_x = randn(1, game_screen.cols - 1);
  loot_y = randn(1, game_screen.rows - 2);
}

void game_move() {
  // move snake one position forward
  for (uint16_t i = 0; i < snake_sz - 1; i++) {
    xpos[i] = xpos[i + 1];
    ypos[i] = ypos[i + 1];
  }

  // update latest {x,y} according to current direction
  uint16_t xx = xpos[snake_sz - 1];
  uint16_t yy = ypos[snake_sz - 1];
  switch (direction) {
  case DIR_TOP:
    if (yy-- == 1) {
      // -2 because of border+status bar at the bottom
      yy = game_screen.rows - 3;
    }
    break;
  case DIR_DOWN:
    if (yy++ == game_screen.rows - 3) {
      yy = 1;
    }
    break;
  case DIR_RIGHT:
    if (xx++ == game_screen.cols - 2) {
      xx = 1;
    }
    break;
  case DIR_LEFT:
    if (xx-- == 1) {
      xx = game_screen.cols - 2;
    }
    break;
  }
  xpos[snake_sz - 1] = xx;
  ypos[snake_sz - 1] = yy;
  if (xx == loot_x && yy == loot_y) {
    game_gen_loot();
    game_grow();
  }
}

void game_grow() {
  score++;
  snake_sz++;
  xpos[snake_sz - 1] = xpos[snake_sz - 2];
  ypos[snake_sz - 1] = ypos[snake_sz - 2];
}

void game_draw_snake() {
  // draw body
  for (uint16_t i = 0; i < snake_sz; i++) {
    screen_draw_point(&game_screen, xpos[i], ypos[i], '#');
  }

  // draw head using another char
  screen_draw_point(&game_screen, xpos[snake_sz - 1], ypos[snake_sz - 1], '@');
}

void game_draw_loot() { screen_draw_point(&game_screen, loot_x, loot_y, '%'); }

void game_print_status_bar() {
  uint16_t x = xpos[snake_sz - 1];
  uint16_t y = ypos[snake_sz - 1];
  printf("head [%u:%u] {%d:%d @ %u} loot=%u:%u dir=%c score=%u  ", x, y,
         game_screen.cols, game_screen.rows, game_screen.size, loot_x, loot_y,
         dirtoc(direction), score);
}

void game_tick() {
  screen_flush(&game_screen);

  game_move();
  game_draw_snake();
  game_draw_loot();

  screen_render(&game_screen);
  game_print_status_bar();
}

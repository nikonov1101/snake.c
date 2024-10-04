#include "screenbuf.h"
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>

#define clear() printf("\033[H\033[J")

// screen_get_size return struct screen with sizes,
// but without allocating the screen buf itself.
// return -1 if ioctl failed, otherwise returns 0.
int screen_init(screen *s) {
  struct winsize w;
  if (ioctl(0, TIOCGWINSZ, &w) < 0) {
    return -1;
  }

  s->cols = w.ws_col;
  s->rows = w.ws_row;
  // -1 because of status bar
  s->size = (w.ws_row - 1) * w.ws_col;

  return 0;
}

// returns the buffer offset for given {X,Y}
uint16_t screen_pos(screen *s, uint16_t x, uint16_t y) {
  return y * s->cols + x;
}

void screen_draw_point(screen *s, uint16_t x, uint16_t y, char c) {
  uint16_t pos = screen_pos(s, x, y);
  s->buf[pos] = c;
}

// screen_draw_frame adds a frame to a given buffer
void screen_draw_frame(screen *s) {
  for (uint16_t col = 0; col < s->cols - 1; col++) {
    screen_draw_point(s, col, 0, '=');
    screen_draw_point(s, col, s->rows - 2, '=');
  }
  for (uint16_t row = 0; row < s->rows; row++) {
    screen_draw_point(s, 0, row, '+');
    screen_draw_point(s, s->cols - 1, row, '+');
  }
}

// empties the gieven buffer and draw a frame around it
void screen_empty(screen *s) {
  uint16_t pos = 0;
  for (uint16_t y = 0; y < s->rows - 1; y++) {
    for (uint16_t x = 0; x < s->cols; x++) {
      pos = screen_pos(s, x, y);
      s->buf[pos] = ' '; // fill buffer with space, start with blank screen
    }
  }

  // draw a frame around the empty screen
  screen_draw_frame(s);
}

// screen_flush remove everything but frame from a screen.
void screen_flush(screen *s) {
  bzero(s->buf, sizeof(char) * s->size);
  screen_empty(s);
}

// renders the screen buffer
void screen_render(screen *s) {
  clear();
  uint16_t pos = 0;
  for (uint16_t y = 0; y < s->rows - 1; y++) {
    for (uint16_t x = 0; x < s->cols; x++) {
      pos = screen_pos(s, x, y);
      printf("%c", s->buf[pos]);
    }
    printf("\n");
  }
}

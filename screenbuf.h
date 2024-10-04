#include <stdint.h>

typedef struct {
  uint16_t cols;
  uint16_t rows;
  uint16_t size;
  char *buf;
} screen;

int screen_init(screen *s);
void screen_flush(screen *s);
void screen_render(screen* s);

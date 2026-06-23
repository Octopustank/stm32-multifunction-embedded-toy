#ifndef __SNAKE_H
#define __SNAKE_H

#include <stdint.h>

#define SNAKE_MAX_LEN  64

typedef enum { DIR_NONE, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } snake_dir_t;

void    snake_init(void);
void    snake_tick(snake_dir_t dir);
void    snake_get_display(uint8_t rows[8]);
int     snake_is_dead(void);
int     snake_get_score(void);
int     snake_get_high(void);
int     snake_get_speed_ms(void);

#endif

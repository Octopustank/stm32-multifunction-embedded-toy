#include "snake.h"
#include <stdlib.h>

typedef struct { int x, y; } point_t;

static point_t body[SNAKE_MAX_LEN];
static int     len, head_idx;
static point_t food;
static int     dead, score, high_score;
static snake_dir_t cur_dir;

static int rng_state;

static int my_rand(void)
{
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state >> 16) & 0x7fff;
}

static int on_body(int x, int y)
{
    for (int i = 0; i < len; i++) {
        int idx = head_idx - i;
        if (idx < 0) idx += SNAKE_MAX_LEN;
        if (body[idx].x == x && body[idx].y == y) return 1;
    }
    return 0;
}

static void place_food(void)
{
    int free = 64 - len;
    if (free <= 0) return;
    int n = my_rand() % free;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (!on_body(x, y)) {
                if (n == 0) { food.x = x; food.y = y; return; }
                n--;
            }
        }
    }
}

void snake_init(void)
{
    len   = 2;
    score = 0;
    dead  = 0;
    cur_dir = DIR_RIGHT;
    rng_state = 42;

    head_idx = 1;
    body[0].x = 3; body[0].y = 4;
    body[1].x = 4; body[1].y = 4;

    place_food();
}

void snake_tick(snake_dir_t dir)
{
    if (dead) return;

    /* prevent 180° reversal */
    if ((dir == DIR_UP    && cur_dir != DIR_DOWN)  ||
        (dir == DIR_DOWN  && cur_dir != DIR_UP)    ||
        (dir == DIR_LEFT  && cur_dir != DIR_RIGHT) ||
        (dir == DIR_RIGHT && cur_dir != DIR_LEFT))
        cur_dir = dir;

    point_t head = body[head_idx];
    switch (cur_dir) {
        case DIR_UP:    head.y--; break;
        case DIR_DOWN:  head.y++; break;
        case DIR_LEFT:  head.x--; break;
        case DIR_RIGHT: head.x++; break;
        default: break;
    }

    /* hit wall → dead */
    if (head.x < 0 || head.x > 7 || head.y < 0 || head.y > 7) {
        dead = 1;
        if (score > high_score) high_score = score;
        return;
    }

    /* self-collision */
    if (on_body(head.x, head.y)) {
        dead = 1;
        if (score > high_score) high_score = score;
        return;
    }

    head_idx = (head_idx + 1) % SNAKE_MAX_LEN;
    body[head_idx] = head;

    if (head.x == food.x && head.y == food.y) {
        score++;
        if (score > high_score) high_score = score;
        len++;
        place_food();
    }
}

void snake_get_display(uint8_t rows[8])
{
    for (int i = 0; i < 8; i++) rows[i] = 0;

    /* draw snake */
    for (int i = 0; i < len; i++) {
        int idx = head_idx - i;
        if (idx < 0) idx += SNAKE_MAX_LEN;
        int x = body[idx].x, y = body[idx].y;
        if (x >= 0 && x < 8 && y >= 0 && y < 8)
            rows[y] |= (1 << (7 - x));
    }

    if (!dead) {
        /* draw food (blinking) */
        if (food.x >= 0 && food.x < 8 && food.y >= 0 && food.y < 8)
            rows[food.y] |= (1 << (7 - food.x));
    }
}

int  snake_is_dead(void)   { return dead; }
int  snake_get_score(void) { return score; }
int  snake_get_high(void)  { return high_score; }

int snake_get_speed_ms(void)
{
    int s = 500 - score * 30;
    return s < 150 ? 150 : s;
}

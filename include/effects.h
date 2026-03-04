#ifndef EFFECTS_H
#define EFFECTS_H

#include <SDL2/SDL.h>

#define MAX_SIZE     800
#define BORDER        40
#define UNDO_HISTORY   5

typedef struct {
    unsigned char *pixels;
    unsigned char *original_pixels;
    unsigned char *undo_stack[UNDO_HISTORY];
    int            undo_head;
    int            undo_count;
    int            w, h;
    int            orig_w, orig_h;
    SDL_Texture   *tex;
    SDL_Renderer  *ren;
    SDL_Window    *win;
    int            needs_layout_update;
    /* Crop/Pan state */
    unsigned char *crop_src;
    int            crop_src_w, crop_src_h;
    int            crop_ox, crop_oy;
    int            crop_active;
    /* Preview mode: when 1, save_undo_state is a no-op */
    int            preview_mode;
} ImageCtx;

extern ImageCtx ctx;

/* ── Effect popup parameter (one adjustable slider) ─────────────────── */
#define EFFECT_MAX_PARAMS 8

typedef struct {
    const char *label;
    int         value;    /* current / default value */
    int         min_val;
    int         max_val;
    int         step;
} EffectParam;

/* ── Effect table entry ──────────────────────────────────────────────── */
typedef struct {
    int          id;
    void       (*fn)(int);          /* simple one-amount call (also used by on_number_confirmed) */
    int          default_amount;
    const char  *name;
    int          param_count;       /* 0 = no popup; >0 = open popup with these params */
    EffectParam  params[EFFECT_MAX_PARAMS];
} Effect;

/* Lookup an effect by ID (returns NULL if not found) */
const Effect  *get_effect(int id);

/* Apply effect with an explicit params array (used by popup confirm + preview) */
const char    *apply_effect_params(int id, int *params, int n_params);

void        reset_image(void);
void        undo_last(void);
void        brighten(int amount);
void        darken(int amount);
void        iceing(int amount);
void        my_new_function(int amount);
void        negative(int amount);
void        kachel_function(int amount);
void        gold(int amount);
void        rainbow(int amount);
const char *on_number_confirmed(int n);
void        crop_pan(int dx, int dy);
const char *crop_aspect(int rw, int rh);
void        composite(int dst_x, int dst_y, int draw_w, int draw_h,
                      int win_w, int win_h, int mirror_mode);
void        save_undo_state(void);
void        canvas_resize(int add_left, int add_right, int add_top, int add_bottom, int fill_mode);
void        pixel_sort(int amount);
void        transform_fill(unsigned char *src_px, int src_w, int src_h, int fill_mode);
void        make_preview(int effect_id,
                         SDL_Texture **out_before, SDL_Texture **out_after,
                         int *out_pw, int *out_ph);

#endif

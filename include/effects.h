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

void        reset_image(void);
void        undo_last(void);
void        brighten(int amount);
void        darken(int amount);
void        iceing(int amount);
void        my_new_function(int amount);
void        negative(int amount);
void        kachel_function(int amount);
void        gold(int amount);
const char *on_number_confirmed(int n);
void        crop_pan(int dx, int dy);
const char *crop_aspect(int rw, int rh);
void        composite(int dst_x, int dst_y, int draw_w, int draw_h,
                      int win_w, int win_h, int mirror_mode);
void        make_preview(int effect_id,
                         SDL_Texture **out_before, SDL_Texture **out_after,
                         int *out_pw, int *out_ph);

#endif

#include "effects.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── R: Bild auf Original zurücksetzen ───────────────────────────────── */
void reset_image(void) {
    if (ctx.crop_src) { free(ctx.crop_src); ctx.crop_src = NULL; }
    ctx.crop_active = 0;
    ctx.crop_ox = ctx.crop_oy = 0;
    if (ctx.w != ctx.orig_w || ctx.h != ctx.orig_h) {
        free(ctx.pixels);
        ctx.pixels = malloc(ctx.orig_w * ctx.orig_h * 4);
        ctx.w = ctx.orig_w;
        ctx.h = ctx.orig_h;
        SDL_DestroyTexture(ctx.tex);
        ctx.tex = SDL_CreateTexture(ctx.ren, SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_STREAMING, ctx.w, ctx.h);
        ctx.needs_layout_update = 1;
    }
    memcpy(ctx.pixels, ctx.original_pixels, ctx.w * ctx.h * 4);
    ctx.undo_head  = 0;
    ctx.undo_count = 0;
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Reset\n"); fflush(stdout);
}

/* ── U: Letzten Schritt rückgängig machen ────────────────────────────── */
void undo_last(void) {
    if (ctx.undo_count == 0) {
        printf("No undo's left\n"); fflush(stdout);
        return;
    }
    int total_bytes = ctx.w * ctx.h * 4;
    ctx.undo_head = (ctx.undo_head - 1 + UNDO_HISTORY) % UNDO_HISTORY;
    ctx.undo_count--;
    memcpy(ctx.pixels, ctx.undo_stack[ctx.undo_head], total_bytes);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Undo (%d/%d left)\n", ctx.undo_count, UNDO_HISTORY); fflush(stdout);
}

/* ── Crop: Bild auf Seitenverhältnis rw:rh zuschneiden (zentriert) ──── */
const char *crop_aspect(int rw, int rh) {
    if (rw <= 0 || rh <= 0) return NULL;

    if (ctx.crop_src) free(ctx.crop_src);
    ctx.crop_src = malloc(ctx.w * ctx.h * 4);
    if (!ctx.crop_src) return NULL;
    memcpy(ctx.crop_src, ctx.pixels, ctx.w * ctx.h * 4);
    ctx.crop_src_w = ctx.w;
    ctx.crop_src_h = ctx.h;

    int new_w, new_h;
    if ((long long)ctx.crop_src_w * rh > (long long)ctx.crop_src_h * rw) {
        new_h = ctx.crop_src_h;
        new_w = ctx.crop_src_h * rw / rh;
    } else {
        new_w = ctx.crop_src_w;
        new_h = ctx.crop_src_w * rh / rw;
    }

    if (new_w <= 0 || new_h <= 0) { free(ctx.crop_src); ctx.crop_src = NULL; return NULL; }

    ctx.crop_ox = (ctx.crop_src_w - new_w) / 2;
    ctx.crop_oy = (ctx.crop_src_h - new_h) / 2;
    ctx.crop_active = 1;

    unsigned char *new_pixels = malloc(new_w * new_h * 4);
    if (!new_pixels) { free(ctx.crop_src); ctx.crop_src = NULL; return NULL; }

    for (int y = 0; y < new_h; y++)
        memcpy(new_pixels + y * new_w * 4,
               ctx.crop_src + ((ctx.crop_oy + y) * ctx.crop_src_w + ctx.crop_ox) * 4,
               new_w * 4);

    free(ctx.pixels);
    ctx.pixels = new_pixels;
    ctx.w = new_w;
    ctx.h = new_h;

    ctx.undo_head  = 0;
    ctx.undo_count = 0;

    SDL_DestroyTexture(ctx.tex);
    ctx.tex = SDL_CreateTexture(ctx.ren, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING, ctx.w, ctx.h);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);

    ctx.needs_layout_update = 1;
    printf("Crop %d:%d -> %dx%d\n", rw, rh, new_w, new_h); fflush(stdout);
    return "Crop";
}

/* ── Pfeil: Crop-Fenster innerhalb der Quelle verschieben ────────────── */
void crop_pan(int dx, int dy) {
    if (!ctx.crop_active) return;

    int max_ox = ctx.crop_src_w - ctx.w;
    int max_oy = ctx.crop_src_h - ctx.h;

    ctx.crop_ox += dx;
    ctx.crop_oy += dy;
    if (ctx.crop_ox < 0)      ctx.crop_ox = 0;
    if (ctx.crop_oy < 0)      ctx.crop_oy = 0;
    if (ctx.crop_ox > max_ox) ctx.crop_ox = max_ox;
    if (ctx.crop_oy > max_oy) ctx.crop_oy = max_oy;

    for (int y = 0; y < ctx.h; y++)
        memcpy(ctx.pixels + y * ctx.w * 4,
               ctx.crop_src + ((ctx.crop_oy + y) * ctx.crop_src_w + ctx.crop_ox) * 4,
               ctx.w * 4);

    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

/* ── C: Aktuellen Viewport einfrieren und als neues Bild übernehmen ──── */
void composite(int dst_x, int dst_y, int draw_w, int draw_h,
               int win_w, int win_h, int mirror_mode) {
    int vp_w = win_w - 2 * BORDER;
    int vp_h = win_h - 2 * BORDER;
    unsigned char *out = calloc(vp_w * vp_h * 4, 1);
    if (!out) return;

    float scale_x = (float)ctx.w / draw_w;
    float scale_y = (float)ctx.h / draw_h;

    for (int py = 0; py < vp_h; py++) {
        for (int px = 0; px < vp_w; px++) {
            int rel_x = (BORDER + px) - dst_x;
            int rel_y = (BORDER + py) - dst_y;
            int wx, wy;

            if (mirror_mode) {
                int tx = rel_x / draw_w; if (rel_x < 0 && rel_x % draw_w != 0) tx--;
                int ty = rel_y / draw_h; if (rel_y < 0 && rel_y % draw_h != 0) ty--;
                wx = rel_x - tx * draw_w;
                wy = rel_y - ty * draw_h;
                if (abs(tx) % 2 == 1) wx = draw_w - 1 - wx;
                if (abs(ty) % 2 == 1) wy = draw_h - 1 - wy;
            } else {
                wx = rel_x; wy = rel_y;
                if (wx < 0 || wx >= draw_w || wy < 0 || wy >= draw_h) continue;
            }

            int sx = (int)(wx * scale_x); if (sx >= ctx.w) sx = ctx.w - 1;
            int sy = (int)(wy * scale_y); if (sy >= ctx.h) sy = ctx.h - 1;
            int oi = (py * vp_w + px) * 4;
            int si = (sy  * ctx.w + sx) * 4;
            out[oi+0] = ctx.pixels[si+0];
            out[oi+1] = ctx.pixels[si+1];
            out[oi+2] = ctx.pixels[si+2];
            out[oi+3] = ctx.pixels[si+3];
        }
    }

    free(ctx.pixels);
    ctx.pixels = out;
    free(ctx.original_pixels);
    ctx.original_pixels = malloc(vp_w * vp_h * 4);
    if (ctx.original_pixels) memcpy(ctx.original_pixels, out, vp_w * vp_h * 4);
    ctx.w = ctx.orig_w = vp_w;
    ctx.h = ctx.orig_h = vp_h;

    int nb = vp_w * vp_h * 4;
    for (int i = 0; i < UNDO_HISTORY; i++) {
        free(ctx.undo_stack[i]);
        ctx.undo_stack[i] = malloc(nb);
    }
    ctx.undo_head = ctx.undo_count = 0;

    if (ctx.crop_src) { free(ctx.crop_src); ctx.crop_src = NULL; }
    ctx.crop_active = 0;

    SDL_DestroyTexture(ctx.tex);
    ctx.tex = SDL_CreateTexture(ctx.ren, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING, ctx.w, ctx.h);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    ctx.needs_layout_update = 1;
    printf("Composite %dx%d\n", vp_w, vp_h); fflush(stdout);
}

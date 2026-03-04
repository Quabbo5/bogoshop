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

    float scale_x = (float)ctx.w / draw_w;
    float scale_y = (float)ctx.h / draw_h;

    /* Output at source-pixel density so zoomed/tiled composites stay sharp */
    int out_w = (int)(vp_w * scale_x);
    int out_h = (int)(vp_h * scale_y);
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;

    /* Top-left corner of the viewport in source-pixel coordinates */
    int istart_x = (int)((float)(BORDER - dst_x) * scale_x);
    int istart_y = (int)((float)(BORDER - dst_y) * scale_y);

    unsigned char *out = calloc(out_w * out_h * 4, 1);
    if (!out) return;

    for (int oy = 0; oy < out_h; oy++) {
        for (int ox = 0; ox < out_w; ox++) {
            int tsrc_x = istart_x + ox;
            int tsrc_y = istart_y + oy;
            int sx, sy;

            if (mirror_mode) {
                /* Integer floor-division for tile index (handles negative coords) */
                int tx = tsrc_x / ctx.w;
                if (tsrc_x < 0 && tsrc_x % ctx.w != 0) tx--;
                int ty = tsrc_y / ctx.h;
                if (tsrc_y < 0 && tsrc_y % ctx.h != 0) ty--;
                int lx = tsrc_x - tx * ctx.w;
                int ly = tsrc_y - ty * ctx.h;
                if ((tx < 0 ? -tx : tx) % 2 == 1) lx = ctx.w - 1 - lx;
                if ((ty < 0 ? -ty : ty) % 2 == 1) ly = ctx.h - 1 - ly;
                sx = lx;
                sy = ly;
            } else {
                sx = tsrc_x;
                sy = tsrc_y;
                if (sx < 0 || sx >= ctx.w || sy < 0 || sy >= ctx.h) continue;
            }

            if (sx < 0) sx = 0; if (sx >= ctx.w) sx = ctx.w - 1;
            if (sy < 0) sy = 0; if (sy >= ctx.h) sy = ctx.h - 1;

            int oi = (oy * out_w + ox) * 4;
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
    ctx.original_pixels = malloc(out_w * out_h * 4);
    if (ctx.original_pixels) memcpy(ctx.original_pixels, out, out_w * out_h * 4);
    ctx.w = ctx.orig_w = out_w;
    ctx.h = ctx.orig_h = out_h;

    int nb = out_w * out_h * 4;
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
    printf("Composite %dx%d\n", out_w, out_h); fflush(stdout);
}

/* ── Bilinear scale: src (sw×sh) → dst (dw×dh) ─────────────────────── */
static void nn_scale(const unsigned char *src, int sw, int sh,
                     unsigned char *dst, int dw, int dh) {
    float sx_step = (sw > 1) ? (float)(sw - 1) / (dw > 1 ? dw - 1 : 1) : 0.0f;
    float sy_step = (sh > 1) ? (float)(sh - 1) / (dh > 1 ? dh - 1 : 1) : 0.0f;
    for (int y = 0; y < dh; y++) {
        float fy = y * sy_step;
        int   y0 = (int)fy;
        int   y1 = y0 + 1; if (y1 >= sh) y1 = sh - 1;
        float wy = fy - y0;
        for (int x = 0; x < dw; x++) {
            float fx = x * sx_step;
            int   x0 = (int)fx;
            int   x1 = x0 + 1; if (x1 >= sw) x1 = sw - 1;
            float wx = fx - x0;
            unsigned char *out = dst + (y * dw + x) * 4;
            const unsigned char *p00 = src + (y0 * sw + x0) * 4;
            const unsigned char *p10 = src + (y0 * sw + x1) * 4;
            const unsigned char *p01 = src + (y1 * sw + x0) * 4;
            const unsigned char *p11 = src + (y1 * sw + x1) * 4;
            float w00 = (1.0f - wx) * (1.0f - wy);
            float w10 = wx           * (1.0f - wy);
            float w01 = (1.0f - wx) * wy;
            float w11 = wx           * wy;
            for (int c = 0; c < 4; c++)
                out[c] = (unsigned char)(p00[c] * w00 + p10[c] * w10
                                      + p01[c] * w01 + p11[c] * w11 + 0.5f);
        }
    }
}

/* ── Canvas-Größe ändern, Inhalt nach fill_mode platzieren ──────────── */
/* fill_mode: 1=Fill  2=Fit  3=Stretch  4=Tile  5=Center  6=Span        */
void canvas_resize(int add_left, int add_right, int add_top, int add_bottom, int fill_mode) {
    int old_w = ctx.w, old_h = ctx.h;
    int new_w = old_w + add_left + add_right;
    int new_h = old_h + add_top  + add_bottom;
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;

    unsigned char *new_pixels = calloc(new_w * new_h * 4, 1);
    if (!new_pixels) return;

    switch (fill_mode) {

        case 3: /* STRETCH — scale to exact canvas size */
            nn_scale(ctx.pixels, old_w, old_h, new_pixels, new_w, new_h);
            break;

        case 1: /* FILL — scale to cover, proportional, center-crop */
        case 6: /* SPAN — same as Fill but anchored top-left */
        {
            float sx = (float)new_w / old_w;
            float sy = (float)new_h / old_h;
            float s  = sx > sy ? sx : sy;
            int sc_w = (int)(old_w * s); if (sc_w < 1) sc_w = 1;
            int sc_h = (int)(old_h * s); if (sc_h < 1) sc_h = 1;
            unsigned char *scaled = malloc(sc_w * sc_h * 4);
            if (scaled) {
                nn_scale(ctx.pixels, old_w, old_h, scaled, sc_w, sc_h);
                int ox = (fill_mode == 1) ? (sc_w - new_w) / 2 : 0;
                int oy = (fill_mode == 1) ? (sc_h - new_h) / 2 : 0;
                for (int y = 0; y < new_h; y++) {
                    int row = oy + y;
                    if (row < 0 || row >= sc_h) continue;
                    for (int x = 0; x < new_w; x++) {
                        int col = ox + x;
                        if (col < 0 || col >= sc_w) continue;
                        memcpy(new_pixels + (y * new_w + x) * 4,
                               scaled + (row * sc_w + col) * 4, 4);
                    }
                }
                free(scaled);
            }
            break;
        }

        case 2: /* FIT — scale to contain, proportional, centered letterbox */
        {
            float sx = (float)new_w / old_w;
            float sy = (float)new_h / old_h;
            float s  = sx < sy ? sx : sy;
            int sc_w = (int)(old_w * s); if (sc_w < 1) sc_w = 1;
            int sc_h = (int)(old_h * s); if (sc_h < 1) sc_h = 1;
            unsigned char *scaled = malloc(sc_w * sc_h * 4);
            if (scaled) {
                nn_scale(ctx.pixels, old_w, old_h, scaled, sc_w, sc_h);
                int dx = (new_w - sc_w) / 2;
                int dy = (new_h - sc_h) / 2;
                for (int y = 0; y < sc_h; y++) {
                    int row = dy + y;
                    if (row < 0 || row >= new_h) continue;
                    int x0 = dx < 0 ? -dx : 0;
                    int x1 = sc_w;
                    if (dx + x1 > new_w) x1 = new_w - dx;
                    if (x1 <= x0) continue;
                    memcpy(new_pixels + (row * new_w + dx + x0) * 4,
                           scaled + (y * sc_w + x0) * 4,
                           (x1 - x0) * 4);
                }
                free(scaled);
            }
            break;
        }

        case 4: /* TILE — repeat image to fill */
            for (int y = 0; y < new_h; y++)
                for (int x = 0; x < new_w; x++)
                    memcpy(new_pixels + (y * new_w + x) * 4,
                           ctx.pixels + ((y % old_h) * old_w + (x % old_w)) * 4, 4);
            break;

        case 5: /* CENTER — keep image at original size, anchor to add_left/add_top */
        default:
        {
            int dx = add_left;
            int dy = add_top;
            int src_x0 = dx < 0 ? -dx : 0;
            int src_y0 = dy < 0 ? -dy : 0;
            int dst_x0 = dx > 0 ?  dx : 0;
            int dst_y0 = dy > 0 ?  dy : 0;
            int cw = old_w - src_x0; if (cw > new_w - dst_x0) cw = new_w - dst_x0; if (cw < 0) cw = 0;
            int ch = old_h - src_y0; if (ch > new_h - dst_y0) ch = new_h - dst_y0; if (ch < 0) ch = 0;
            for (int y = 0; y < ch; y++)
                memcpy(new_pixels + ((dst_y0 + y) * new_w + dst_x0) * 4,
                       ctx.pixels  + ((src_y0 + y) * old_w  + src_x0) * 4,
                       cw * 4);
            break;
        }
    }

    free(ctx.pixels);
    ctx.pixels = new_pixels;
    ctx.w = ctx.orig_w = new_w;
    ctx.h = ctx.orig_h = new_h;

    free(ctx.original_pixels);
    int nb = new_w * new_h * 4;
    ctx.original_pixels = malloc(nb);
    if (ctx.original_pixels) memcpy(ctx.original_pixels, new_pixels, nb);

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
    printf("Canvas resize %dx%d (mode %d)\n", new_w, new_h, fill_mode); fflush(stdout);
}

/* ── Transform: fit external src image into current canvas (no size change) ── */
/* fill_mode: 1=Fill 2=Fit 3=Stretch 4=Tile 5=Center 6=Span                    */
void transform_fill(unsigned char *src_px, int src_w, int src_h, int fill_mode) {
    int new_w = ctx.w, new_h = ctx.h;
    unsigned char *new_pixels = calloc(new_w * new_h * 4, 1);
    if (!new_pixels) return;

    switch (fill_mode) {
        case 3: /* STRETCH */
            nn_scale(src_px, src_w, src_h, new_pixels, new_w, new_h);
            break;
        case 1: /* FILL — scale to cover, center-crop */
        case 6: /* SPAN — scale to cover, top-left anchor */
        {
            float sx = (float)new_w / src_w;
            float sy = (float)new_h / src_h;
            float s  = sx > sy ? sx : sy;
            int sc_w = (int)(src_w * s + 0.5f); if (sc_w < 1) sc_w = 1;
            int sc_h = (int)(src_h * s + 0.5f); if (sc_h < 1) sc_h = 1;
            unsigned char *scaled = malloc(sc_w * sc_h * 4);
            if (scaled) {
                nn_scale(src_px, src_w, src_h, scaled, sc_w, sc_h);
                int ox = (fill_mode == 1) ? (sc_w - new_w) / 2 : 0;
                int oy = (fill_mode == 1) ? (sc_h - new_h) / 2 : 0;
                for (int y = 0; y < new_h; y++) {
                    int row = oy + y; if (row < 0 || row >= sc_h) continue;
                    for (int x = 0; x < new_w; x++) {
                        int col = ox + x; if (col < 0 || col >= sc_w) continue;
                        memcpy(new_pixels + (y * new_w + x) * 4,
                               scaled + (row * sc_w + col) * 4, 4);
                    }
                }
                free(scaled);
            }
            break;
        }
        case 2: /* FIT — scale to contain, letterbox */
        {
            float sx = (float)new_w / src_w;
            float sy = (float)new_h / src_h;
            float s  = sx < sy ? sx : sy;
            int sc_w = (int)(src_w * s + 0.5f); if (sc_w < 1) sc_w = 1;
            int sc_h = (int)(src_h * s + 0.5f); if (sc_h < 1) sc_h = 1;
            unsigned char *scaled = malloc(sc_w * sc_h * 4);
            if (scaled) {
                nn_scale(src_px, src_w, src_h, scaled, sc_w, sc_h);
                int dx = (new_w - sc_w) / 2;
                int dy = (new_h - sc_h) / 2;
                for (int y = 0; y < sc_h; y++) {
                    int row = dy + y; if (row < 0 || row >= new_h) continue;
                    int x0 = dx < 0 ? -dx : 0, x1 = sc_w;
                    if (dx + x1 > new_w) x1 = new_w - dx;
                    if (x1 <= x0) continue;
                    memcpy(new_pixels + (row * new_w + dx + x0) * 4,
                           scaled + (y * sc_w + x0) * 4, (x1 - x0) * 4);
                }
                free(scaled);
            }
            break;
        }
        case 4: /* TILE */
            for (int y = 0; y < new_h; y++)
                for (int x = 0; x < new_w; x++)
                    memcpy(new_pixels + (y * new_w + x) * 4,
                           src_px + ((y % src_h) * src_w + (x % src_w)) * 4, 4);
            break;
        case 5: /* CENTER — original at center, rest transparent */
        default:
        {
            int dx = (new_w - src_w) / 2;
            int dy = (new_h - src_h) / 2;
            for (int y = 0; y < src_h; y++) {
                int row = dy + y; if (row < 0 || row >= new_h) continue;
                for (int x = 0; x < src_w; x++) {
                    int col = dx + x; if (col < 0 || col >= new_w) continue;
                    memcpy(new_pixels + (row * new_w + col) * 4,
                           src_px + (y * src_w + x) * 4, 4);
                }
            }
            break;
        }
    }
    save_undo_state();
    free(ctx.pixels);
    ctx.pixels = new_pixels;
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Transform fill %dx%d -> %dx%d (mode %d)\n", src_w, src_h, new_w, new_h, fill_mode);
    fflush(stdout);
}

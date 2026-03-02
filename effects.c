#include "effects.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ImageCtx ctx;

static void save_undo_state(void) {
    int total_bytes = ctx.w * ctx.h * 4;
    memcpy(ctx.undo_stack[ctx.undo_head], ctx.pixels, total_bytes);
    ctx.undo_head = (ctx.undo_head + 1) % UNDO_HISTORY;
    if (ctx.undo_count < UNDO_HISTORY) ctx.undo_count++;
}

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
        printf("No more undos for you\n"); fflush(stdout);
        return;
    }
    int total_bytes = ctx.w * ctx.h * 4;
    ctx.undo_head = (ctx.undo_head - 1 + UNDO_HISTORY) % UNDO_HISTORY;
    ctx.undo_count--;
    memcpy(ctx.pixels, ctx.undo_stack[ctx.undo_head], total_bytes);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Undo (%d/%d left)\n", ctx.undo_count, UNDO_HISTORY); fflush(stdout);
}

/* ── Bild um 'amount' aufhellen (0–255) und Textur aktualisieren ─────── */
void brighten(int amount) {
    save_undo_state();
    int total_bytes = ctx.w * ctx.h * 4;
    for (int byte_index = 0; byte_index < total_bytes; byte_index++) {
        if (byte_index % 4 == 3) continue;
        int neuer_wert = ctx.pixels[byte_index] + amount;
        ctx.pixels[byte_index] = neuer_wert > 255 ? 255 : (unsigned char)neuer_wert;
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

/* ── Bild um 'amount' abdunkeln (0–255) und Textur aktualisieren ─────── */
void darken(int amount) {
    save_undo_state();
    int total_bytes = ctx.w * ctx.h * 4;
    for (int byte_index = 0; byte_index < total_bytes; byte_index++) {
        if (byte_index % 4 == 3) continue;
        int neuer_wert = ctx.pixels[byte_index] - amount;
        ctx.pixels[byte_index] = neuer_wert < 0 ? 0 : (unsigned char)neuer_wert;
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

/*Ich weiß diese funktion macht keine helligkeit runter aber der effekt ist cool deswegen bitte so stehen lassen*/
void iceing(int amount) {
    save_undo_state();
    int total_bytes = ctx.w * ctx.h * 4;
    for (int byte_index = 0; byte_index < total_bytes; byte_index++) {
        if (byte_index % 4 == 3) continue;
        int neuer_wert = ctx.pixels[byte_index] - amount;
        ctx.pixels[byte_index] = neuer_wert > 255 ? 255 : (unsigned char)neuer_wert;
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

void my_new_function(int amount) {
    save_undo_state();
    int total_bytes = ctx.w * ctx.h * 4;
    for (int byte_index = 0; byte_index < total_bytes; byte_index++) {
        if (byte_index % 4 == 3) continue;
        int neuer_wert = ctx.pixels[byte_index] - amount;
        ctx.pixels[byte_index] = neuer_wert > 255 ? 255 : (unsigned char)neuer_wert;
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

void negative(int amount){
    save_undo_state();
    int total_bytes = ctx.w * ctx.h * 4;
    for (int byte_index = 0; byte_index < total_bytes; byte_index++) {
        if (byte_index % 4 == 3) continue;
        ctx.pixels[byte_index] = 255 - ctx.pixels[byte_index];
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

void kachel_function(int amount) {
    save_undo_state();
    int w = ctx.w, h = ctx.h;
    int tile_w = w / 2;
    int tile_h = h / 2;

    /* Temporäre Kopie zum Lesen – sonst überschreiben wir Quellpixel die wir noch brauchen */
    unsigned char *tmp = malloc(w * h * 4);
    memcpy(tmp, ctx.pixels, w * h * 4);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            /* Position innerhalb der Kachel → zurück auf Vollbild-Koordinaten skalieren */
            int sx = (x % tile_w) * 2;
            int sy = (y % tile_h) * 2;
            if (sx >= w) sx = w - 1;
            if (sy >= h) sy = h - 1;

            int dst = (y * w + x) * 4;
            int src = (sy * w + sx) * 4;
            ctx.pixels[dst + 0] = tmp[src + 0];
            ctx.pixels[dst + 1] = tmp[src + 1];
            ctx.pixels[dst + 2] = tmp[src + 2];
            ctx.pixels[dst + 3] = tmp[src + 3];
        }
    }

    free(tmp);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

/* ── Effect-Tabelle: ID → Funktion → Amount → Name ───────────────────── */
static const struct {
    int         id;
    void      (*fn)(int);
    int         amount;
    const char *name;
} effects[] = {
    { 1, brighten,        30, "Brighten"  },
    { 2, darken,          30, "Darken"    },
    { 3, iceing,          30, "Iceing"    },
    { 4, my_new_function, 30, "MyFunc"    },
    { 5, negative,         0, "Negative"  },
    { 6, kachel_function,  0, "Kachel"    },
};

/* ── Crop: Bild auf Seitenverhältnis rw:rh zuschneiden (zentriert) ──── */
const char *crop_aspect(int rw, int rh) {
    if (rw <= 0 || rh <= 0) return NULL;

    /* Quelle für Pan merken (alte crop_src freigeben falls vorhanden) */
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
    printf("Crop %d:%d → %dx%d\n", rw, rh, new_w, new_h); fflush(stdout);
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

/* catching signals – gibt den Effektnamen zurück, NULL wenn nicht gefunden */
const char *on_number_confirmed(int n) {
    printf("Input: %d\n", n);
    fflush(stdout);
    for (int i = 0; i < (int)(sizeof(effects) / sizeof(effects[0])); i++) {
        if (effects[i].id == n) {
            effects[i].fn(effects[i].amount);
            return effects[i].name;
        }
    }
    printf("No function found dumbass hahahhaha\n");
    return NULL;
}

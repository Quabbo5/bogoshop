#include "effects.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ImageCtx ctx;

static void save_undo_state(void) {
    if (ctx.preview_mode) return;
    int total_bytes = ctx.w * ctx.h * 4;
    memcpy(ctx.undo_stack[ctx.undo_head], ctx.pixels, total_bytes);
    ctx.undo_head = (ctx.undo_head + 1) % UNDO_HISTORY;
    if (ctx.undo_count < UNDO_HISTORY) ctx.undo_count++;
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

/* ── Gold: Dreieck-Rasterizer (Scanline) ─────────────────────────────── */
static void fill_tri(unsigned char *px, int w, int h,
                     int x0, int y0, int x1, int y1, int x2, int y2,
                     int dr, int dg, int db) {
    /* Vertices nach Y sortieren */
    #define SWAP2(a,b) { int _t=(a);(a)=(b);(b)=_t; }
    if (y0 > y1) { SWAP2(y0,y1); SWAP2(x0,x1); }
    if (y0 > y2) { SWAP2(y0,y2); SWAP2(x0,x2); }
    if (y1 > y2) { SWAP2(y1,y2); SWAP2(x1,x2); }
    #undef SWAP2

    for (int y = y0; y <= y2; y++) {
        if (y < 0 || y >= h) continue;
        float xa, xb;
        if (y <= y1) {
            float t01 = (y1==y0) ? 1.f : (float)(y-y0)/(y1-y0);
            float t02 = (y2==y0) ? 1.f : (float)(y-y0)/(y2-y0);
            xa = x0 + t01*(x1-x0);
            xb = x0 + t02*(x2-x0);
        } else {
            float t12 = (y2==y1) ? 1.f : (float)(y-y1)/(y2-y1);
            float t02 = (y2==y0) ? 1.f : (float)(y-y0)/(y2-y0);
            xa = x1 + t12*(x2-x1);
            xb = x0 + t02*(x2-x0);
        }
        if (xa > xb) { float t=xa; xa=xb; xb=t; }
        for (int x = (int)xa; x <= (int)xb; x++) {
            if (x < 0 || x >= w) continue;
            int i = (y*w + x)*4;
            int r = px[i+0]+dr, g = px[i+1]+dg, b = px[i+2]+db;
            px[i+0] = r<0?0:r>255?255:(unsigned char)r;
            px[i+1] = g<0?0:g>255?255:(unsigned char)g;
            px[i+2] = b<0?0:b>255?255:(unsigned char)b;
        }
    }
}

/* ── Gold: Jittered-Grid Tessellation ────────────────────────────────── */
void gold(int amount) {
    const char *EFFECT_ID   = "007";
    const char *EFFECT_NAME = "Gold Dust";
    save_undo_state();
    int w = ctx.w, h = ctx.h;

    /* 1. Gold-Tint über alle Pixel blenden */
    float t = 0.42f;
    for (int i = 0; i < w * h; i++) {
        unsigned char *p = ctx.pixels + i*4;
        p[0] = (unsigned char)(p[0]*(1.f-t) + 255*t);
        p[1] = (unsigned char)(p[1]*(1.f-t) + 195*t);
        p[2] = (unsigned char)(p[2]*(1.f-t) +  20*t);
    }

    /* 2. Jittered Grid: Raster mit leicht verschobenen Punkten */
    int cols = 8 + amount / 6;       /* Anzahl Spalten */
    int rows = cols * h / w;         /* Zeilen proportional */
    if (rows < 3) rows = 3;

    int gw = cols + 1, gh = rows + 1;
    float *gx = malloc(gw * gh * sizeof(float));
    float *gy = malloc(gw * gh * sizeof(float));
    if (!gx || !gy) { free(gx); free(gy); return; }

    float cw = (float)w / cols;
    float ch = (float)h / rows;

    for (int r = 0; r < gh; r++) {
        for (int c = 0; c < gw; c++) {
            float jx = (c > 0 && c < gw-1)
                       ? ((float)(rand()%1000)/1000.f * 2.f - 1.f) * cw * 0.38f : 0.f;
            float jy = (r > 0 && r < gh-1)
                       ? ((float)(rand()%1000)/1000.f * 2.f - 1.f) * ch * 0.38f : 0.f;
            gx[r*gw + c] = c * cw + jx;
            gy[r*gw + c] = r * ch + jy;
        }
    }

    /* 3. Jede Zelle → 2 verbundene Dreiecke */
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int tl = r*gw + c,   tr = r*gw + c+1;
            int bl = (r+1)*gw+c, br = (r+1)*gw+c+1;

            /* Jedes Dreieck bekommt eigenen Shimmer */
            for (int tri = 0; tri < 2; tri++) {
                int roll  = rand() % 10;
                int light = roll >= 3;       /* 70% hell */
                int glint = roll == 9;       /* 10% sehr heller Glint */
                int dark  = roll < 3;        /* 30% tief dunkel */
                int s = glint ? (60 + rand()%40)
                      : light ? (25 + rand()%40)
                      :         (25 + rand()%35);
                int dr = glint ?  s                   : light ?  s              : -(s*3/4);
                int dg = glint ? (int)(s*.90f)        : light ? (int)(s*.72f)   : -s;
                int db = glint ? (int)(s*.55f)        : light ? -(s/3)          : -s;

                if (tri == 0)
                    fill_tri(ctx.pixels, w, h,
                             (int)gx[tl],(int)gy[tl],
                             (int)gx[tr],(int)gy[tr],
                             (int)gx[bl],(int)gy[bl], dr,dg,db);
                else
                    fill_tri(ctx.pixels, w, h,
                             (int)gx[tr],(int)gy[tr],
                             (int)gx[br],(int)gy[br],
                             (int)gx[bl],(int)gy[bl], dr,dg,db);
            }
        }
    }

    free(gx); free(gy);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Applied: %s -- ID: %s (%dx%d grid, %d tris)\n", EFFECT_NAME, EFFECT_ID, cols, rows, cols*rows*2);
    fflush(stdout);
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
    { 7, gold,            40, "Gold"      },
};

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
    printf("No function found\n");
    return NULL;
}

/* ── Vorschau: skaliertes Before/After für Help-Modus ────────────────── */
void make_preview(int effect_id,
                  SDL_Texture **out_before, SDL_Texture **out_after,
                  int *out_pw, int *out_ph) {
    const int MAX_PV = 200;
    float fsx = (float)MAX_PV / ctx.w;
    float fsy = (float)MAX_PV / ctx.h;
    float sc  = fsx < fsy ? fsx : fsy;
    int pw = (int)(ctx.w * sc);
    int ph = (int)(ctx.h * sc);
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;

    /* Nearest-neighbour downscale → preview buffer */
    unsigned char *pv = malloc(pw * ph * 4);
    if (!pv) return;
    for (int y = 0; y < ph; y++) {
        for (int x = 0; x < pw; x++) {
            int ix = (int)((float)x / pw * ctx.w);
            int iy = (int)((float)y / ph * ctx.h);
            if (ix >= ctx.w) ix = ctx.w - 1;
            if (iy >= ctx.h) iy = ctx.h - 1;
            int di = (y * pw + x) * 4;
            int si = (iy * ctx.w + ix) * 4;
            pv[di+0] = ctx.pixels[si+0];
            pv[di+1] = ctx.pixels[si+1];
            pv[di+2] = ctx.pixels[si+2];
            pv[di+3] = ctx.pixels[si+3];
        }
    }

    /* "Before" texture */
    SDL_Texture *tbefore = SDL_CreateTexture(ctx.ren, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, pw, ph);
    if (!tbefore) { free(pv); return; }
    SDL_UpdateTexture(tbefore, NULL, pv, pw * 4);

    /* Swap ctx to isolated preview state */
    unsigned char *save_pixels = ctx.pixels;
    int save_w = ctx.w, save_h = ctx.h;
    SDL_Texture *save_tex = ctx.tex;

    ctx.pixels = malloc(pw * ph * 4);
    if (!ctx.pixels) {
        ctx.pixels = save_pixels; ctx.w = save_w; ctx.h = save_h; ctx.tex = save_tex;
        free(pv); SDL_DestroyTexture(tbefore); return;
    }
    memcpy(ctx.pixels, pv, pw * ph * 4);
    ctx.w = pw; ctx.h = ph;
    ctx.tex = SDL_CreateTexture(ctx.ren, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING, pw, ph);
    ctx.preview_mode = 1;

    /* Apply effect in isolated state */
    on_number_confirmed(effect_id);

    /* "After" texture */
    SDL_Texture *tafter = SDL_CreateTexture(ctx.ren, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STATIC, pw, ph);
    if (tafter) SDL_UpdateTexture(tafter, NULL, ctx.pixels, pw * 4);

    /* Restore original ctx */
    free(ctx.pixels);
    SDL_DestroyTexture(ctx.tex);
    ctx.pixels = save_pixels;
    ctx.w = save_w; ctx.h = save_h;
    ctx.tex = save_tex;
    ctx.preview_mode = 0;

    free(pv);
    *out_before = tbefore;
    *out_after  = tafter;
    *out_pw     = pw;
    *out_ph     = ph;
}

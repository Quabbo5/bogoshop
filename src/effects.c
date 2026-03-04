#include "effects.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

ImageCtx ctx;

void save_undo_state(void) {
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

/* ── Rainbow core: screen-blend gradient at any angle ───────────────── */
/* intensity: 0-100   direction: 0-359 deg (0=top→bottom, 90=left→right) */
static void rainbow_ex(int intensity, int direction) {
    
    int   w      = ctx.w, h = ctx.h;
    float intens = intensity / 100.0f;
    float angle  = (float)direction * 3.14159265f / 180.0f;

    /* Gradient axis direction vector (screen coords, y-down) */
    float dx = sinf(angle);
    float dy = cosf(angle);

    /* Maximum projection of any corner onto the axis → used to normalise t */
    float t_max = 0.5f * fabsf(dx) + 0.5f * fabsf(dy);
    if (t_max < 1e-6f) t_max = 1e-6f;

    for (int y = 0; y < h; y++) {
        float ny = (h > 1) ? ((float)y / (h - 1) - 0.5f) : 0.0f;
        for (int x = 0; x < w; x++) {
            float nx = (w > 1) ? ((float)x / (w - 1) - 0.5f) : 0.0f;

            /* Project pixel position onto gradient axis → t in [0,1] */
            float t = (nx * dx + ny * dy + t_max) / (2.0f * t_max);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            /* Hue: 0° (red) → 270° (violet) */
            float hue = t * 270.0f;
            float H   = hue / 60.0f;
            int   sec = (int)H;
            float f   = H - sec;
            float q   = 1.0f - f;
            float rr, gg, bb;
            switch (sec) {
                case 0: rr=1; gg=f; bb=0; break;
                case 1: rr=q; gg=1; bb=0; break;
                case 2: rr=0; gg=1; bb=f; break;
                case 3: rr=0; gg=q; bb=1; break;
                case 4: rr=f; gg=0; bb=1; break;
                default:rr=1; gg=0; bb=q; break;
            }
            rr *= intens; gg *= intens; bb *= intens;

            unsigned char *p = ctx.pixels + (y * w + x) * 4;
            float sr = p[0] / 255.0f, sg = p[1] / 255.0f, sb = p[2] / 255.0f;
            /* Screen blend: 1 - (1-src)*(1-overlay) */
            p[0] = (unsigned char)((1.0f-(1.0f-sr)*(1.0f-rr))*255.0f+0.5f);
            p[1] = (unsigned char)((1.0f-(1.0f-sg)*(1.0f-gg))*255.0f+0.5f);
            p[2] = (unsigned char)((1.0f-(1.0f-sb)*(1.0f-bb))*255.0f+0.5f);
        }
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

/* ── Rainbow public wrapper (saves undo, default direction = top→bottom) */
void rainbow(int amount) {
    save_undo_state();
    rainbow_ex(amount, 0);
    printf("Applied: Rainbow Overlay -- ID: 008\n"); fflush(stdout);
}

/* ── Pixel Sort ──────────────────────────────────────────────────────── */
/* Returns sort key in 0..255 range for the given mode:                  */
/*   1 = Luminance,  2 = Hue,  3 = Saturation                           */
static float ps_key(unsigned char r, unsigned char g, unsigned char b, int mode) {
    switch (mode) {
        case 2: {
            float fr = r/255.f, fg = g/255.f, fb = b/255.f;
            float M = fr>fg?(fr>fb?fr:fb):(fg>fb?fg:fb);
            float m = fr<fg?(fr<fb?fr:fb):(fg<fb?fg:fb);
            float d = M - m;
            if (d < 1e-6f) return 0.f;
            float h;
            if      (M==fr) h = 60.f * fmodf((fg-fb)/d, 6.f);
            else if (M==fg) h = 60.f * ((fb-fr)/d + 2.f);
            else            h = 60.f * ((fr-fg)/d + 4.f);
            if (h < 0.f) h += 360.f;
            return h / 360.f * 255.f;
        }
        case 3: {
            float fr = r/255.f, fg = g/255.f, fb = b/255.f;
            float M = fr>fg?(fr>fb?fr:fb):(fg>fb?fg:fb);
            float m = fr<fg?(fr<fb?fr:fb):(fg<fb?fg:fb);
            if (M < 1e-6f) return 0.f;
            return ((M-m)/M) * 255.f;
        }
        default: /* Luminance */
            return 0.2126f*r + 0.7152f*g + 0.0722f*b;
    }
}

static int            ps_sort_mode;
static unsigned char *ps_sort_line;

static int ps_cmp(const void *a, const void *b) {
    float ka = ps_key(ps_sort_line[(*(const int*)a)*4  ],
                      ps_sort_line[(*(const int*)a)*4+1],
                      ps_sort_line[(*(const int*)a)*4+2], ps_sort_mode);
    float kb = ps_key(ps_sort_line[(*(const int*)b)*4  ],
                      ps_sort_line[(*(const int*)b)*4+1],
                      ps_sort_line[(*(const int*)b)*4+2], ps_sort_mode);
    return (ka > kb) - (ka < kb);
}

static void pixel_sort_ex(int mode, int lo, int hi, int vertical) {
    int w = ctx.w, h = ctx.h;
    int outer = vertical ? w : h;
    int inner = vertical ? h : w;

    unsigned char *src  = malloc(w * h * 4);
    int           *idx  = malloc(inner * sizeof(int));
    unsigned char *line = malloc(inner * 4);
    unsigned char *seg  = malloc(inner * 4);
    if (!src || !idx || !line || !seg) {
        free(src); free(idx); free(line); free(seg); return;
    }
    memcpy(src, ctx.pixels, w * h * 4);

    for (int o = 0; o < outer; o++) {
        /* extract line */
        for (int i = 0; i < inner; i++) {
            int px = vertical ? o : i, py = vertical ? i : o;
            memcpy(line + i*4, src + (py*w + px)*4, 4);
        }

        /* find threshold intervals and sort each */
        ps_sort_mode = mode;
        ps_sort_line = line;
        int i = 0;
        while (i < inner) {
            float k = ps_key(line[i*4], line[i*4+1], line[i*4+2], mode);
            if (k < lo || k > hi) { i++; continue; }
            int start = i, cnt = 0;
            while (i < inner) {
                k = ps_key(line[i*4], line[i*4+1], line[i*4+2], mode);
                if (k < lo || k > hi) break;
                idx[cnt++] = i++;
            }
            if (cnt > 1) {
                qsort(idx, cnt, sizeof(int), ps_cmp);
                for (int j = 0; j < cnt; j++)
                    memcpy(seg + j*4, line + idx[j]*4, 4);
                for (int j = 0; j < cnt; j++)
                    memcpy(line + (start+j)*4, seg + j*4, 4);
            }
        }

        /* write back */
        for (int i2 = 0; i2 < inner; i2++) {
            int px = vertical ? o : i2, py = vertical ? i2 : o;
            memcpy(ctx.pixels + (py*w + px)*4, line + i2*4, 4);
        }
    }
    free(src); free(idx); free(line); free(seg);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}

void pixel_sort(int amount) {
    save_undo_state();
    pixel_sort_ex(1, 60, 200, 0);
    printf("Applied: Pixel Sort -- ID: 009\n"); fflush(stdout);
}

/* ── Effect-Tabelle ──────────────────────────────────────────────────── */
/* param_count > 0  → popup with sliders; 0 → direct apply              */
static const Effect effects[] = {
    { 1, brighten,        30, "Brighten",  0, {{0}} },
    { 2, darken,          30, "Darken",    0, {{0}} },
    { 3, iceing,          30, "Iceing",    0, {{0}} },
    { 4, my_new_function, 30, "MyFunc",    0, {{0}} },
    { 5, negative,         0, "Negative",  0, {{0}} },
    { 6, kachel_function,  0, "Kachel",    0, {{0}} },
    { 7, gold,            40, "Gold",      0, {{0}} },
    { 8, rainbow,         60, "Rainbow Overlay v1",   2, {
        { "Intensity",  60,   0, 100,  5 },
        { "Direction",   0,   0, 359, 15 },
    }},
    { 9, pixel_sort,       0, "Pixel Sort",            4, {
        { "Mode  1=Lum 2=Hue 3=Sat",  1,  1,   3,  1 },
        { "Low threshold",            60,  0, 255,  5 },
        { "High threshold",          200,  0, 255,  5 },
        { "Direction  0=H 1=V",        0,  0,   1,  1 },
    }},
};
#define EFFECTS_N (int)(sizeof(effects)/sizeof(effects[0]))

/* Return pointer to effect entry (NULL if not found) */
const Effect *get_effect(int id) {
    for (int i = 0; i < EFFECTS_N; i++)
        if (effects[i].id == id) return &effects[i];
    return NULL;
}

/* Apply effect with explicit params array (used by popup preview + confirm) */
const char *apply_effect_params(int id, int *params, int n_params) {
    for (int i = 0; i < EFFECTS_N; i++) {
        if (effects[i].id != id) continue;
        if (id == 8) {
            /* Rainbow: intensity + direction */
            int intensity = (n_params > 0) ? params[0] : effects[i].params[0].value;
            int direction = (n_params > 1) ? params[1] : effects[i].params[1].value;
            save_undo_state();
            rainbow_ex(intensity, direction);
        } else if (id == 9) {
            /* Pixel Sort: mode, lo, hi, direction */
            int mode = (n_params > 0) ? params[0] : effects[i].params[0].value;
            int lo   = (n_params > 1) ? params[1] : effects[i].params[1].value;
            int hi   = (n_params > 2) ? params[2] : effects[i].params[2].value;
            int vert = (n_params > 3) ? params[3] : effects[i].params[3].value;
            save_undo_state();
            pixel_sort_ex(mode, lo, hi, vert);
        } else {
            /* All other effects: single amount */
            int amount = (n_params > 0) ? params[0] : effects[i].default_amount;
            effects[i].fn(amount);
        }
        return effects[i].name;
    }
    return NULL;
}

/* catching signals – gibt den Effektnamen zurück, NULL wenn nicht gefunden */
const char *on_number_confirmed(int n) {
    printf("Input: %d\n", n);
    fflush(stdout);
    for (int i = 0; i < (int)(sizeof(effects) / sizeof(effects[0])); i++) {
        if (effects[i].id == n) {
            effects[i].fn(effects[i].default_amount);
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

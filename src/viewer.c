#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"
#include <SDL2/SDL_ttf.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#else
#define MAX_PATH 4096
#include <strings.h>
#define _stricmp strcasecmp
#include <mach-o/dyld.h>
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "effects.h"
#include "license.h"
#include "render.h"
#include "vendor/tinyfiledialogs.h"

#define BOGOSHOP_VERSION "0.2.1_PRE_RELEASE"

#define HELP_MAX_LINES 512
#define HELP_LINE_LEN  256

/* Gibt den Ordner der laufenden .exe zurück (mit trailing Slash) */
static void get_exe_dir(char *out, int out_size) {
    out[0] = '\0';
#ifdef _WIN32
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof(exe));
    char *last = strrchr(exe, '\\');
    if (last) { *(last + 1) = '\0'; strncpy(out, exe, out_size - 1); }
#else
    char exe[MAX_PATH];
    uint32_t size = (uint32_t)sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) != 0) return;
    char *last = strrchr(exe, '/');
    if (last) { *(last + 1) = '\0'; strncpy(out, exe, out_size - 1); }
#endif
}

static int load_help(const char *rel_path, char lines[][HELP_LINE_LEN], int max) {
    char exe_dir[MAX_PATH];
    get_exe_dir(exe_dir, sizeof(exe_dir));
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", exe_dir, rel_path);
    FILE *f = fopen(full_path, "r");
    if (!f) return 0;
    int n = 0;
    while (n < max && fgets(lines[n], HELP_LINE_LEN, f)) {
        int len = (int)strlen(lines[n]);
        if (len > 0 && lines[n][len - 1] == '\n') lines[n][len - 1] = '\0';
        if (len > 1 && lines[n][len - 2] == '\r') lines[n][len - 2] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

static int open_file_dialog(char *out, int out_size) {
    static const char *filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp"};
    const char *result = tinyfd_openFileDialog("Open File", "", 4, filters, "Bilder", 0);
    if (!result) return 0;
    strncpy(out, result, out_size - 1);
    out[out_size - 1] = '\0';
    return 1;
}

static int save_file_dialog(char *out, int out_size) {
#ifdef _WIN32
    wchar_t wpath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, out, -1, wpath, MAX_PATH);
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter =
        L"PNG Image (*.png)\0*.png\0"
        L"JPEG Image (*.jpg)\0*.jpg;*.jpeg\0"
        L"Bitmap (*.bmp)\0*.bmp\0";
    ofn.nFilterIndex = 1;   /* default: PNG */
    ofn.lpstrFile    = wpath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = L"Export als...";
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt  = L"png";
    if (!GetSaveFileNameW(&ofn)) return 0;
    WideCharToMultiByte(CP_ACP, 0, wpath, -1, out, out_size, NULL, NULL);
    return 1;
#else
    static const char *filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp"};
    const char *result = tinyfd_saveFileDialog("Export als...", out, 4, filters, "Bilder");
    if (!result) return 0;
    strncpy(out, result, out_size - 1);
    out[out_size - 1] = '\0';
    return 1;
#endif
}

/* ── TTF font globals ────────────────────────────────────────────────────── */
static TTF_Font *font_sm     = NULL;  /* 12pt — windowed UI           */
static TTF_Font *font_lg     = NULL;  /* 17pt — fullscreen UI         */
static TTF_Font *font_xl     = NULL;  /* 64pt — big centre number     */
TTF_Font        *font_active = NULL;  /* points to font_sm or font_lg */

/* Internal helper: render one string with a specific font */
static void ttf_draw(SDL_Renderer *ren, float x, float y, const char *text,
                     TTF_Font *font, Uint8 r, Uint8 g, Uint8 b) {
    if (!font || !text || !text[0]) return;
    SDL_Color col = {r, g, b, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, col);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;
    int w, h;
    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    SDL_Rect dst = {(int)x, (int)y, w, h};
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

/* Render text with font_active */
void draw_text(SDL_Renderer *ren, float x, float y, const char *text,
               Uint8 r, Uint8 g, Uint8 b) {
    ttf_draw(ren, x, y, text, font_active, r, g, b);
}

/* scale > 8  → font_xl (big centre number)
   scale > 1.5 → font_lg (license screen titles, etc.)
   else        → font_active */
void draw_text_scaled(SDL_Renderer *ren, float x, float y, const char *text,
                      float scale, Uint8 r, Uint8 g, Uint8 b) {
    TTF_Font *f = (scale > 8.0f)  ? font_xl     :
                  (scale > 1.5f)  ? font_lg      : font_active;
    ttf_draw(ren, x, y, text, f, r, g, b);
}

int text_width(const char *text) {
    if (!font_active || !text || !text[0]) return 0;
    int w = 0;
    TTF_SizeText(font_active, text, &w, NULL);
    return w;
}

#ifdef _WIN32
static void apply_window_icon(SDL_Window *win) {
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(win, &wm)) return;
    HWND hwnd = wm.info.win.window;
    HICON big   = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON,
                    GetSystemMetrics(SM_CXICON),   GetSystemMetrics(SM_CYICON),   LR_DEFAULTCOLOR);
    HICON small_ = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON,
                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (big)    SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)big);
    if (small_) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)small_);
}
static void force_foreground(SDL_Window *win) {
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(win, &wm)) return;
    HWND hwnd    = wm.info.win.window;
    HWND fg      = GetForegroundWindow();
    DWORD fg_tid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
    DWORD my_tid = GetCurrentThreadId();
    if (fg_tid && fg_tid != my_tid) AttachThreadInput(fg_tid, my_tid, TRUE);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    if (fg_tid && fg_tid != my_tid) AttachThreadInput(fg_tid, my_tid, FALSE);
}
#else
static void apply_window_icon(SDL_Window *win) { (void)win; }
static void force_foreground(SDL_Window *win) { SDL_RaiseWindow(win); }
#endif

/* ══════════════════════════════════════════════════════════════════════════
   Effect Popup — embedded panel, main window expands to the right
   ══════════════════════════════════════════════════════════════════════════ */
#define POPUP_W           340  /* panel width added to the right of the main window */
#define POPUP_PREVIEW_MAX 3000 /* max dimension of fast-preview downsample */

/* ── State ───────────────────────────────────────────────────────────── */
/* popup_cur: mutable copy of a const Effect entry (param values adjusted by user) */
static int            popup_active   = 0;
static int            popup_sel      = 0;
static int            popup_panel_x  = 0;    /* x where panel begins */
static Effect         popup_cur;
static unsigned char *popup_saved_px   = NULL; /* full-res pixel backup */
static unsigned char *popup_preview_px = NULL; /* downscaled px for fast preview */
static unsigned char *popup_preview_og = NULL; /* backup of downscaled orig */
static SDL_Texture   *popup_preview_tex = NULL;
static int            popup_preview_w  = 0;
static int            popup_preview_h  = 0;

/* ── Effect-Suchpopup ────────────────────────────────────────────────── */
#define SEARCH_MAX_RESULTS 16

static int search_match(const char *haystack, const char *needle) {
    char h[128], n[64];
    int i;
    for (i = 0; haystack[i] && i < 127; i++) h[i] = (char)tolower((unsigned char)haystack[i]);
    h[i] = '\0';
    for (i = 0; needle[i] && i < 63; i++) n[i] = (char)tolower((unsigned char)needle[i]);
    n[i] = '\0';
    return strstr(h, n) != NULL;
}

static void search_filter_fn(const char *query, int *results, int *count) {
    *count = 0;
    for (int id = 1; id <= 100 && *count < SEARCH_MAX_RESULTS; id++) {
        const Effect *e = get_effect(id);
        if (!e) continue;
        char id_str[8];
        snprintf(id_str, sizeof(id_str), "%d", id);
        if (!query[0] || search_match(id_str, query) || search_match(e->name, query))
            results[(*count)++] = id;
    }
}

/* Returns 1 if effect n has a popup (param_count > 0) */
static int popup_has_template(int n) {
    const Effect *e = get_effect(n);
    return e != NULL && e->param_count > 0;
}

/* Nearest-neighbour downscale for fast preview buffer */
static void popup_downscale(const unsigned char *src, int sw, int sh,
                             unsigned char *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            const unsigned char *sp = src + (sy * sw + sx) * 4;
            unsigned char       *dp = dst + (y  * dw + x ) * 4;
            dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2]; dp[3]=sp[3];
        }
    }
}

/* Build params int-array from current popup_cur values */
static void popup_collect_vals(int *vals) {
    for (int i = 0; i < popup_cur.param_count; i++)
        vals[i] = popup_cur.params[i].value;
}

/* Apply effect for preview.
   Uses the small downscaled buffer if available (fast), else falls back to
   full-res (correct for small images where no downscale was needed). */
static void popup_apply_preview(void) {
    if (!popup_saved_px) return;
    int vals[EFFECT_MAX_PARAMS];
    popup_collect_vals(vals);

    if (popup_preview_px && popup_preview_og && popup_preview_tex) {
        /* Fast path: apply on small buffer only */
        int small_bytes = popup_preview_w * popup_preview_h * 4;
        memcpy(popup_preview_px, popup_preview_og, (size_t)small_bytes);

        /* Temporarily swap ctx to small buffer so effect functions work */
        unsigned char *sv_px  = ctx.pixels;
        int            sv_w   = ctx.w,  sv_h = ctx.h;
        SDL_Texture   *sv_tex = ctx.tex;

        ctx.pixels = popup_preview_px;
        ctx.w      = popup_preview_w;
        ctx.h      = popup_preview_h;
        ctx.tex    = popup_preview_tex;
        ctx.preview_mode = 1;

        apply_effect_params(popup_cur.id, vals, popup_cur.param_count);

        ctx.pixels = sv_px;
        ctx.w      = sv_w;
        ctx.h      = sv_h;
        ctx.tex    = sv_tex;
        ctx.preview_mode = 1; /* keep set so undo is suppressed */
    } else {
        /* Fallback: full-res apply (small image, no downscale needed) */
        memcpy(ctx.pixels, popup_saved_px, (size_t)ctx.w * ctx.h * 4);
        ctx.preview_mode = 1;
        apply_effect_params(popup_cur.id, vals, popup_cur.param_count);
    }
}

/* Draw the right-side panel onto the main renderer */
static void popup_render(SDL_Renderer *ren, int win_h) {
    if (!popup_active || !ren) return;

    int fh = font_active ? TTF_FontHeight(font_active) : 16;
    int lh = fh + 4;   /* line height for hints */
    int ph = fh + 6;   /* row height per param block (label + bar) */

    /* Dark panel background */
    SDL_SetRenderDrawColor(ren, 18, 18, 24, 255);
    SDL_Rect panel = { popup_panel_x, 0, POPUP_W, win_h };
    SDL_RenderFillRect(ren, &panel);

    /* Left border */
    SDL_SetRenderDrawColor(ren, 60, 60, 80, 255);
    SDL_RenderDrawLine(ren, popup_panel_x, 0, popup_panel_x, win_h);

    int x  = popup_panel_x + 14;
    int xe = popup_panel_x + POPUP_W - 14;
    int y  = BORDER;

    /* Title */
    draw_text(ren, x, y, popup_cur.name, 255, 220, 60);
    y += fh + 8;
    SDL_SetRenderDrawColor(ren, 55, 55, 70, 255);
    SDL_RenderDrawLine(ren, x, y, xe, y);
    y += 10;

    /* Params */
    for (int i = 0; i < popup_cur.param_count; i++) {
        EffectParam *p = &popup_cur.params[i];
        int sel = (i == popup_sel);

        /* Label */
        char lbuf[48];
        snprintf(lbuf, sizeof(lbuf), "%d  %s", i + 1, p->label);
        draw_text(ren, x, y, lbuf,
                  sel ? 255 : 140, sel ? 220 : 140, sel ? 60 : 140);
        y += fh + 4;

        /* Bar + value */
        int bar_h = 6;
        int bar_w = xe - x - 36;
        int fill  = bar_w * p->value / (p->max_val > 0 ? p->max_val : 1);
        SDL_SetRenderDrawColor(ren, 35, 35, 50, 255);
        SDL_Rect bg = { x, y, bar_w, bar_h };
        SDL_RenderFillRect(ren, &bg);
        if (fill > 0) {
            SDL_SetRenderDrawColor(ren, sel ? 90:50, sel ? 180:110, sel ? 60:40, 255);
            SDL_Rect fg = { x, y, fill, bar_h };
            SDL_RenderFillRect(ren, &fg);
        }
        char vbuf[12];
        snprintf(vbuf, sizeof(vbuf), "%d", p->value);
        draw_text(ren, x + bar_w + 6, y - (fh - bar_h) / 2, vbuf, 190, 190, 190);
        y += ph;
    }

    y += 8;
    SDL_SetRenderDrawColor(ren, 55, 55, 70, 255);
    SDL_RenderDrawLine(ren, x, y, xe, y);
    y += 10;

    draw_text(ren, x, y, "+/-  adjust",  65, 65, 85); y += lh;
    draw_text(ren, x, y, "1-8  select",  65, 65, 85); y += lh;
    draw_text(ren, x, y, "ENTER  apply", 65, 65, 85); y += lh;
    draw_text(ren, x, y, "ESC  cancel",  65, 65, 85);
}

/* Free all popup preview resources (call before closing popup) */
static void popup_free_preview(void) {
    if (popup_preview_px)  { free(popup_preview_px);  popup_preview_px  = NULL; }
    if (popup_preview_og)  { free(popup_preview_og);  popup_preview_og  = NULL; }
    if (popup_preview_tex) { SDL_DestroyTexture(popup_preview_tex); popup_preview_tex = NULL; }
    popup_preview_w = popup_preview_h = 0;
}

/* Short per-effect note shown in the search popup */
static const char *effect_note(int id) {
    switch (id) {
        case 1: return "lighten image";
        case 2: return "darken image";
        case 3: return "cool blue tint";
        case 4: return "custom function";
        case 5: return "invert colors";
        case 6: return "tile/mosaic";
        case 7: return "gold toning";
        case 8: return "rainbow overlay";
        case 9: return "glitch sort";
        default: return "";
    }
}

/* Draw the centered effect-search overlay.
   Layout is driven by font_active metrics — adapts automatically to
   windowed (font_sm/12pt) and fullscreen (font_lg/17pt). */
static void search_render(SDL_Renderer *ren, int win_w, int win_h,
                          const char *query, int sel, int *results, int count,
                          float ps) {
    (void)ps; /* no longer used; font_active handles sizing */

    int fh     = font_active ? TTF_FontHeight(font_active) : 16;
    int row_h  = fh + 5;
    int pad    = fh;
    int hdr_h  = fh + 14;
    int hint_h = fh + 4;
    int body_h = (count > 0 ? count : 1) * row_h + pad / 2;
    int sh     = hdr_h + body_h + hint_h + 6;
    int sw     = win_w / 2;
    if (sw < 340) sw = 340;
    if (sw > 640) sw = 640;
    int sx     = (win_w - sw) / 2;
    int sy     = win_h / 5;

    /* Column x positions */
    int col_id   = sx + pad / 2;
    int col_name = col_id + fh * 2;
    int col_note = col_name + sw / 3;

    /* Semi-transparent dim overlay */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_Rect overlay = { 0, 0, win_w, win_h };
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    /* Panel background */
    SDL_SetRenderDrawColor(ren, 18, 18, 26, 255);
    SDL_Rect panel = { sx, sy, sw, sh };
    SDL_RenderFillRect(ren, &panel);

    /* Border */
    SDL_SetRenderDrawColor(ren, 90, 90, 140, 255);
    SDL_RenderDrawRect(ren, &panel);

    /* Header: "Search: <query>[autocomplete]_" */
    {
        int ty  = sy + (hdr_h - fh) / 2;
        char prefix[] = "Search: ";
        draw_text(ren, sx + pad / 2, ty, prefix, 160, 160, 160);
        int px = sx + pad / 2 + text_width(prefix);

        /* typed query in yellow */
        if (query[0]) draw_text(ren, px, ty, query, 255, 220, 60);

        /* autocomplete suffix: show if query is a case-insensitive prefix */
        int qw = 0;
        if (query[0] && font_active)
            TTF_SizeText(font_active, query, &qw, NULL);
        if (count > 0 && query[0]) {
            const Effect *eff = get_effect(results[0]);
            if (eff) {
                int qlen = (int)strlen(query);
                int nlen = (int)strlen(eff->name);
                int match = (nlen > qlen);
                for (int i = 0; i < qlen && match; i++)
                    if (tolower((unsigned char)query[i]) !=
                        tolower((unsigned char)eff->name[i])) match = 0;
                if (match)
                    draw_text(ren, px + qw, ty, eff->name + qlen, 65, 65, 65);
            }
        }
        draw_text(ren, px + qw, ty, "_", 255, 220, 60);
    }

    /* Separator */
    SDL_SetRenderDrawColor(ren, 55, 55, 80, 255);
    SDL_RenderDrawLine(ren, sx, sy + hdr_h, sx + sw, sy + hdr_h);

    /* Results */
    if (count == 0) {
        draw_text(ren, col_id, sy + hdr_h + pad / 2, "No results", 70, 70, 90);
    } else {
        for (int i = 0; i < count; i++) {
            int ry     = sy + hdr_h + pad / 2 + i * row_h;
            int is_sel = (i == sel);
            if (is_sel) {
                SDL_SetRenderDrawColor(ren, 35, 55, 80, 255);
                SDL_Rect selrect = { sx + 1, ry - 2, sw - 2, row_h };
                SDL_RenderFillRect(ren, &selrect);
            }
            const Effect *eff = get_effect(results[i]);
            if (!eff) continue;

            /* ID column */
            char idbuf[8];
            snprintf(idbuf, sizeof(idbuf), "%d", results[i]);
            draw_text(ren, col_id, ry,
                      idbuf, is_sel ? 130:70, is_sel ? 180:80, is_sel ? 255:100);

            /* Name column */
            draw_text(ren, col_name + (is_sel ? 3 : 0), ry, eff->name,
                      is_sel ? 255:190, is_sel ? 220:190, is_sel ? 60:190);

            /* Note column */
            const char *note = effect_note(results[i]);
            if (note[0])
                draw_text(ren, col_note, ry, note, 70, 70, 90);
        }
    }

    /* Bottom hint */
    draw_text(ren, sx + pad / 2, sy + sh - hint_h,
              "UP/DOWN  TAB = complete  ENTER = apply  ESC = close", 50, 50, 70);
}

/* Open panel: save pixels, build downscaled preview, expand window */
static void popup_open(int effect_id, SDL_Window *win, int win_w, int win_h) {
    const Effect *tmpl = get_effect(effect_id);
    if (!tmpl || tmpl->param_count == 0) return;

    popup_cur = *tmpl; /* mutable working copy */
    popup_sel = 0;

    int nb = ctx.w * ctx.h * 4;
    popup_saved_px = malloc((size_t)nb);
    if (!popup_saved_px) return;
    memcpy(popup_saved_px, ctx.pixels, (size_t)nb);

    /* Build downscaled buffer for fast preview (only if image is large) */
    popup_free_preview();
    if (ctx.w > POPUP_PREVIEW_MAX || ctx.h > POPUP_PREVIEW_MAX) {
        float sx = (float)POPUP_PREVIEW_MAX / ctx.w;
        float sy = (float)POPUP_PREVIEW_MAX / ctx.h;
        float s  = sx < sy ? sx : sy;
        popup_preview_w = (int)(ctx.w * s);
        popup_preview_h = (int)(ctx.h * s);
        if (popup_preview_w < 1) popup_preview_w = 1;
        if (popup_preview_h < 1) popup_preview_h = 1;
        int small_bytes = popup_preview_w * popup_preview_h * 4;
        popup_preview_px  = malloc((size_t)small_bytes);
        popup_preview_og  = malloc((size_t)small_bytes);
        if (popup_preview_px && popup_preview_og) {
            popup_downscale(popup_saved_px, ctx.w, ctx.h,
                            popup_preview_og, popup_preview_w, popup_preview_h);
            memcpy(popup_preview_px, popup_preview_og, (size_t)small_bytes);
            popup_preview_tex = SDL_CreateTexture(ctx.ren,
                SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                popup_preview_w, popup_preview_h);
        } else {
            popup_free_preview(); /* alloc failed — fall back to full-res */
        }
    }
    /* If image is small enough, popup_preview_tex stays NULL → full-res path */

    popup_panel_x = win_w;
    SDL_SetWindowSize(win, win_w + POPUP_W, win_h);

    popup_apply_preview();
    popup_active = 1;
}

int main(int argc, char *argv[]) {
    char dialog_path[MAX_PATH] = {0};
    printf("Bogoshop v%s launched\n", BOGOSHOP_VERSION); fflush(stdout);

    /* ── SDL + TTF einmalig initialisieren ─────────────────────────────── */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() != 0) { SDL_Quit(); return 1; }

    /* ── Schrift laden ──────────────────────────────────────────────────── */
    {
        char font_path[MAX_PATH];
        get_exe_dir(font_path, sizeof(font_path));
        strncat(font_path, "assets/JetBrainsMono-Regular.ttf",
                sizeof(font_path) - strlen(font_path) - 1);
        font_sm = TTF_OpenFont(font_path, 12);
        font_lg = TTF_OpenFont(font_path, 17);
        font_xl = TTF_OpenFont(font_path, 64);
        if (!font_sm || !font_lg || !font_xl)
            fprintf(stderr, "Font load failed: %s\n", TTF_GetError());
        font_active = font_sm;
    }

    /* ── Lizenz prüfen ── */
    if (!load_license()) {
        /* Temporäres Fenster für den Lizenz-Screen */
        SDL_Window   *lwin = SDL_CreateWindow("Bogoshop",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                480, 320, 0);
        apply_window_icon(lwin);
        force_foreground(lwin);
        SDL_Renderer *lren = SDL_CreateRenderer(lwin, -1, SDL_RENDERER_ACCELERATED);
        int ok = show_license_screen(lwin, lren);
        SDL_DestroyRenderer(lren);
        SDL_DestroyWindow(lwin);
        if (!ok) {
            if (font_sm) TTF_CloseFont(font_sm);
            if (font_lg) TTF_CloseFont(font_lg);
            if (font_xl) TTF_CloseFont(font_xl);
            TTF_Quit(); SDL_Quit();
            return 0;
        }
    }

    if (argc < 2) {
        if (!open_file_dialog(dialog_path, sizeof(dialog_path)))
            return 0;
        argv[1] = dialog_path;
    }

    /* Dateipfad für Speichern merken */
    static char current_path[MAX_PATH];
    strncpy(current_path, argv[1], sizeof(current_path) - 1);

    /* ── Sicherheitskopie in %APPDATA%\bogoshop\ anlegen ── */
#ifdef _WIN32
    {
        const char *appdata = getenv("APPDATA");
        if (appdata) {
            char backup_dir[MAX_PATH];
            snprintf(backup_dir, sizeof(backup_dir), "%s\\bogoshop", appdata);
            CreateDirectoryA(backup_dir, NULL); /* ignoriert Fehler wenn Ordner existiert */

            /* Dateiname aus Pfad extrahieren */
            const char *fname = strrchr(current_path, '\\');
            fname = fname ? fname + 1 : current_path;

            /* Zufälligen 8-stelligen Hex-Suffix generieren */
            unsigned int rnd = (unsigned int)(GetTickCount() ^ (uintptr_t)fname);
            rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5;

            /* Suffix vor der Extension einfügen: backup_img_2_A3F20B1C.jpg */
            const char *dot = strrchr(fname, '.');
            char backup_path[MAX_PATH];
            if (dot) {
                char stem[MAX_PATH];
                int stem_len = (int)(dot - fname);
                snprintf(stem, sizeof(stem), "%.*s", stem_len, fname);
                snprintf(backup_path, sizeof(backup_path),
                         "%s\\backup_%s_%08X%s", backup_dir, stem, rnd, dot);
            } else {
                snprintf(backup_path, sizeof(backup_path),
                         "%s\\backup_%s_%08X", backup_dir, fname, rnd);
            }

            /* Binär kopieren */
            FILE *src = fopen(current_path, "rb");
            FILE *dst_f = fopen(backup_path, "wb");
            if (src && dst_f) {
                char buf[65536];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                    fwrite(buf, 1, n, dst_f);
                printf("Backup created: %s\n", backup_path); fflush(stdout);
            }
            if (src)   fclose(src);
            if (dst_f) fclose(dst_f);
        }
    }
#endif

    int channels;
    ctx.pixels = stbi_load(current_path, &ctx.w, &ctx.h, &channels, 4);
    if (!ctx.pixels) {
        fprintf(stderr, "Image failed to load: %s\n", stbi_failure_reason());
        return 1;
    }
    ctx.orig_w = ctx.w;
    ctx.orig_h = ctx.h;
    ctx.needs_layout_update = 0;
    int total_bytes = ctx.w * ctx.h * 4;
    ctx.original_pixels = malloc(total_bytes);
    memcpy(ctx.original_pixels, ctx.pixels, total_bytes);
    for (int i = 0; i < UNDO_HISTORY; i++)
        ctx.undo_stack[i] = malloc(total_bytes);
    ctx.undo_head  = 0;
    ctx.undo_count = 0;

    float zoom_level  = 1.0f;
    int   mirror_mode = 0;
    int   fullscreen  = 0;
    float ui_scale    = 1.0f;   /* 1.0 windowed, 1.5 fullscreen */
    int   fs_saved_win_w = 0, fs_saved_win_h = 0;
    int   pending_save = 0;
    int   pending_quit = 0;
    int   res_mode         = 0;  /* 0=off  1=canvas resize mode active */
    int   canvas_fill_mode = 5;  /* 1=Fill 2=Fit 3=Stretch 4=Tile 5=Center 6=Span */
    int   canvas_submode   = 1;  /* +1=expand  -1=shrink */
    Uint8 bg_r = 0, bg_g = 0, bg_b = 0;
    int max_img = MAX_SIZE - 2 * BORDER;
    float scale = 1.0f;
    if (ctx.w > max_img || ctx.h > max_img) {
        float sx = (float)max_img / ctx.w;
        float sy = (float)max_img / ctx.h;
        scale = sx < sy ? sx : sy;
    }
    int draw_w = (int)(ctx.w * scale);
    int draw_h = (int)(ctx.h * scale);
    int base_draw_w = draw_w;
    int base_draw_h = draw_h;

    int win_w = draw_w + 2 * BORDER;
    int win_h = draw_h + 2 * BORDER;

    int comp_w = ctx.w;  /* composition frame width  (can be < ctx.w = virtual shrink) */
    int comp_h = ctx.h;  /* composition frame height */
    int comp_dirty = 0;  /* 1 = recalculate layout from comp_w/comp_h */
    int transform_mode = 0;              /* T = fit original into comp */
    unsigned char *pre_comp_pixels = NULL; /* pixels saved when entering comp mode */
    int pre_comp_w = 0, pre_comp_h = 0;

    char win_title[MAX_PATH + 64];
    snprintf(win_title, sizeof(win_title), "Bogoshop v" BOGOSHOP_VERSION);
    SDL_Window *win = SDL_CreateWindow(
        win_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, 0
    );
    apply_window_icon(win);
    force_foreground(win);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); /* bilinear filtering beim Zoom */
    ctx.ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    ctx.win = win;
    ctx.tex = SDL_CreateTexture(
        ctx.ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, ctx.w, ctx.h
    );
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);



    SDL_Rect dst   = { BORDER, BORDER, draw_w, draw_h };
    float    bot_y = 0; /* computed each frame below */

    char input[16] = {0};
    int  input_len       = 0;
    int  input_confirmed = 0;   /* 1 = Enter wurde gedrückt, Wert wird gehalten */
    char last_effect[128] = {0};
    int  help_mode         = 0;
    int  help_scroll       = 0;
    int  help_line_count   = 0;
    int  help_saved_win_w  = 0;
    int  help_saved_win_h  = 0;
    SDL_Texture *help_tex_before = NULL;
    SDL_Texture *help_tex_after  = NULL;
    int  help_preview_w    = 0;
    int  help_preview_h    = 0;
    static char help_lines[HELP_MAX_LINES][HELP_LINE_LEN];

    int   search_active    = 0;
    char  search_input[64] = {0};
    int   search_len       = 0;
    int   search_sel       = 0;
    int   search_results[SEARCH_MAX_RESULTS];
    int   search_count     = 0;

    SDL_Event e;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                /* ── Search popup keys (intercept all input) ───────── */
                if (search_active) {
                    if (k == SDLK_ESCAPE) {
                        search_active = 0;
                        search_input[0] = '\0'; search_len = 0;
                        SDL_StopTextInput();
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        if (search_count > 0) {
                            int sel_id = search_results[search_sel];
                            search_active = 0;
                            search_input[0] = '\0'; search_len = 0;
                            SDL_StopTextInput();
                            if (popup_has_template(sel_id)) {
                                popup_open(sel_id, win, win_w, win_h);
                                snprintf(last_effect, sizeof(last_effect), "%s...", popup_cur.name);
                            } else {
                                const char *name = on_number_confirmed(sel_id);
                                if (name) snprintf(last_effect, sizeof(last_effect), "%s", name);
                                else      snprintf(last_effect, sizeof(last_effect), "???");
                            }
                            input_confirmed = 1; input_len = 0; input[0] = '\0';
                        }
                    } else if (k == SDLK_UP) {
                        if (search_sel > 0) search_sel--;
                    } else if (k == SDLK_DOWN) {
                        if (search_sel < search_count - 1) search_sel++;
                    } else if (k == SDLK_BACKSPACE && search_len > 0) {
                        search_input[--search_len] = '\0';
                        search_filter_fn(search_input, search_results, &search_count);
                        if (search_sel >= search_count) search_sel = search_count > 0 ? search_count - 1 : 0;
                    } else if (k == SDLK_TAB && search_count > 0) {
                        /* Autocomplete: fill in first result's name */
                        const Effect *eff = get_effect(search_results[0]);
                        if (eff) {
                            strncpy(search_input, eff->name, sizeof(search_input) - 1);
                            search_input[sizeof(search_input) - 1] = '\0';
                            search_len = (int)strlen(search_input);
                            search_filter_fn(search_input, search_results, &search_count);
                            search_sel = 0;
                        }
                    }
                }
                /* ── Popup panel keys (intercept all input) ────────── */
                else if (popup_active) {
                    if (k == SDLK_ESCAPE) {
                        /* cancel: restore original pixels */
                        if (popup_saved_px) {
                            memcpy(ctx.pixels, popup_saved_px,
                                   (size_t)ctx.w * ctx.h * 4);
                            free(popup_saved_px); popup_saved_px = NULL;
                        }
                        ctx.preview_mode = 0;
                        SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
                        popup_free_preview();
                        SDL_SetWindowSize(win, win_w, win_h);
                        popup_active = 0;
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        /* confirm: restore backup then apply full-res (saves undo) */
                        if (popup_saved_px) {
                            memcpy(ctx.pixels, popup_saved_px,
                                   (size_t)ctx.w * ctx.h * 4);
                            free(popup_saved_px); popup_saved_px = NULL;
                        }
                        ctx.preview_mode = 0;
                        popup_free_preview();
                        { int vals[EFFECT_MAX_PARAMS];
                          popup_collect_vals(vals);
                          apply_effect_params(popup_cur.id, vals, popup_cur.param_count); }
                        SDL_SetWindowSize(win, win_w, win_h);
                        popup_active = 0;
                        snprintf(last_effect, sizeof(last_effect),
                                 "%s", popup_cur.name);
                        input_confirmed = 1;
                    } else if (k == SDLK_EQUALS || k == SDLK_PLUS ||
                               k == SDLK_KP_PLUS) {
                        EffectParam *p = &popup_cur.params[popup_sel];
                        p->value += p->step;
                        if (p->value > p->max_val) p->value = p->max_val;
                        popup_apply_preview();
                    } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) {
                        EffectParam *p = &popup_cur.params[popup_sel];
                        p->value -= p->step;
                        if (p->value < p->min_val) p->value = p->min_val;
                        popup_apply_preview();
                    } else if (k >= SDLK_1 && k <= SDLK_8) {
                        int idx = k - SDLK_1;
                        if (idx < popup_cur.param_count) popup_sel = idx;
                    }
                } else {

#define HELP_WIN_W 900
#define HELP_WIN_H 740
                /* ── Global keys (all non-popup modes) ─────────────────── */
                if (k == SDLK_h) {
                    if (help_mode) {
                        help_mode = 0;
                        if (!fullscreen) {
                            win_w = help_saved_win_w;
                            win_h = help_saved_win_h;
                            SDL_SetWindowSize(ctx.win, win_w, win_h);
                            SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        }
                        if (help_tex_before) { SDL_DestroyTexture(help_tex_before); help_tex_before = NULL; }
                        if (help_tex_after)  { SDL_DestroyTexture(help_tex_after);  help_tex_after  = NULL; }
                    } else {
                        char path[MAX_PATH];
                        int effect_n = 0;
                        if (input_len > 0) {
                            effect_n = atoi(input);
                            snprintf(path, sizeof(path), "docs/effects/%d.txt", effect_n);
                            input_len = 0; input[0] = '\0';
                        } else {
                            snprintf(path, sizeof(path), "docs/README.txt");
                        }
                        help_line_count = load_help(path, help_lines, HELP_MAX_LINES);
                        if (help_line_count > 0) {
                            if (!fullscreen) {
                                help_saved_win_w = win_w;
                                help_saved_win_h = win_h;
                                win_w = HELP_WIN_W;
                                win_h = HELP_WIN_H;
                                SDL_SetWindowSize(ctx.win, win_w, win_h);
                                SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                            }
                            help_mode   = 1;
                            help_scroll = 0;
                            input_confirmed = 0;
                            if (help_tex_before) { SDL_DestroyTexture(help_tex_before); help_tex_before = NULL; }
                            if (help_tex_after)  { SDL_DestroyTexture(help_tex_after);  help_tex_after  = NULL; }
                            if (effect_n > 0)
                                make_preview(effect_n, &help_tex_before, &help_tex_after,
                                             &help_preview_w, &help_preview_h);
                        } else {
                            snprintf(last_effect, sizeof(last_effect),
                                     "Help: file not found (%s)", path);
                            input_confirmed = 1;
                        }
                    }
                } else if (k == SDLK_ESCAPE || k == SDLK_q) {
                    if (help_mode) {
                        help_mode = 0;
                        if (!fullscreen) {
                            win_w = help_saved_win_w;
                            win_h = help_saved_win_h;
                            SDL_SetWindowSize(ctx.win, win_w, win_h);
                            SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                        }
                        if (help_tex_before) { SDL_DestroyTexture(help_tex_before); help_tex_before = NULL; }
                        if (help_tex_after)  { SDL_DestroyTexture(help_tex_after);  help_tex_after  = NULL; }
                    } else if (pending_quit) {
                        running = 0; /* zweites Q/ESC → force quit */
                    } else {
                        pending_quit  = 1;
                        pending_save  = 0;
                        input_len     = 0; input[0] = '\0';
                        snprintf(last_effect, sizeof(last_effect),
                                 "Quit? S = save & quit  E = export & quit  Q/ESC = force quit");
                        input_confirmed = 1;
                    }
                } else if (k == SDLK_r) {
                    zoom_level  = 1.0f;
                    mirror_mode = 0;
                    res_mode    = 0;
                    draw_w = base_draw_w;
                    draw_h = base_draw_h;
                    dst.x  = BORDER;
                    dst.y  = BORDER;
                    dst.w  = draw_w;
                    dst.h  = draw_h;
                    reset_image();
                    comp_w = ctx.w; comp_h = ctx.h;
                    transform_mode = 0;
                    if (pre_comp_pixels) { free(pre_comp_pixels); pre_comp_pixels = NULL; }
                    pre_comp_w = 0; pre_comp_h = 0;
                } else if (k == SDLK_u) {
                    undo_last();
                } else if (k == SDLK_BACKSPACE && input_len > 0) {
                    input[--input_len] = '\0';
                    input_confirmed    = 0;
                } else if (k == SDLK_f && !(e.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT))) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    if (!fullscreen) {
                        SDL_SetWindowSize(win, win_w, win_h);
                    }
                    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    snprintf(last_effect, sizeof(last_effect), fullscreen ? "Fullscreen ON" : "Fullscreen OFF");
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (pending_save && (k == SDLK_y)) {
                    pending_save = 0;
                    const char *ext = strrchr(current_path, '.');
                    int ok = 0;
                    if (ext && (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0))
                        ok = stbi_write_jpg(current_path, ctx.w, ctx.h, 4, ctx.pixels, 92);
                    else if (ext && _stricmp(ext, ".bmp") == 0)
                        ok = stbi_write_bmp(current_path, ctx.w, ctx.h, 4, ctx.pixels);
                    else
                        ok = stbi_write_png(current_path, ctx.w, ctx.h, 4, ctx.pixels, ctx.w * 4);
                    snprintf(last_effect, sizeof(last_effect),
                             ok ? "Saved!" : "Save failed");
                    input_confirmed = 1;
                } else if (pending_save && (k == SDLK_n)) {
                    pending_save = 0;
                    snprintf(last_effect, sizeof(last_effect), "Save cancelled");
                    input_confirmed = 1;
                } else if (pending_quit && k == SDLK_s) {
                    /* Save & quit — works in any mode */
                    pending_quit = 0;
                    const char *ext = strrchr(current_path, '.');
                    int ok = 0;
                    if (ext && (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0))
                        ok = stbi_write_jpg(current_path, ctx.w, ctx.h, 4, ctx.pixels, 92);
                    else if (ext && _stricmp(ext, ".bmp") == 0)
                        ok = stbi_write_bmp(current_path, ctx.w, ctx.h, 4, ctx.pixels);
                    else
                        ok = stbi_write_png(current_path, ctx.w, ctx.h, 4, ctx.pixels, ctx.w * 4);
                    snprintf(last_effect, sizeof(last_effect),
                             ok ? "Saved!" : "Save failed");
                    input_confirmed = 1;
                    running = 0;
                } else if (pending_quit && k == SDLK_e) {
                    /* Export & quit — works in any mode */
                    pending_quit = 0;
                    char export_path[MAX_PATH];
                    strncpy(export_path, current_path, sizeof(export_path) - 1);
                    if (save_file_dialog(export_path, sizeof(export_path))) {
                        const char *ext = strrchr(export_path, '.');
                        int ok = 0;
                        if (ext && (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0))
                            ok = stbi_write_jpg(export_path, ctx.w, ctx.h, 4, ctx.pixels, 92);
                        else if (ext && _stricmp(ext, ".bmp") == 0)
                            ok = stbi_write_bmp(export_path, ctx.w, ctx.h, 4, ctx.pixels);
                        else
                            ok = stbi_write_png(export_path, ctx.w, ctx.h, 4, ctx.pixels, ctx.w * 4);
                        snprintf(last_effect, sizeof(last_effect),
                                 ok ? "Exported!" : "Export failed");
                        running = 0;
                    } else {
                        snprintf(last_effect, sizeof(last_effect), "Quit cancelled");
                    }
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (help_mode && (k == SDLK_UP || k == SDLK_DOWN)) {
                    /* Help scroll — global */
                    if (k == SDLK_UP   && help_scroll > 0) help_scroll--;
                    if (k == SDLK_DOWN && help_scroll < help_line_count - 1) help_scroll++;
                }
                /* ── Space: open effect search ─────────────────────────── */
                else if (k == SDLK_SPACE && !help_mode && !pending_save && !pending_quit) {
                    search_active = 1;
                    search_input[0] = '\0'; search_len = 0;
                    search_filter_fn(search_input, search_results, &search_count);
                    search_sel = 0;
                    SDL_StartTextInput();
                }
                /* ── Composition mode ──────────────────────────────────── */
                else if (res_mode) {
                    if (k == SDLK_UP || k == SDLK_DOWN ||
                        k == SDLK_LEFT || k == SDLK_RIGHT) {
                        int shifted = (e.key.keysym.mod & KMOD_SHIFT) != 0;
                        int al=0,ar=0,at=0,ab=0;
                        if (canvas_submode > 0) {
                            if (!shifted) {
                                if (k == SDLK_LEFT)  al =  10;
                                if (k == SDLK_RIGHT) ar =  10;
                                if (k == SDLK_UP)    at =  10;
                                if (k == SDLK_DOWN)  ab =  10;
                            } else {
                                if (k == SDLK_LEFT || k == SDLK_RIGHT) { al = 10; ar = 10; }
                                if (k == SDLK_UP   || k == SDLK_DOWN)  { at = 10; ab = 10; }
                            }
                        } else {
                            if (!shifted) {
                                if (k == SDLK_LEFT)  ar = -10;
                                if (k == SDLK_RIGHT) al = -10;
                                if (k == SDLK_UP)    ab = -10;
                                if (k == SDLK_DOWN)  at = -10;
                            } else {
                                if (k == SDLK_LEFT || k == SDLK_RIGHT) { al = -10; ar = -10; }
                                if (k == SDLK_UP   || k == SDLK_DOWN)  { at = -10; ab = -10; }
                            }
                        }
                        if (canvas_submode > 0) {
                            canvas_resize(al, ar, at, ab, canvas_fill_mode);
                            comp_w = ctx.w; comp_h = ctx.h;
                            snprintf(last_effect, sizeof(last_effect), "Comp %dx%d", ctx.w, ctx.h);
                        } else {
                            int dw = al + ar, dh = at + ab;
                            comp_w += dw; if (comp_w < 1) comp_w = 1; if (comp_w > ctx.w) comp_w = ctx.w;
                            comp_h += dh; if (comp_h < 1) comp_h = 1; if (comp_h > ctx.h) comp_h = ctx.h;
                            comp_dirty = 1;
                            snprintf(last_effect, sizeof(last_effect), "Comp frame %dx%d  (image %dx%d)", comp_w, comp_h, ctx.w, ctx.h);
                        }
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) {
                        int nw = comp_w + 20;
                        int nh = ctx.w > 0 ? (int)((float)nw * ctx.h / ctx.w + 0.5f) : nw;
                        int dw = nw - ctx.w, dh = nh - ctx.h;
                        if (dw > 0 || dh > 0) {
                            int xdw = dw > 0 ? dw : 0, xdh = dh > 0 ? dh : 0;
                            canvas_resize(xdw/2, xdw - xdw/2, xdh/2, xdh - xdh/2, canvas_fill_mode);
                        }
                        comp_w = nw < ctx.w ? nw : ctx.w;
                        comp_h = nh < ctx.h ? nh : ctx.h;
                        comp_dirty = 1;
                        snprintf(last_effect, sizeof(last_effect), "Comp frame %dx%d", comp_w, comp_h);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) {
                        int nw = comp_w - 20; if (nw < 1) nw = 1;
                        int nh = ctx.w > 0 ? (int)((float)nw * ctx.h / ctx.w + 0.5f) : nw;
                        if (nh < 1) nh = 1;
                        comp_w = nw; comp_h = nh;
                        comp_dirty = 1;
                        snprintf(last_effect, sizeof(last_effect), "Comp frame %dx%d  (image %dx%d)", comp_w, comp_h, ctx.w, ctx.h);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_s) {
                        canvas_submode = -1;
                        snprintf(last_effect, sizeof(last_effect), "Comp: SHRINK  arrows=1 side  Shift+arrows=2 sides");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_e) {
                        canvas_submode = 1;
                        snprintf(last_effect, sizeof(last_effect), "Comp: EXPAND  arrows=1 side  Shift+arrows=2 sides");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        if (input_len > 0) {
                            char *xsep = strchr(input, 'x');
                            char *sep  = strchr(input, ':');
                            if (xsep && xsep != input && *(xsep + 1) != '\0') {
                                int new_w = atoi(input);
                                int new_h = atoi(xsep + 1);
                                if (new_w >= 1 && new_h >= 1) {
                                    int dw = new_w - ctx.w, dh = new_h - ctx.h;
                                    if (dw > 0 || dh > 0) {
                                        int xdw = dw > 0 ? dw : 0, xdh = dh > 0 ? dh : 0;
                                        canvas_resize(xdw/2, xdw - xdw/2, xdh/2, xdh - xdh/2, canvas_fill_mode);
                                    }
                                    comp_w = new_w < ctx.w ? new_w : ctx.w;
                                    comp_h = new_h < ctx.h ? new_h : ctx.h;
                                    comp_dirty = 1;
                                    snprintf(last_effect, sizeof(last_effect),
                                             "Comp frame %dx%d  (image %dx%d)", comp_w, comp_h, ctx.w, ctx.h);
                                } else {
                                    snprintf(last_effect, sizeof(last_effect), "Invalid size");
                                }
                            } else if (sep && sep != input && *(sep + 1) != '\0') {
                                int rw = atoi(input);
                                int rh = atoi(sep + 1);
                                if (rw >= 1 && rh >= 1) {
                                    int new_w, new_h;
                                    if ((long long)comp_w * rh >= (long long)comp_h * rw) {
                                        new_w = comp_w; new_h = comp_w * rh / rw;
                                    } else {
                                        new_h = comp_h; new_w = comp_h * rw / rh;
                                    }
                                    int dw = new_w - ctx.w, dh = new_h - ctx.h;
                                    if (dw > 0 || dh > 0) {
                                        int xdw = dw > 0 ? dw : 0, xdh = dh > 0 ? dh : 0;
                                        canvas_resize(xdw/2, xdw - xdw/2, xdh/2, xdh - xdh/2, canvas_fill_mode);
                                    }
                                    comp_w = new_w < ctx.w ? new_w : ctx.w;
                                    comp_h = new_h < ctx.h ? new_h : ctx.h;
                                    comp_dirty = 1;
                                    snprintf(last_effect, sizeof(last_effect), "Comp %d:%d = %dx%d", rw, rh, comp_w, comp_h);
                                }
                            } else {
                                int n = atoi(input);
                                if (n >= 1 && n <= 1000) {
                                    if (popup_has_template(n)) {
                                        popup_open(n, win, win_w, win_h);
                                        snprintf(last_effect, sizeof(last_effect),
                                                 "%s...", popup_cur.name);
                                    } else {
                                        const char *name = on_number_confirmed(n);
                                        if (name) snprintf(last_effect, sizeof(last_effect), "%s", name);
                                        else      snprintf(last_effect, sizeof(last_effect), "???");
                                    }
                                }
                            }
                            input_confirmed = 1; input_len = 0; input[0] = '\0';
                        }
                    } else if (k == SDLK_c && !(e.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT))) {
                        /* Exit composition mode — commit virtual shrink if pending */
                        res_mode = 0;
                        transform_mode = 0;
                        if (comp_w < ctx.w || comp_h < ctx.h) {
                            /* Virtual shrink → apply as real crop */
                            int dl = ctx.w - comp_w, dt = ctx.h - comp_h;
                            canvas_resize(-(dl/2), -(dl - dl/2), -(dt/2), -(dt - dt/2), 5 /* CENTER: keep content */);
                        }
                        comp_w = ctx.w; comp_h = ctx.h;
                        ctx.needs_layout_update = 1;
                        if (pre_comp_pixels)
                            snprintf(last_effect, sizeof(last_effect), "Comp mode OFF  |  T = transform original into canvas");
                        else
                            snprintf(last_effect, sizeof(last_effect), "Comp mode OFF");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    }
                }
                /* ── Transform mode ────────────────────────────────────── */
                else if (transform_mode) {
                    if (!(e.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT)) &&
                        e.key.keysym.scancode >= SDL_SCANCODE_1 &&
                        e.key.keysym.scancode <= SDL_SCANCODE_6) {
                        static const char *fnames[] = {"","Fill","Fit","Stretch","Tile","Center","Span"};
                        canvas_fill_mode = (int)(e.key.keysym.scancode - SDL_SCANCODE_1) + 1;
                        if (pre_comp_pixels) {
                            transform_fill(pre_comp_pixels, pre_comp_w, pre_comp_h, canvas_fill_mode);
                            snprintf(last_effect, sizeof(last_effect),
                                     "Transform %s  %dx%d", fnames[canvas_fill_mode], ctx.w, ctx.h);
                        }
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_t && !(e.key.keysym.mod & KMOD_SHIFT)) {
                        transform_mode = 0;
                        snprintf(last_effect, sizeof(last_effect), "Transform OFF");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        if (input_len > 0) {
                            int n = atoi(input);
                            if (n >= 1 && n <= 1000) {
                                if (popup_has_template(n)) {
                                    popup_open(n, win, win_w, win_h);
                                    snprintf(last_effect, sizeof(last_effect),
                                             "%s...", popup_cur.name);
                                } else {
                                    const char *name = on_number_confirmed(n);
                                    if (name) snprintf(last_effect, sizeof(last_effect), "%s", name);
                                    else      snprintf(last_effect, sizeof(last_effect), "???");
                                }
                            }
                            input_confirmed = 1;
                        }
                    }
                }
                /* ── Normal mode ────────────────────────────────────────── */
                else {
                    if (k == SDLK_UP || k == SDLK_DOWN ||
                        k == SDLK_LEFT || k == SDLK_RIGHT) {
                        if (mirror_mode || zoom_level > 1.0f) {
                            int step = mirror_mode && zoom_level <= 1.0f
                                       ? (draw_w > 4 ? draw_w / 4 : 1)
                                       : (win_w - 2 * BORDER) / 8;
                            if (k == SDLK_LEFT)  dst.x += step;
                            if (k == SDLK_RIGHT) dst.x -= step;
                            if (k == SDLK_UP)    dst.y += step;
                            if (k == SDLK_DOWN)  dst.y -= step;
                            if (!mirror_mode) {
                                int iw = win_w - 2 * BORDER, ih = win_h - 2 * BORDER;
                                if (draw_w > iw) {
                                    if (dst.x > BORDER)                  dst.x = BORDER;
                                    if (dst.x + draw_w < win_w - BORDER) dst.x = win_w - BORDER - draw_w;
                                }
                                if (draw_h > ih) {
                                    if (dst.y > BORDER)                  dst.y = BORDER;
                                    if (dst.y + draw_h < win_h - BORDER) dst.y = win_h - BORDER - draw_h;
                                }
                            }
                        } else {
                            int sw = ctx.crop_active ? ctx.crop_src_w : 1;
                            int sh = ctx.crop_active ? ctx.crop_src_h : 1;
                            int step = sw > sh ? sw / 20 : sh / 20;
                            if (step < 5) step = 5;
                            if (k == SDLK_UP)    crop_pan(0,    -step);
                            if (k == SDLK_DOWN)  crop_pan(0,     step);
                            if (k == SDLK_LEFT)  crop_pan(-step, 0);
                            if (k == SDLK_RIGHT) crop_pan( step, 0);
                        }
                    } else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) {
                        { int old_dw = draw_w, old_dh = draw_h;
                        zoom_level += 0.25f;
                        if (zoom_level > 4.0f) zoom_level = 4.0f;
                        draw_w = (int)(base_draw_w * zoom_level);
                        draw_h = (int)(base_draw_h * zoom_level);
                        dst.x  = win_w/2 - (win_w/2 - dst.x) * draw_w / old_dw;
                        dst.y  = win_h/2 - (win_h/2 - dst.y) * draw_h / old_dh;
                        dst.w  = draw_w;
                        dst.h  = draw_h;
                        if (!mirror_mode) {
                            int iw = win_w-2*BORDER, ih = win_h-2*BORDER;
                            if (draw_w <= iw) { dst.x = BORDER; }
                            else { if (dst.x > BORDER) dst.x = BORDER; if (dst.x+draw_w < win_w-BORDER) dst.x = win_w-BORDER-draw_w; }
                            if (draw_h <= ih) { dst.y = BORDER; }
                            else { if (dst.y > BORDER) dst.y = BORDER; if (dst.y+draw_h < win_h-BORDER) dst.y = win_h-BORDER-draw_h; }
                        }
                        }
                        snprintf(last_effect, sizeof(last_effect), "Zoom %.2fx", zoom_level);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) {
                        { int old_dw = draw_w, old_dh = draw_h;
                        zoom_level -= 0.25f;
                        if (!mirror_mode && zoom_level < 1.0f) zoom_level = 1.0f;
                        if (zoom_level < 0.25f) zoom_level = 0.25f;
                        draw_w = (int)(base_draw_w * zoom_level);
                        draw_h = (int)(base_draw_h * zoom_level);
                        dst.x  = win_w/2 - (win_w/2 - dst.x) * draw_w / old_dw;
                        dst.y  = win_h/2 - (win_h/2 - dst.y) * draw_h / old_dh;
                        dst.w  = draw_w;
                        dst.h  = draw_h;
                        if (!mirror_mode) {
                            int iw = win_w-2*BORDER, ih = win_h-2*BORDER;
                            if (draw_w <= iw) { dst.x = BORDER; }
                            else { if (dst.x > BORDER) dst.x = BORDER; if (dst.x+draw_w < win_w-BORDER) dst.x = win_w-BORDER-draw_w; }
                            if (draw_h <= ih) { dst.y = BORDER; }
                            else { if (dst.y > BORDER) dst.y = BORDER; if (dst.y+draw_h < win_h-BORDER) dst.y = win_h-BORDER-draw_h; }
                        }
                        }
                        snprintf(last_effect, sizeof(last_effect), "Zoom %.2fx", zoom_level);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_m) {
                        mirror_mode = !mirror_mode;
                        snprintf(last_effect, sizeof(last_effect),
                                 mirror_mode ? "Mirror ON" : "Mirror OFF");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_c && !(e.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT))) {
                        /* Enter composition mode */
                        res_mode = 1;
                        canvas_submode = 1;
                        comp_w = ctx.w; comp_h = ctx.h;
                        if (pre_comp_pixels) free(pre_comp_pixels);
                        pre_comp_pixels = malloc((size_t)ctx.w * ctx.h * 4);
                        if (pre_comp_pixels)
                            memcpy(pre_comp_pixels, ctx.pixels, (size_t)ctx.w * ctx.h * 4);
                        pre_comp_w = ctx.w; pre_comp_h = ctx.h;
                        snprintf(last_effect, sizeof(last_effect),
                                 "Comp mode ON  E=expand  S=shrink  +/-=all sides  (T after exit)");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_c &&
                               (e.key.keysym.mod & KMOD_CTRL) &&
                               (e.key.keysym.mod & KMOD_SHIFT)) {
                        composite(dst.x, dst.y, draw_w, draw_h, win_w, win_h, mirror_mode);
                        mirror_mode = 0;
                        snprintf(last_effect, sizeof(last_effect), "Composite");
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_s) {
                        pending_save = 1;
                        const char *fname = strrchr(current_path, '\\');
                        if (!fname) fname = strrchr(current_path, '/');
                        fname = fname ? fname + 1 : current_path;
                        snprintf(last_effect, sizeof(last_effect),
                                 "Overwrite \"%s\"? Y / N", fname);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_e) {
                        char export_path[MAX_PATH];
                        strncpy(export_path, current_path, sizeof(export_path) - 1);
                        if (save_file_dialog(export_path, sizeof(export_path))) {
                            const char *ext = strrchr(export_path, '.');
                            int ok = 0;
                            if (ext && (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0))
                                ok = stbi_write_jpg(export_path, ctx.w, ctx.h, 4, ctx.pixels, 92);
                            else if (ext && _stricmp(ext, ".bmp") == 0)
                                ok = stbi_write_bmp(export_path, ctx.w, ctx.h, 4, ctx.pixels);
                            else
                                ok = stbi_write_png(export_path, ctx.w, ctx.h, 4, ctx.pixels, ctx.w * 4);
                            snprintf(last_effect, sizeof(last_effect),
                                     ok ? "Exported!" : "Export failed");
                        } else {
                            snprintf(last_effect, sizeof(last_effect), "Export cancelled");
                        }
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_t && !(e.key.keysym.mod & KMOD_SHIFT)) {
                        /* T: toggle transform mode (uses pre-comp original) */
                        if (pre_comp_pixels) {
                            transform_mode = !transform_mode;
                            snprintf(last_effect, sizeof(last_effect),
                                     transform_mode
                                     ? "Transform ON  1-6 = fit original into canvas"
                                     : "Transform OFF");
                        } else {
                            snprintf(last_effect, sizeof(last_effect),
                                     "Transform: enter Comp mode (C) first to save a reference");
                        }
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (transform_mode && pre_comp_pixels &&
                               !(e.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT)) &&
                               e.key.keysym.scancode >= SDL_SCANCODE_1 &&
                               e.key.keysym.scancode <= SDL_SCANCODE_6) {
                        /* 1-6 in transform mode: fit original into current canvas */
                        static const char *fnames[] = {"","Fill","Fit","Stretch","Tile","Center","Span"};
                        canvas_fill_mode = (int)(e.key.keysym.scancode - SDL_SCANCODE_1) + 1;
                        transform_fill(pre_comp_pixels, pre_comp_w, pre_comp_h, canvas_fill_mode);
                        snprintf(last_effect, sizeof(last_effect),
                                 "Transform %s  %dx%d", fnames[canvas_fill_mode], ctx.w, ctx.h);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_t && (e.key.keysym.mod & KMOD_SHIFT)) {
                        bg_r = rand() % 256;
                        bg_g = rand() % 256;
                        bg_b = rand() % 256;
                        snprintf(last_effect, sizeof(last_effect),
                                 "BG #%02X%02X%02X", bg_r, bg_g, bg_b);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        if (input_len > 0) {
                            char *sep = strchr(input, ':');
                            if (sep && sep != input && *(sep + 1) != '\0') {
                                int rw = atoi(input);
                                int rh = atoi(sep + 1);
                                if (rw >= 1 && rh >= 1) {
                                    const char *name = crop_aspect(rw, rh);
                                    if (name) snprintf(last_effect, sizeof(last_effect), "%s %d:%d", name, rw, rh);
                                    else      snprintf(last_effect, sizeof(last_effect), "???");
                                }
                            } else {
                                int n = atoi(input);
                                if (n >= 1 && n <= 1000) {
                                    if (popup_has_template(n)) {
                                        popup_open(n, win, win_w, win_h);
                                        snprintf(last_effect, sizeof(last_effect),
                                                 "%s...", popup_cur.name);
                                    } else {
                                        const char *name = on_number_confirmed(n);
                                        if (name) snprintf(last_effect, sizeof(last_effect), "%s", name);
                                        else      snprintf(last_effect, sizeof(last_effect), "???");
                                    }
                                }
                            }
                            input_confirmed = 1;
                        }
                    }
                }
                } /* end else (popup_active) */
            }

            if (e.type == SDL_TEXTINPUT) {
                char c = e.text.text[0];
                /* Search input: accept all printable chars (skip the opening space) */
                if (search_active) {
                    if (c >= 32 && c < 127 && !(search_len == 0 && c == ' ')) {
                        if (search_len < 63) {
                            search_input[search_len++] = c;
                            search_input[search_len]   = '\0';
                            search_filter_fn(search_input, search_results, &search_count);
                            search_sel = 0;
                        }
                    }
                } else {
                int is_digit = (c >= '0' && c <= '9');
                int is_colon = (c == ':' && input_len > 0 && !strchr(input, ':'));
                int is_x     = (c == 'x' && res_mode && input_len > 0
                                && !strchr(input,'x') && !strchr(input,':'));
                if (is_digit || is_colon || is_x) {
                    if (input_confirmed) {
                        input_len       = 0;
                        input[0]        = '\0';
                        input_confirmed = 0;
                    }
                    if (input_len < 14) {
                        input[input_len++] = c;
                        input[input_len]   = '\0';
                    }
                }
                } /* end else (!search_active) */
            }
        }

        if (ctx.needs_layout_update) {
            ctx.needs_layout_update = 0;
            zoom_level = 1.0f;
            {
                int max_i = MAX_SIZE - 2 * BORDER;
                float sx = (float)max_i / ctx.w;
                float sy = (float)max_i / ctx.h;
                float sc = sx < sy ? sx : sy;
                if (sc > 1.0f) sc = 1.0f;
                base_draw_w = (int)(ctx.w * sc);
                base_draw_h = (int)(ctx.h * sc);
                win_w = base_draw_w + 2 * BORDER;
                win_h = base_draw_h + 2 * BORDER;
                SDL_SetWindowSize(ctx.win, win_w, win_h);
                SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
            draw_w = base_draw_w;
            draw_h = base_draw_h;
            dst.x  = BORDER;
            dst.y  = BORDER;
            dst.w  = draw_w;
            dst.h  = draw_h;
        }

        if (comp_dirty) {
            comp_dirty = 0;
            int eff_w = comp_w, eff_h = comp_h;
            zoom_level = 1.0f;
            {
                int max_i = MAX_SIZE - 2 * BORDER;
                float sx = (float)max_i / eff_w;
                float sy = (float)max_i / eff_h;
                float sc = sx < sy ? sx : sy;
                if (sc > 1.0f) sc = 1.0f;
                base_draw_w = (int)(eff_w * sc);
                base_draw_h = (int)(eff_h * sc);
                win_w = base_draw_w + 2 * BORDER;
                win_h = base_draw_h + 2 * BORDER;
                SDL_SetWindowSize(ctx.win, win_w, win_h);
                SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
            draw_w = base_draw_w;
            draw_h = base_draw_h;
            dst.x  = BORDER;
            dst.y  = BORDER;
            dst.w  = draw_w;
            dst.h  = draw_h;
        }

        SDL_SetRenderDrawColor(ctx.ren, bg_r, bg_g, bg_b, 255);
        SDL_RenderClear(ctx.ren);
        if (fullscreen) {
            int aw, ah;
            SDL_GetWindowSize(ctx.win, &aw, &ah);
            int vp_w = popup_active ? win_w + POPUP_W : win_w;
            SDL_Rect vp = { (aw - vp_w) / 2, (ah - win_h) / 2, vp_w, win_h };
            SDL_RenderSetViewport(ctx.ren, &vp);
        } else {
            SDL_RenderSetViewport(ctx.ren, NULL);
        }
        SDL_Rect safe = { BORDER, BORDER, win_w - 2 * BORDER, win_h - 2 * BORDER };
        SDL_RenderSetClipRect(ctx.ren, &safe);
        /* Use downscaled preview tex during popup (stretched to full dst rect by SDL) */
        SDL_Texture *display_tex = (popup_active && popup_preview_tex)
                                   ? popup_preview_tex : ctx.tex;
        /* Comp frame: show only comp_w x comp_h region (centered in image) */
        SDL_Rect *p_src = NULL;
        SDL_Rect src_rect;
        if (!popup_active && (comp_w < ctx.w || comp_h < ctx.h)) {
            src_rect.x = (ctx.w - comp_w) / 2;
            src_rect.y = (ctx.h - comp_h) / 2;
            src_rect.w = comp_w;
            src_rect.h = comp_h;
            p_src = &src_rect;
        }
        if (mirror_mode && draw_w > 0 && draw_h > 0) {
            int i_min = (BORDER - dst.x) / draw_w - 1;
            int i_max = (win_w - BORDER - dst.x) / draw_w + 1;
            int j_min = (BORDER - dst.y) / draw_h - 1;
            int j_max = (win_h - BORDER - dst.y) / draw_h + 1;
            for (int j = j_min; j <= j_max; j++) {
                for (int i = i_min; i <= i_max; i++) {
                    SDL_Rect tile = {
                        dst.x + i * draw_w,
                        dst.y + j * draw_h,
                        draw_w, draw_h
                    };
                    int fh = (i < 0 ? -i : i) % 2;
                    int fv = (j < 0 ? -j : j) % 2;
                    int flip = 0;
                    if (fh) flip |= SDL_FLIP_HORIZONTAL;
                    if (fv) flip |= SDL_FLIP_VERTICAL;
                    SDL_RenderCopyEx(ctx.ren, display_tex, p_src, &tile,
                                     0.0, NULL, (SDL_RendererFlip)flip);
                }
            }
        } else {
            SDL_RenderCopy(ctx.ren, display_tex, p_src, &dst);
        }
        SDL_RenderSetClipRect(ctx.ren, NULL);
        if (mirror_mode) {
            draw_text(ctx.ren, BORDER + 3, BORDER + 3, "MIRROR MODE ACTIVE", 255, 80, 80);
        }
        if (transform_mode) {
            static const char *fnames[] = {"","FILL","FIT","STRETCH","TILE","CENTER","SPAN"};
            char tlabel[48];
            snprintf(tlabel, sizeof(tlabel), "TRANSFORM [%s]  orig %dx%d",
                     fnames[canvas_fill_mode], pre_comp_w, pre_comp_h);
            int tw = text_width(tlabel);
            draw_text(ctx.ren, win_w - BORDER - tw, BORDER + 16, tlabel, 255, 200, 80);
        }
        if (res_mode) {
            char reslabel[64];
            if (comp_w != ctx.w || comp_h != ctx.h)
                snprintf(reslabel, sizeof(reslabel), "COMP %dx%d  img %dx%d", comp_w, comp_h, ctx.w, ctx.h);
            else
                snprintf(reslabel, sizeof(reslabel), "COMP %dx%d", comp_w, comp_h);
            draw_text(ctx.ren, BORDER + 3, BORDER + 3, reslabel, 100, 200, 255);
        }

        draw_text(ctx.ren, BORDER, (BORDER - 13) / 2.0f, argv[1], 255, 255, 255);


        /* Große zentrierte Zahl über dem Bild während der Eingabe */
        if (input_len > 0 && !popup_active && !res_mode) {
            int tw = 0, th = 0;
            if (font_xl) TTF_SizeText(font_xl, input, &tw, &th);
            float cx = BORDER + (win_w - 2 * BORDER) / 2.0f - tw / 2.0f;
            float cy = BORDER + (win_h - 2 * BORDER) * 0.65f - th / 2.0f;
            draw_text_scaled(ctx.ren, cx, cy, input, 15.0f, 255, 220, 0); /* 15 > 8 → font_xl */
        }

        /* Untere Konsole: Effektname nach Enter, sonst Eingabe-Prompt */
        char display[140];
        Uint8 cr, cg, cb;
        if (input_confirmed && last_effect[0]) {
            snprintf(display, sizeof(display), "> %s", last_effect);
            cr = 100; cg = 220; cb = 100;
        } else {
            snprintf(display, sizeof(display), "> %s_", input);
            cr = input_len ? 255 : 120;
            cg = input_len ? 220 : 120;
            cb = input_len ?   0 : 120;
        }
        {
            bot_y = dst.y + draw_h + (BORDER - 13) / 2.0f;
            draw_text(ctx.ren, BORDER, bot_y, display, cr, cg, cb);
        }

        /* ── Help / Doc Modus ──────────────────────────────────────────── */
        if (help_mode) {
            SDL_SetRenderDrawColor(ctx.ren, 18, 18, 18, 255);
            SDL_Rect overlay = { 0, 0, win_w, win_h };
            SDL_RenderFillRect(ctx.ren, &overlay);

            int fh       = font_active ? TTF_FontHeight(font_active) : 16;
            int line_h   = fh + 3;
            /* centred text column — leave 12% margin on each side */
            int x_margin = win_w / 8;
            int y_start  = BORDER;

            /* Before/After preview images (only for effect docs) */
            if (help_tex_before && help_tex_after) {
                int gap     = 16;
                int total_w = help_preview_w * 2 + gap;
                int px      = (win_w - total_w) / 2;
                int py      = BORDER;
                SDL_Rect rbefore = { px,                           py, help_preview_w, help_preview_h };
                SDL_Rect rafter  = { px + help_preview_w + gap,   py, help_preview_w, help_preview_h };
                SDL_RenderCopy(ctx.ren, help_tex_before, NULL, &rbefore);
                SDL_RenderCopy(ctx.ren, help_tex_after,  NULL, &rafter);
                int lbl_y = py + help_preview_h + 4;
                draw_text(ctx.ren, px,                         lbl_y, "Before", 140, 140, 140);
                draw_text(ctx.ren, px + help_preview_w + gap, lbl_y, "After",  120, 220, 120);
                y_start = lbl_y + line_h + 4;
            }

            int lines_vis = (win_h - y_start - BORDER - line_h) / line_h;

            for (int i = 0; i < lines_vis; i++) {
                int idx = help_scroll + i;
                if (idx >= help_line_count) break;
                const char *line = help_lines[idx];
                int y = y_start + i * line_h;
                if (line[0] == '#' && line[1] == '#') {
                    draw_text(ctx.ren, x_margin, y, line + 2, 120, 200, 255);
                } else if (line[0] == '#') {
                    draw_text_scaled(ctx.ren, x_margin, y, line + 1, 1.3f, 255, 220, 80);
                } else if (line[0] == '-' || line[0] == '*') {
                    draw_text(ctx.ren, x_margin + 12, y, line, 180, 180, 180);
                } else if (line[0] == '\0') {
                    /* empty line — skip */
                } else {
                    draw_text(ctx.ren, x_margin, y, line, 210, 210, 210);
                }
            }

            /* Scrollbar-Indikator + Hinweis */
            char hint[48];
            snprintf(hint, sizeof(hint), "[%d/%d] UP/DOWN  H/ESC = close",
                     help_scroll + 1, help_line_count);
            draw_text(ctx.ren, x_margin, win_h - BORDER / 2 - fh / 2, hint, 80, 80, 80);
        }

        if (search_active)
            search_render(ctx.ren, win_w, win_h,
                          search_input, search_sel, search_results, search_count,
                          ui_scale);
        popup_render(ctx.ren, win_h);
        SDL_RenderPresent(ctx.ren);
    }

    /* cleanup search if still open */
    if (search_active) SDL_StopTextInput();
    /* cleanup popup if still open */
    if (popup_saved_px) { free(popup_saved_px); popup_saved_px = NULL; }
    popup_free_preview();
    ctx.preview_mode = 0;
    stbi_image_free(ctx.pixels);
    if (ctx.crop_src) free(ctx.crop_src);
    free(ctx.original_pixels);
    for (int i = 0; i < UNDO_HISTORY; i++)
        free(ctx.undo_stack[i]);
    SDL_DestroyTexture(ctx.tex);
    SDL_DestroyRenderer(ctx.ren);
    SDL_DestroyWindow(win);
    if (font_sm) TTF_CloseFont(font_sm);
    if (font_lg) TTF_CloseFont(font_lg);
    if (font_xl) TTF_CloseFont(font_xl);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

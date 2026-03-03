#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"
#define STB_EASY_FONT_IMPLEMENTATION
#include "vendor/stb_easy_font.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#include "effects.h"
#include "license.h"
#include "render.h"
#include "vendor/tinyfiledialogs.h"

#define BOGOSHOP_VERSION "0.2.0_PRE_RELEASE"

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
    static const char *filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp"};
    const char *result = tinyfd_saveFileDialog("Export als...", out, 4, filters, "Bilder");
    if (!result) return 0;
    strncpy(out, result, out_size - 1);
    out[out_size - 1] = '\0';
    return 1;
}

/* Render text normal */
void draw_text(SDL_Renderer *ren, float x, float y, const char *text,
               Uint8 r, Uint8 g, Uint8 b) {
    char tbuf[65536];
    int quads = stb_easy_font_print(x, y, (char *)text, NULL, tbuf, sizeof(tbuf));
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    for (int i = 0; i < quads; i++) {
        float *v = (float *)(tbuf + i * 64);
        SDL_Rect rect = {
            (int)v[0],  (int)v[1],
            (int)(v[4] - v[0]),
            (int)(v[9] - v[5])
        };
        SDL_RenderFillRect(ren, &rect);
    }
}

/* Render text skaliert – rendert erst bei Ursprung, dann skaliert + versetzt */
void draw_text_scaled(SDL_Renderer *ren, float x, float y, const char *text,
                      float scale, Uint8 r, Uint8 g, Uint8 b) {
    char tbuf[65536];
    int quads = stb_easy_font_print(0, 0, (char *)text, NULL, tbuf, sizeof(tbuf));
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    for (int i = 0; i < quads; i++) {
        float *v = (float *)(tbuf + i * 64);
        SDL_Rect rect = {
            (int)(x + v[0] * scale),
            (int)(y + v[1] * scale),
            (int)((v[4] - v[0]) * scale),
            (int)((v[9] - v[5]) * scale)
        };
        SDL_RenderFillRect(ren, &rect);
    }
}

int text_width(const char *text) {
    return stb_easy_font_width((char *)text);
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

int main(int argc, char *argv[]) {
    char dialog_path[MAX_PATH] = {0};
    printf("Bogoshop v%s launched\n", BOGOSHOP_VERSION); fflush(stdout);

    /* ── Lizenz prüfen ── */
    if (!load_license()) {
        /* Temporäres Fenster für den Lizenz-Screen */
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
        SDL_Window   *lwin = SDL_CreateWindow("Bogoshop",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                480, 320, 0);
        apply_window_icon(lwin);
        force_foreground(lwin);
        SDL_Renderer *lren = SDL_CreateRenderer(lwin, -1, SDL_RENDERER_ACCELERATED);
        int ok = show_license_screen(lwin, lren);
        SDL_DestroyRenderer(lren);
        SDL_DestroyWindow(lwin);
        SDL_Quit();
        if (!ok) return 0;
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

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL Init failed: %s\n", SDL_GetError());
        stbi_image_free(ctx.pixels);
        return 1;
    }

    float zoom_level  = 1.0f;
    int   mirror_mode = 0;
    int   fullscreen  = 0;
    int   pending_save = 0;
    int   pending_quit = 0;
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

    char win_title[MAX_PATH + 64];
    snprintf(win_title, sizeof(win_title), "Bogoshop v" BOGOSHOP_VERSION);
    SDL_Window *win = SDL_CreateWindow(
        win_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, 0
    );
    apply_window_icon(win);
    force_foreground(win);
    ctx.ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    ctx.win = win;
    ctx.tex = SDL_CreateTexture(
        ctx.ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, ctx.w, ctx.h
    );
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);

    SDL_Rect dst   = { BORDER, BORDER, draw_w, draw_h };
    float    bot_y = BORDER + draw_h + (BORDER - 13) / 2.0f;

    char input[8] = {0};
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

    SDL_Event e;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

#define HELP_WIN_W 780
#define HELP_WIN_H 680
                if (k == SDLK_h) {
                    if (help_mode) {
                        help_mode = 0;
                        win_w = help_saved_win_w;
                        win_h = help_saved_win_h;
                        SDL_SetWindowSize(ctx.win, win_w, win_h);
                        SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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
                            help_saved_win_w = win_w;
                            help_saved_win_h = win_h;
                            win_w = HELP_WIN_W;
                            win_h = HELP_WIN_H;
                            SDL_SetWindowSize(ctx.win, win_w, win_h);
                            SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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
                        win_w = help_saved_win_w;
                        win_h = help_saved_win_h;
                        SDL_SetWindowSize(ctx.win, win_w, win_h);
                        SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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
                    draw_w = base_draw_w;
                    draw_h = base_draw_h;
                    dst.x  = BORDER;
                    dst.y  = BORDER;
                    dst.w  = draw_w;
                    dst.h  = draw_h;
                    reset_image();
                } else if (k == SDLK_u) {
                    undo_last();
                } else if (k == SDLK_BACKSPACE && input_len > 0) {
                    input[--input_len] = '\0';
                    input_confirmed    = 0;
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
                } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                    if (input_len > 0) {
                        char *sep = strchr(input, ':');
                        if (sep && sep != input && *(sep + 1) != '\0') {
                            /* Ratio-Format: z.B. "4:3" → crop */
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
                                const char *name = on_number_confirmed(n);
                                if (name) snprintf(last_effect, sizeof(last_effect), "%s", name);
                                else      snprintf(last_effect, sizeof(last_effect), "???");
                            }
                        }
                        input_confirmed = 1;   /* Wert bleibt stehen */
                    }
                } else if (help_mode && (k == SDLK_UP || k == SDLK_DOWN)) {
                    if (k == SDLK_UP   && help_scroll > 0) help_scroll--;
                    if (k == SDLK_DOWN && help_scroll < help_line_count - 1) help_scroll++;
                } else if (k == SDLK_UP || k == SDLK_DOWN ||
                           k == SDLK_LEFT || k == SDLK_RIGHT) {
                    if (mirror_mode || zoom_level > 1.0f) {
                        /* Viewport pan: dst verschieben (Mirror-Mode oder Zoom > 1) */
                        int step = mirror_mode && zoom_level <= 1.0f
                                   ? (draw_w > 4 ? draw_w / 4 : 1)
                                   : (win_w - 2 * BORDER) / 8;
                        if (k == SDLK_LEFT)  dst.x += step;
                        if (k == SDLK_RIGHT) dst.x -= step;
                        if (k == SDLK_UP)    dst.y += step;
                        if (k == SDLK_DOWN)  dst.y -= step;
                        /* Klemmen nur wenn kein Mirror-Mode (Kacheln füllen immer die Lücken) */
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
                        /* Crop pan: Ausschnitt im Quellbild verschieben */
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
                    } }
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
                    } }
                    snprintf(last_effect, sizeof(last_effect), "Zoom %.2fx", zoom_level);
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (k == SDLK_m) {
                    mirror_mode = !mirror_mode;
                    snprintf(last_effect, sizeof(last_effect),
                             mirror_mode ? "Mirror ON" : "Mirror OFF");
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (k == SDLK_c) {
                    composite(dst.x, dst.y, draw_w, draw_h, win_w, win_h, mirror_mode);
                    mirror_mode = 0;
                    snprintf(last_effect, sizeof(last_effect), "Composite");
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (k == SDLK_t) {
                    bg_r = rand() % 256;
                    bg_g = rand() % 256;
                    bg_b = rand() % 256;
                    snprintf(last_effect, sizeof(last_effect),
                             "BG #%02X%02X%02X", bg_r, bg_g, bg_b);
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (k == SDLK_s) {
                    if (pending_quit) {
                        /* Direkt speichern und beenden */
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
                    } else {
                        /* Normale Speicher-Bestätigung */
                        pending_save = 1;
                        const char *fname = strrchr(current_path, '\\');
                        if (!fname) fname = strrchr(current_path, '/');
                        fname = fname ? fname + 1 : current_path;
                        snprintf(last_effect, sizeof(last_effect),
                                 "Overwrite \"%s\"? Y / N", fname);
                        input_confirmed = 1; input_len = 0; input[0] = '\0';
                    }
                } else if (k == SDLK_e) {
                    /* Export: nativer Speicherdialog via tinyfiledialogs */
                    int was_quit = pending_quit;
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
                        if (was_quit) running = 0;
                    } else {
                        snprintf(last_effect, sizeof(last_effect),
                                 was_quit ? "Quit cancelled" : "Export cancelled");
                    }
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                } else if (k == SDLK_f) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(ctx.win,
                        fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    if (!fullscreen)
                        SDL_SetWindowPosition(ctx.win,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    snprintf(last_effect, sizeof(last_effect),
                             fullscreen ? "Fullscreen ON" : "Fullscreen OFF");
                    input_confirmed = 1; input_len = 0; input[0] = '\0';
                }
            }

            if (e.type == SDL_TEXTINPUT) {
                char c = e.text.text[0];
                int is_digit = (c >= '0' && c <= '9');
                int is_colon = (c == ':' && input_len > 0 && !strchr(input, ':'));
                if (is_digit || is_colon) {
                    if (input_confirmed) {
                        input_len       = 0;
                        input[0]        = '\0';
                        input_confirmed = 0;
                    }
                    if (input_len < 6) {
                        input[input_len++] = c;
                        input[input_len]   = '\0';
                    }
                }
            }
        }

        if (ctx.needs_layout_update) {
            ctx.needs_layout_update = 0;
            zoom_level = 1.0f;
            int max_i = MAX_SIZE - 2 * BORDER;
            float sx = (float)max_i / ctx.w;
            float sy = (float)max_i / ctx.h;
            float sc = sx < sy ? sx : sy;
            base_draw_w = (int)(ctx.w * sc);
            base_draw_h = (int)(ctx.h * sc);
            draw_w = base_draw_w;
            draw_h = base_draw_h;
            win_w  = draw_w + 2 * BORDER;
            win_h  = draw_h + 2 * BORDER;
            dst.x  = BORDER;
            dst.y  = BORDER;
            dst.w  = draw_w;
            dst.h  = draw_h;
            bot_y  = BORDER + draw_h + (BORDER - 13) / 2.0f;
            SDL_SetWindowSize(ctx.win, win_w, win_h);
            SDL_SetWindowPosition(ctx.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }

        SDL_SetRenderDrawColor(ctx.ren, bg_r, bg_g, bg_b, 255);
        SDL_RenderClear(ctx.ren);
        if (fullscreen) {
            int aw, ah;
            SDL_GetWindowSize(ctx.win, &aw, &ah);
            SDL_Rect vp = { (aw - win_w) / 2, (ah - win_h) / 2, win_w, win_h };
            SDL_RenderSetViewport(ctx.ren, &vp);
        } else {
            SDL_RenderSetViewport(ctx.ren, NULL);
        }
        SDL_Rect safe = { BORDER, BORDER, win_w - 2 * BORDER, win_h - 2 * BORDER };
        SDL_RenderSetClipRect(ctx.ren, &safe);
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
                    SDL_RenderCopyEx(ctx.ren, ctx.tex, NULL, &tile,
                                     0.0, NULL, (SDL_RendererFlip)flip);
                }
            }
        } else {
            SDL_RenderCopy(ctx.ren, ctx.tex, NULL, &dst);
        }
        SDL_RenderSetClipRect(ctx.ren, NULL);
        if (mirror_mode) {
            draw_text(ctx.ren, BORDER + 3, BORDER + 3, "MIRROR MODE ACTIVE", 255, 80, 80);
        }

        draw_text(ctx.ren, BORDER, (BORDER - 13) / 2.0f, argv[1], 255, 255, 255);


        /* Große zentrierte Zahl über dem Bild während der Eingabe */
        if (input_len > 0) {
            float big_scale = 15.0f;
            float text_w = text_width(input) * big_scale;
            float text_h = 13.0f * big_scale;
            float cx = BORDER + (win_w - 2 * BORDER) / 2.0f - text_w / 2.0f;
            float cy = BORDER + (win_h - 2 * BORDER) * 0.65f - text_h / 2.0f;
            draw_text_scaled(ctx.ren, cx, cy, input, big_scale, 255, 220, 0);
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
            int avail = win_w - 2 * BORDER;
            int tw    = text_width(display);
            float sc  = tw > 0 && tw > avail ? (float)avail / tw : 1.0f;
            draw_text_scaled(ctx.ren, BORDER, bot_y, display, sc, cr, cg, cb);
        }

        /* ── Help / Doc Modus ──────────────────────────────────────────── */
        if (help_mode) {
            SDL_SetRenderDrawColor(ctx.ren, 18, 18, 18, 255);
            SDL_Rect overlay = { 0, 0, win_w, win_h };
            SDL_RenderFillRect(ctx.ren, &overlay);

            int y_start = BORDER;

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
                int lbl_y = py + help_preview_h + 3;
                draw_text(ctx.ren, px,                         lbl_y, "Before", 140, 140, 140);
                draw_text(ctx.ren, px + help_preview_w + gap, lbl_y, "After",  120, 220, 120);
                y_start = lbl_y + 14;
            }

            int line_h    = 14;
            int x_margin  = BORDER;
            int lines_vis = (win_h - y_start - BORDER - 20) / line_h;

            for (int i = 0; i < lines_vis; i++) {
                int idx = help_scroll + i;
                if (idx >= help_line_count) break;
                const char *line = help_lines[idx];
                float y = y_start + i * line_h;
                if (line[0] == '#' && line[1] == '#') {
                    draw_text(ctx.ren, x_margin, y, line + 2, 120, 200, 255);
                } else if (line[0] == '#') {
                    draw_text_scaled(ctx.ren, x_margin, y, line + 1, 1.3f, 255, 220, 80);
                } else if (line[0] == '-' || line[0] == '*') {
                    draw_text(ctx.ren, x_margin + 8, y, line, 180, 180, 180);
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
            draw_text(ctx.ren, x_margin, win_h - BORDER / 2 - 6, hint, 80, 80, 80);
        }

        SDL_RenderPresent(ctx.ren);
    }

    stbi_image_free(ctx.pixels);
    if (ctx.crop_src) free(ctx.crop_src);
    free(ctx.original_pixels);
    for (int i = 0; i < UNDO_HISTORY; i++)
        free(ctx.undo_stack[i]);
    SDL_DestroyTexture(ctx.tex);
    SDL_DestroyRenderer(ctx.ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

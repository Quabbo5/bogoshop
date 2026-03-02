#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#define STB_EASY_FONT_IMPLEMENTATION
#include "vendor/stb_easy_font.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "effects.h"
#include "license.h"
#include "render.h"

#ifdef _WIN32
static int open_file_dialog(char *out, int out_size) {
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize   = sizeof(ofn);
    ofn.lpstrFilter   = "Bilder\0*.png;*.jpg;*.jpeg;*.bmp\0Alle Dateien\0*.*\0";
    ofn.lpstrFile     = out;
    ofn.nMaxFile      = out_size;
    ofn.Flags         = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle    = "Open File";
    out[0]            = '\0';
    return GetOpenFileNameA(&ofn);
}
#endif

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

int main(int argc, char *argv[]) {
#ifdef _WIN32
    char dialog_path[MAX_PATH] = {0};
#endif

    /* ── Lizenz prüfen ── */
    if (!load_license()) {
        /* Temporäres Fenster für den Lizenz-Screen */
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
        SDL_Window   *lwin = SDL_CreateWindow("Bogoshop",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                480, 320, 0);
        apply_window_icon(lwin);
        SDL_Renderer *lren = SDL_CreateRenderer(lwin, -1, SDL_RENDERER_ACCELERATED);
        int ok = show_license_screen(lwin, lren);
        SDL_DestroyRenderer(lren);
        SDL_DestroyWindow(lwin);
        SDL_Quit();
        if (!ok) return 0;
    }

    if (argc < 2) {
#ifdef _WIN32
        if (!open_file_dialog(dialog_path, sizeof(dialog_path)))
            return 0;
        argv[1] = dialog_path;
#else
        fprintf(stderr, "Usage: %s <bilddatei>\n", argv[0]);
        return 1;
#endif
    }

    int channels;
    ctx.pixels = stbi_load(argv[1], &ctx.w, &ctx.h, &channels, 4);
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
        fprintf(stderr, "SDL Init fehlgeschlagen: %s\n", SDL_GetError());
        stbi_image_free(ctx.pixels);
        return 1;
    }

    float zoom_level  = 1.0f;
    int   mirror_mode = 0;
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

    SDL_Window *win = SDL_CreateWindow(
        argv[1], SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, 0
    );
    apply_window_icon(win);
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
    char last_effect[32] = {0}; /* Name des zuletzt angewendeten Effekts */

    SDL_Event e;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE) {
                    running = 0;
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
            float sc  = 1.0f;
            if (ctx.w > max_i || ctx.h > max_i) {
                float sx = (float)max_i / ctx.w;
                float sy = (float)max_i / ctx.h;
                sc = sx < sy ? sx : sy;
            }
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
        }

        SDL_SetRenderDrawColor(ctx.ren, 0, 0, 0, 255);
        SDL_RenderClear(ctx.ren);
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
            draw_text(ctx.ren, BORDER + 3, BORDER + 3, "MIRROR", 255, 80, 80);
        }

        draw_text(ctx.ren, BORDER, (BORDER - 13) / 2.0f, argv[1], 255, 255, 255);

        /* Programmname oben rechts */
        char *prog_name = "Bogoshop";
        float prog_x = win_w - BORDER - text_width(prog_name);
        draw_text(ctx.ren, prog_x, (BORDER - 13) / 2.0f, prog_name, 255, 255, 255);

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
        char display[48];
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
        draw_text(ctx.ren, BORDER, bot_y, display, cr, cg, cb);

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

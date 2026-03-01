#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_SIZE      800
#define BORDER         40
#define UNDO_HISTORY    5   /* Anzahl der Undo-Schritte */

/* ── Globaler Kontext (wird von main befüllt) ────────────────────────── */
static struct {
    unsigned char *pixels;                        /* aktuelle Pixeldaten (RGBA)      */
    unsigned char *original_pixels;               /* unverändertes Original → Reset  */
    unsigned char *undo_stack[UNDO_HISTORY];      /* Ring-Puffer für Undo-Schritte   */
    int            undo_head;                     /* nächster Schreibindex im Stack  */
    int            undo_count;                    /* gespeicherte Schritte (0–5)     */
    int            w, h;
    SDL_Texture   *tex;
    SDL_Renderer  *ren;
} ctx;

/* ── Aktuellen Zustand als Undo-Snapshot speichern ───────────────────── */
static void save_undo_state(void) {
    int total_bytes = ctx.w * ctx.h * 4;
    memcpy(ctx.undo_stack[ctx.undo_head], ctx.pixels, total_bytes);
    ctx.undo_head = (ctx.undo_head + 1) % UNDO_HISTORY;
    if (ctx.undo_count < UNDO_HISTORY) ctx.undo_count++;
}

/* ── R: Bild auf Original zurücksetzen ───────────────────────────────── */
static void reset_image(void) {
    int total_bytes = ctx.w * ctx.h * 4;
    memcpy(ctx.pixels, ctx.original_pixels, total_bytes);
    ctx.undo_head  = 0;
    ctx.undo_count = 0;
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Reset\n"); fflush(stdout);
}

/* ── U: Letzten Schritt rückgängig machen ────────────────────────────── */
static void undo_last(void) {
    if (ctx.undo_count == 0) {
        printf("Kein Undo mehr verfügbar\n"); fflush(stdout);
        return;
    }
    int total_bytes = ctx.w * ctx.h * 4;
    ctx.undo_head = (ctx.undo_head - 1 + UNDO_HISTORY) % UNDO_HISTORY;
    ctx.undo_count--;
    memcpy(ctx.pixels, ctx.undo_stack[ctx.undo_head], total_bytes);
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
    printf("Undo (%d/%d verbleibend)\n", ctx.undo_count, UNDO_HISTORY); fflush(stdout);
}

/* ── Bild um 'amount' aufhellen (0–255) und Textur aktualisieren ─────── */
static void brighten(int amount) {

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
static void darken(int amount) {
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
static void iceing(int amount) {
    save_undo_state();
    int total_bytes = ctx.w * ctx.h * 4;
    for (int byte_index = 0; byte_index < total_bytes; byte_index++) {
        if (byte_index % 4 == 3) continue;
        int neuer_wert = ctx.pixels[byte_index] - amount;
        ctx.pixels[byte_index] = neuer_wert > 255 ? 255 : (unsigned char)neuer_wert;
    }
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
}


/* catching signals */
static void on_number_confirmed(int n) {
    printf("Zahl bestätigt: %d\n", n);
    fflush(stdout);
    if (n == 1) {
        printf("Zahl 1 True\n");
        /*fflush(stdout);*/
        brighten(30);
    }
    if (n == 2) {
        printf("Zahl 2 True\n");
        darken(30);
    }
    if (n == 3) {
        printf("Zahl 3 True\n");
        iceing(30);
    }
    else{
        printf("No function found dumbass hahahhaha\n");
    }
}

/* Render top texts */
static void draw_text(SDL_Renderer *ren, float x, float y, const char *text,
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Verwendung: %s <bilddatei>\n", argv[0]);
        return 1;
    }

    int channels;
    ctx.pixels = stbi_load(argv[1], &ctx.w, &ctx.h, &channels, 4);
    if (!ctx.pixels) {
        fprintf(stderr, "Bild konnte nicht geladen werden: %s\n", stbi_failure_reason());
        return 1;
    }
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

    int max_img = MAX_SIZE - 2 * BORDER;
    float scale = 1.0f;
    if (ctx.w > max_img || ctx.h > max_img) {
        float sx = (float)max_img / ctx.w;
        float sy = (float)max_img / ctx.h;
        scale = sx < sy ? sx : sy;
    }
    int draw_w = (int)(ctx.w * scale);
    int draw_h = (int)(ctx.h * scale);

    int win_w = draw_w + 2 * BORDER;
    int win_h = draw_h + 2 * BORDER;

    SDL_Window *win = SDL_CreateWindow(
        argv[1], SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, 0
    );
    ctx.ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    ctx.tex = SDL_CreateTexture(
        ctx.ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, ctx.w, ctx.h
    );
    SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);

    SDL_Rect dst   = { BORDER, BORDER, draw_w, draw_h };
    float    bot_y = BORDER + draw_h + (BORDER - 13) / 2.0f;

    char input[5] = {0};
    int  input_len       = 0;
    int  input_confirmed = 0;   /* 1 = Enter wurde gedrückt, Wert wird gehalten */

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
                    reset_image();
                } else if (k == SDLK_u) {
                    undo_last();
                } else if (k >= SDLK_0 && k <= SDLK_9) {
                    /* Neue Ziffer: wenn vorheriger Wert noch gehalten wird, zuerst löschen */
                    if (input_confirmed) {
                        input_len       = 0;
                        input[0]        = '\0';
                        input_confirmed = 0;
                    }
                    if (input_len < 4) {
                        input[input_len++] = '0' + (k - SDLK_0);
                        input[input_len]   = '\0';
                    }
                } else if (k == SDLK_BACKSPACE && input_len > 0) {
                    input[--input_len] = '\0';
                    input_confirmed    = 0;
                } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                    if (input_len > 0) {
                        int n = atoi(input);
                        if (n >= 1 && n <= 1000)
                            on_number_confirmed(n);
                        input_confirmed = 1;   /* Wert bleibt stehen */
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(ctx.ren, 0, 0, 0, 255);
        SDL_RenderClear(ctx.ren);
        SDL_RenderCopy(ctx.ren, ctx.tex, NULL, &dst);

        draw_text(ctx.ren, BORDER, (BORDER - 13) / 2.0f, argv[1], 255, 255, 255);

        /* Programmname oben rechts */
        char *prog_name = "Bogoshop";
        float prog_x = win_w - BORDER - stb_easy_font_width(prog_name);
        draw_text(ctx.ren, prog_x, (BORDER - 13) / 2.0f, prog_name, 255, 255, 255);

        char display[16];
        snprintf(display, sizeof(display), "> %s_", input);
        Uint8 cr = input_len ? 255 : 120;
        Uint8 cg = input_len ? 220 : 120;
        Uint8 cb = input_len ?   0 : 120;
        draw_text(ctx.ren, BORDER, bot_y, display, cr, cg, cb);

        SDL_RenderPresent(ctx.ren);
    }

    stbi_image_free(ctx.pixels);
    free(ctx.original_pixels);
    for (int i = 0; i < UNDO_HISTORY; i++)
        free(ctx.undo_stack[i]);
    SDL_DestroyTexture(ctx.tex);
    SDL_DestroyRenderer(ctx.ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

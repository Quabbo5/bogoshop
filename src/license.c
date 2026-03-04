#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>   /* Online-Verify via WinHTTP — link with -lwinhttp */
#else
#define MAX_PATH 4096
#include <sys/stat.h>
#endif

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "vendor/stb_easy_font.h"
#include "license.h"
#include "render.h"

/* ── Railway API ──────────────────────────────────────────────────────────
   TODO: Nach dem Railway-Deploy diese URL durch die echte ersetzen.
   Beispiel: L"bogoshop-api.up.railway.app"
   ──────────────────────────────────────────────────────────────────────── */
#define RAILWAY_HOST    L"bogoshop-production.up.railway.app"
#define RAILWAY_PATH    L"/verify"

/* Offline-Gnadenfrist: 7 Tage in Sekunden */
#define OFFLINE_GRACE_SEC (7 * 24 * 3600)

/* ══════════════════════════════════════════════════════════════════════════
   LICENSE / PRODUCT-KEY SYSTEM
   Format : XXXXX-XXXXX-XXXXX-XXXXX  (20 Zeichen, 4 Gruppen à 5)
   Alphabet: 24 Zeichen (keine verwechselbaren Zeichen wie 0/O, 1/I/L)
   Ersten 15 Zeichen = Seriennummer (base-24), letzte 5 = Prüfsumme
   ══════════════════════════════════════════════════════════════════════════ */
#define KEY_ALPHA      "ACDEFGHJKLMNPQRTVWXY3479"
#define KEY_ALPHA_LEN  24
#define KEY_SECRET1    0xA7F3C2D15B8E4A9CULL
#define KEY_SECRET2    0x3D7C1F94E2B6A850ULL

static int key_char_to_val(char c) {
    for (int i = 0; i < KEY_ALPHA_LEN; i++)
        if (KEY_ALPHA[i] == (char)toupper((unsigned char)c)) return i;
    return -1;
}

/* ── Machine-ID: Volume-Seriennummer von C:\ (stabil, eindeutig) ─────── */
#ifdef _WIN32
static void get_machine_id(char out[16]) {
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);
    snprintf(out, 16, "%08lX", (unsigned long)serial);
}
#else
static void get_machine_id(char out[16]) {
    /* Mac/Linux: einfacher Fallback, erweiterbar */
    strncpy(out, "UNKNOWN00000000", 16);
}
#endif

/* ── Online-Verification via Railway API ─────────────────────────────── */
/* Gibt  1 zurück wenn Server "valid":true meldet,
         0 wenn Server "valid":false meldet,
        -1 wenn kein Netz / Timeout (Offline-Fallback greift) */
#ifdef _WIN32
static int online_verify_key(const char *key) {
    char machine_id[16];
    get_machine_id(machine_id);
    printf("[license] machine-id: %s\n", machine_id);

    /* JSON-Body bauen */
    char body[128];
    snprintf(body, sizeof(body),
             "{\"key\":\"%s\",\"machine_id\":\"%s\"}", key, machine_id);
    int body_len = (int)strlen(body);

    int result = -1; /* Standard: Offline */

    printf("[license] connecting to bogoshop-production.up.railway.app ...\n");
    HINTERNET hSession = WinHttpOpen(
        L"bogoshop/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { printf("[license] WinHttpOpen failed\n"); return -1; }

    HINTERNET hConn = WinHttpConnect(hSession, RAILWAY_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { printf("[license] connect failed\n"); WinHttpCloseHandle(hSession); return -1; }

    HINTERNET hReq = WinHttpOpenRequest(
        hConn, L"POST", RAILWAY_PATH,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hReq) { printf("[license] open request failed\n"); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession); return -1; }

    /* Timeout: 5 Sekunden */
    DWORD timeout = 5000;
    WinHttpSetOption(hReq, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hReq, WINHTTP_OPTION_RECEIVE_TIMEOUT,  &timeout, sizeof(timeout));

    printf("[license] sending request ...\n");
    BOOL sent = WinHttpSendRequest(
        hReq,
        L"Content-Type: application/json\r\n",
        (DWORD)-1,
        body, body_len, body_len, 0);

    if (sent && WinHttpReceiveResponse(hReq, NULL)) {
        char resp[256] = {0};
        DWORD read = 0;
        WinHttpReadData(hReq, resp, sizeof(resp) - 1, &read);
        printf("[license] server response: %s\n", resp);
        result = (strstr(resp, "\"valid\":true") != NULL) ? 1 : 0;
    } else {
        printf("[license] no response (offline?)\n");
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
    printf("[license] result: %s\n", result == 1 ? "valid" : result == 0 ? "invalid" : "offline");
    return result;
}
#else
static int online_verify_key(const char *key) {
    (void)key;
    return -1; /* Offline-Fallback auf Mac/Linux bis libcurl eingebaut */
}
#endif

/* ── Offline-Timestamp aus der gespeicherten Lizenz lesen/schreiben ──── */
#ifdef _WIN32
static void save_last_online(void) {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("APPDATA", path, sizeof(path))) return;
    strncat(path, "\\bogoshop\\last_ok.txt", sizeof(path) - strlen(path) - 1);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%llu\n", (unsigned long long)time(NULL));
    fclose(f);
}

static int offline_grace_ok(void) {
    char path[MAX_PATH];
    if (!GetEnvironmentVariableA("APPDATA", path, sizeof(path))) return 0;
    strncat(path, "\\bogoshop\\last_ok.txt", sizeof(path) - strlen(path) - 1);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long long ts = 0;
    fscanf(f, "%llu", &ts);
    fclose(f);
    return ((unsigned long long)time(NULL) - ts) < OFFLINE_GRACE_SEC;
}
#else
static void save_last_online(void) {}
static int offline_grace_ok(void) { return 0; }
#endif

int validate_key(const char *key) {
    /* Bindestriche ignorieren, 20 Ziffern einlesen */
    int digits[20];
    int d = 0;
    for (int i = 0; key[i] && d < 20; i++) {
        if (key[i] == '-') continue;
        int v = key_char_to_val(key[i]);
        if (v < 0) return 0;
        digits[d++] = v;
    }
    if (d != 20) return 0;

    /* Erste 15 Ziffern → mixed data */
    unsigned long long data = 0;
    for (int i = 0; i < 15; i++) data = data * KEY_ALPHA_LEN + digits[i];

    /* Erwartete Prüfsumme (basiert direkt auf mixed data) */
    unsigned long long check_val = (data ^ KEY_SECRET2) % 7962624ULL; /* 24^5 */

    /* Letzte 5 Ziffern → tatsächliche Prüfsumme */
    unsigned long long actual = 0;
    for (int i = 15; i < 20; i++) actual = actual * KEY_ALPHA_LEN + digits[i];

    return check_val == actual;
}

int load_license(void) {
    char path[MAX_PATH];
#ifdef _WIN32
    if (!GetEnvironmentVariableA("APPDATA", path, sizeof(path))) return 0;
    strncat(path, "\\bogoshop\\license.key", sizeof(path) - strlen(path) - 1);
#else
    const char *home = getenv("HOME");
    if (!home) return 0;
    snprintf(path, sizeof(path), "%s/.config/bogoshop/license.key", home);
#endif
    printf("[license] looking for license at: %s\n", path);
    FILE *f = fopen(path, "r");
    if (!f) { printf("[license] no license file found\n"); return 0; }
    char key[32] = {0};
    fgets(key, sizeof(key), f);
    fclose(f);
    key[strcspn(key, "\r\n")] = '\0';
    printf("[license] found key: %s\n", key);

    /* Lokale Struktur muss stimmen (Prüfsumme) */
    if (!validate_key(key)) { printf("[license] local checksum invalid\n"); return 0; }
    printf("[license] local checksum OK\n");

    /* Online-Check: Server ist die eigentliche Autorität */
    int online = online_verify_key(key);
    if (online == 1) {
        save_last_online();
        return 1;
    }
    if (online == 0) { printf("[license] server rejected key\n"); return 0; }

    /* online == -1: kein Netz — Gnadenfrist prüfen */
    int grace = offline_grace_ok();
    printf("[license] offline grace period: %s\n", grace ? "OK" : "expired");
    return grace;
}

void save_license(const char *key) {
    char dir[MAX_PATH], path[MAX_PATH];
#ifdef _WIN32
    if (!GetEnvironmentVariableA("APPDATA", dir, sizeof(dir))) return;
    snprintf(path, sizeof(path), "%s\\bogoshop", dir);
    CreateDirectoryA(path, NULL); /* ignoriert Fehler falls schon vorhanden */
    snprintf(path, sizeof(path), "%s\\bogoshop\\license.key", dir);
#else
    const char *home = getenv("HOME");
    if (!home) return;
    snprintf(dir, sizeof(dir), "%s/.config/bogoshop", home);
    mkdir(dir, 0755); /* ignoriert Fehler falls schon vorhanden */
    snprintf(path, sizeof(path), "%s/license.key", dir);
#endif
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%s\n", key);
    fclose(f);
}

/* ── Gibt 1 zurück wenn Lizenz OK, 0 wenn abgebrochen ───────────────────── */
int show_license_screen(SDL_Window *win, SDL_Renderer *ren) {
    char input[32] = {0};
    int  input_len = 0;
    int  state     = 0; /* 0=eingabe, 1=ok, 2=fehler */

    SDL_StartTextInput();
    SDL_Event e;
    int running = 1, result = 0;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }
            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) { running = 0; break; }
                if (k == SDLK_BACKSPACE && input_len > 0) {
                    /* Rückwärts löschen, Bindestriche überspringen */
                    input[--input_len] = '\0';
                    while (input_len > 0 && input[input_len-1] == '-')
                        input[--input_len] = '\0';
                    state = 0;
                }
                if ((k == SDLK_v) && (e.key.keysym.mod & KMOD_CTRL)) {
                    char *clip = SDL_GetClipboardText();
                    if (clip) {
                        /* Eingabefeld leeren und Clipboard einfügen */
                        input_len = 0; input[0] = '\0';
                        for (int ci = 0; clip[ci] && input_len < 23; ci++) {
                            char c = (char)toupper((unsigned char)clip[ci]);
                            if (c == '-') continue;
                            if (key_char_to_val(c) < 0) continue;
                            input[input_len++] = c;
                            input[input_len]   = '\0';
                            if ((input_len == 5 || input_len == 11 || input_len == 17) && input_len < 23) {
                                input[input_len++] = '-';
                                input[input_len]   = '\0';
                            }
                        }
                        SDL_free(clip);
                        state = 0;
                    }
                }
                if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                    printf("[license] user submitted key: %s\n", input);
                    if (validate_key(input)) {
                        printf("[license] local checksum OK — checking server ...\n");
                        int online = online_verify_key(input);
                        if (online == 1 || online == -1) {
                            if (online == 1) save_last_online();
                            save_license(input);
                            printf("[license] activation successful\n");
                            state = 1; running = 0; result = 1;
                        } else {
                            printf("[license] server rejected key\n");
                            state = 2;
                        }
                    } else {
                        printf("[license] local checksum failed — invalid key\n");
                        state = 2;
                    }
                }
            }
            if (e.type == SDL_TEXTINPUT && input_len < 23) {
                char c = (char)toupper((unsigned char)e.text.text[0]);
                if (key_char_to_val(c) >= 0) {
                    /* Automatisch Bindestrich nach jeder 5. Zeichengruppe */
                    int raw = input_len - (input_len / 6); /* Zeichen ohne Bindestriche */
                    input[input_len++] = c;
                    input[input_len]   = '\0';
                    if ((input_len == 5 || input_len == 11 || input_len == 17) && input_len < 23) {
                        input[input_len++] = '-';
                        input[input_len]   = '\0';
                    }
                    (void)raw;
                    state = 0;
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
        SDL_RenderClear(ren);

        int ww, wh;
        SDL_GetWindowSize(win, &ww, &wh);
        float cx = ww / 2.0f;

        /* Titel */
        const char *title = "Bogoshop";
        draw_text_scaled(ren, cx - text_width(title) * 3.0f / 2.0f,
                         wh * 0.18f, title, 3.0f, 255, 255, 255);

        const char *sub = "activate licence";
        draw_text(ren, cx - text_width(sub) / 2.0f,
                  wh * 0.32f, sub, 180, 180, 180);

        /* Key-Eingabefeld */
        const char *disp = input_len ? input : "XXXXX-XXXXX-XXXXX-XXXXX";
        Uint8 kr = input_len ? 255 : 80;
        Uint8 kg = input_len ? 220 : 80;
        Uint8 kb = input_len ?   0 : 80;
        draw_text_scaled(ren,
                         cx - text_width(disp) * 2.0f / 2.0f,
                         wh * 0.44f, disp, 2.0f, kr, kg, kb);

        /* Status-Meldung */
        if (state == 2) {
            const char *err = "invalid key";
            draw_text(ren, cx - text_width(err) / 2.0f,
                      wh * 0.60f, err, 255, 80, 80);
        }

        const char *hint = "enter confirm   esc cancel";
        draw_text(ren, cx - text_width(hint) / 2.0f,
                  wh * 0.72f, hint, 100, 100, 100);

        SDL_RenderPresent(ren);
    }

    SDL_StopTextInput();
    return result;
}

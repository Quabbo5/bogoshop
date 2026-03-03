/*
 * Bogoshop Keygen
 * Usage:  keygen.exe <seriennummer>
 * Beispiel: keygen.exe 1
 *
 * WICHTIG: Diese Datei NICHT mit der EXE zusammen verschicken!
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_ALPHA      "ACDEFGHJKLMNPQRTVWXY3479"
#define KEY_ALPHA_LEN  24
#define KEY_SECRET1    0xA7F3C2D15B8E4A9CULL
#define KEY_SECRET2    0x3D7C1F94E2B6A850ULL

static void generate_key(unsigned long long serial, char out[24]) {
    /* Serial mischen damit Keys auch bei kleinen Nummern gut aussehen */
    unsigned long long data  = serial * KEY_SECRET1;
    unsigned long long check = (data ^ KEY_SECRET2) % 7962624ULL; /* 24^5 */

    int digits[20];

    /* Seriennummer als 15 Ziffern in base-24 codieren */
    unsigned long long tmp = data;
    for (int i = 14; i >= 0; i--) { digits[i] = (int)(tmp % KEY_ALPHA_LEN); tmp /= KEY_ALPHA_LEN; }

    /* Prüfsumme als 5 Ziffern */
    for (int i = 19; i >= 15; i--) { digits[i] = (int)(check % KEY_ALPHA_LEN); check /= KEY_ALPHA_LEN; }

    /* Ausgabe mit Bindestrichen */
    int pos = 0;
    for (int g = 0; g < 4; g++) {
        if (g > 0) out[pos++] = '-';
        for (int i = 0; i < 5; i++) out[pos++] = KEY_ALPHA[digits[g * 5 + i]];
    }
    out[pos] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <seriennummer>\n", argv[0]);
        fprintf(stderr, "Beispiel: %s 1\n", argv[0]);
        return 1;
    }

    unsigned long long serial = (unsigned long long)atoll(argv[1]);
    if (serial == 0) {
        fprintf(stderr, "Seriennummer muss >= 1 sein.\n");
        return 1;
    }

    char key[24];
    generate_key(serial, key);
    printf("Serial %llu  →  %s\n", serial, key);
    return 0;
}

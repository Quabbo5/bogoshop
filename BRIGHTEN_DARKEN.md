# Dokumentation: `brighten()` und `darken()`

---

## Grundlagen: Wie ein Bild im Speicher aussieht

Bevor wir die Funktionen verstehen können, müssen wir wissen wie ein Bild überhaupt gespeichert ist.

### Pixel und Kanäle

Ein Bild besteht aus **Pixeln** (einzelnen Bildpunkten). Jeder Pixel hat 4 **Farbkanäle**:

| Kanal | Bedeutung     | Wertebereich |
|-------|---------------|--------------|
| R     | Rot           | 0 – 255      |
| G     | Grün          | 0 – 255      |
| B     | Blau          | 0 – 255      |
| A     | Alpha (Trans) | 0 – 255      |

### Was ist ein Byte?

Ein **Byte** besteht aus **8 Bits**. Ein Bit kann nur `0` oder `1` sein.

```
1 Bit   = 0 oder 1
8 Bits  = 1 Byte  → kann Werte von 0 bis 255 speichern
```

Warum 0–255? Weil 2⁸ = 256 mögliche Werte (0 bis 255 einschließlich).

```
00000000 =   0  (schwarz / kein Anteil)
11111111 = 255  (maximaler Anteil / weiß)
```

### Wie liegt ein Pixel im Speicher?

Jeder Pixel belegt genau **4 Bytes** hintereinander im Speicher:

```
Index:  [0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8] ...
Inhalt:  R    G    B    A    R    G    B    A    R  ...
         ←── Pixel 0 ───→   ←── Pixel 1 ───→
```

Ein Bild mit 100×100 Pixeln belegt also `100 × 100 × 4 = 40.000 Bytes` im Speicher.

---

## Die Funktion `brighten(int amount)`

```c
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
```

### Zeile für Zeile erklärt

---

#### `static void brighten(int amount)`

```c
static void brighten(int amount)
```

| Teil            | Bedeutung |
|-----------------|-----------|
| `static`        | Die Funktion ist nur in dieser `.c`-Datei sichtbar (kein globaler Export) |
| `void`          | Die Funktion gibt keinen Wert zurück |
| `brighten`      | Der Name der Funktion |
| `int amount`    | Ein Parameter: um wie viel jeder Kanal aufgehellt wird (z.B. `30`) |

---

#### `save_undo_state()`

```c
save_undo_state();
```

Speichert den aktuellen Pixelzustand in den Undo-Ring-Puffer, bevor wir irgendetwas verändern. So kann `U` die Änderung rückgängig machen.

---

#### `int total_bytes = ctx.w * ctx.h * 4`

```c
int total_bytes = ctx.w * ctx.h * 4;
```

Berechnet die **Gesamtzahl der Bytes** im Bildspeicher.

```
Breite × Höhe × 4 Kanäle = Anzahl Bytes

Beispiel: 800 × 600 Pixel
→ 800 × 600 × 4 = 1.920.000 Bytes ≈ 1,9 MB
```

- `ctx.w` = Bildbreite in Pixeln
- `ctx.h` = Bildhöhe in Pixeln
- `× 4`   = 4 Bytes pro Pixel (RGBA)

---

#### Die `for`-Schleife

```c
for (int byte_index = 0; byte_index < total_bytes; byte_index++)
```

Geht **jeden einzelnen Byte** im Bildspeicher durch, von `0` bis zum letzten Byte.

| Teil                              | Bedeutung |
|-----------------------------------|-----------|
| `int byte_index = 0`              | Startwert: beginne beim ersten Byte |
| `byte_index < total_bytes`        | Bedingung: weitermachen solange nicht am Ende |
| `byte_index++`                    | Nach jeder Iteration: einen Byte weitergehen |

---

#### Alpha-Kanal überspringen

```c
if (byte_index % 4 == 3) continue;
```

`%` ist der **Modulo-Operator** — er gibt den Rest einer Division zurück.

```
byte_index % 4 gibt die Position innerhalb eines Pixels:

byte_index 0  → 0 % 4 = 0  → R-Kanal  ← ändern
byte_index 1  → 1 % 4 = 1  → G-Kanal  ← ändern
byte_index 2  → 2 % 4 = 2  → B-Kanal  ← ändern
byte_index 3  → 3 % 4 = 3  → A-Kanal  ← ÜBERSPRINGEN
byte_index 4  → 4 % 4 = 0  → R-Kanal  ← ändern
byte_index 5  → 5 % 4 = 1  → G-Kanal  ← ändern
...
```

Der Alpha-Kanal steuert die **Transparenz** des Pixels. Wenn wir ihn verändern, würde das Bild durchsichtig werden — das wollen wir nicht.

`continue` springt sofort zur nächsten Iteration der Schleife (überspringt den Rest).

---

#### Neuen Wert berechnen

```c
int neuer_wert = ctx.pixels[byte_index] + amount;
```

- `ctx.pixels[byte_index]` liest den aktuellen Wert des Bytes (z.B. `120`)
- `+ amount` addiert den Aufhellungsbetrag (z.B. `+30`)
- Ergebnis wird in `int` (32-bit Ganzzahl) gespeichert, **nicht** in `unsigned char`

Warum `int` und nicht direkt `unsigned char`?
Weil `unsigned char` nur 0–255 kann. `120 + 200 = 320` würde in `unsigned char` **überlaufen** (wraparound zu `64`). Wir speichern es erstmal in `int` um den Wert prüfen zu können.

---

#### Clamp auf Maximum 255

```c
ctx.pixels[byte_index] = neuer_wert > 255 ? 255 : (unsigned char)neuer_wert;
```

Das ist ein **ternärer Operator**: `bedingung ? wert_wenn_wahr : wert_wenn_falsch`

```
Wenn neuer_wert > 255:  schreibe 255          (Wert "abschneiden" = Clamping)
Sonst:                  schreibe neuer_wert   (normal übernehmen)
```

Ohne Clamping würde ein Wert von z.B. `280` bei der Umwandlung zu `unsigned char` überlaufen:
```
280 in binär (9 Bits):  1 0001 1000
Als unsigned char (8 Bits abschneiden): 0001 1000 = 24  ← falsch!
```

Mit Clamping: `280 → 255` — der Kanal bleibt maximal hell, aber korrekt.

`(unsigned char)` ist ein **Cast** — erzwungene Typumwandlung von `int` zu `unsigned char`.

---

#### Textur aktualisieren

```c
SDL_UpdateTexture(ctx.tex, NULL, ctx.pixels, ctx.w * 4);
```

Nachdem wir alle Bytes im RAM verändert haben, müssen wir SDL mitteilen dass die Textur (die auf der GPU liegt) neu geladen werden soll.

| Parameter      | Bedeutung |
|----------------|-----------|
| `ctx.tex`      | Die SDL-Textur die aktualisiert werden soll |
| `NULL`         | Gesamte Textur ersetzen (kein Teilbereich) |
| `ctx.pixels`   | Zeiger auf die neuen Pixeldaten im RAM |
| `ctx.w * 4`    | **Pitch**: Anzahl Bytes pro Zeile (Breite × 4 Bytes/Pixel) |

---

## Die Funktion `darken(int amount)`

```c
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
```

`darken` ist fast identisch mit `brighten` — es gibt nur **zwei Unterschiede**:

### Unterschied 1: Subtraktion statt Addition

```c
// brighten:
int neuer_wert = ctx.pixels[byte_index] + amount;

// darken:
int neuer_wert = ctx.pixels[byte_index] - amount;
```

Statt zu addieren (heller) wird subtrahiert (dunkler).

```
Beispiel: Kanal-Wert = 80, amount = 30
brighten: 80 + 30 = 110  → heller
darken:   80 - 30 =  50  → dunkler
```

### Unterschied 2: Clamp auf Minimum 0

```c
// brighten:
ctx.pixels[byte_index] = neuer_wert > 255 ? 255 : (unsigned char)neuer_wert;

// darken:
ctx.pixels[byte_index] = neuer_wert < 0 ? 0 : (unsigned char)neuer_wert;
```

Bei `brighten` clampten wir nach **oben** (max. 255).
Bei `darken` clampen wir nach **unten** (min. 0).

Ohne diesen Check würde ein negativer Wert beim Cast zu `unsigned char` **überlaufen**:
```
-10 als unsigned char (8-bit wraparound): 256 - 10 = 246  ← falsch!
```

Mit Clamping: `-10 → 0` — der Kanal bleibt schwarz, aber korrekt.

---

## Vergleich der beiden Funktionen

| Aspekt              | `brighten`               | `darken`                 |
|---------------------|--------------------------|--------------------------|
| Operation           | `+ amount`               | `- amount`               |
| Clamp-Richtung      | oben: `> 255 → 255`      | unten: `< 0 → 0`         |
| Effekt              | Kanäle werden heller     | Kanäle werden dunkler    |
| Extremfall          | Alles wird weiß (255)    | Alles wird schwarz (0)   |

---

## Visuelles Beispiel

Ein orangener Pixel: R=200, G=100, B=20, A=255

```
brighten(50):
  R: 200 + 50 = 250  ✓
  G: 100 + 50 = 150  ✓
  B:  20 + 50 =  70  ✓
  A: 255           (unberührt)
  → heller, etwas mehr Gelb

darken(50):
  R: 200 - 50 = 150  ✓
  G: 100 - 50 =  50  ✓
  B:  20 - 50 = -30 → 0 (geclamprt)
  A: 255           (unberührt)
  → dunkler, etwas mehr Braun
```

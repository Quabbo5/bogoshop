# Build Commands

## viewer.exe

Die Quelldateien müssen alle zusammen kompiliert werden:
- `src/viewer.c`                  — main, Render-Hilfsfunktionen
- `src/effects.c`                 — nummerierte Bildeffekte + Effekt-Tabelle
- `src/functions.c`               — Hotkey-Funktionen (reset, undo, crop, composite)
- `src/license.c`                 — Produktschlüssel, Lizenz-Screen
- `vendor/tinyfiledialogs.c`      — nativer Dateidialog (Windows + Mac)
- `assets/icon.res`               — eingebettetes App-Icon (nur Windows, aus icon.rc + icon.ico)

### Schritt 1: icon.res generieren (nur einmal nötig, bzw. wenn icon.png geändert wird)
```bash
windres assets/icon.rc -O coff -o assets/icon.res
```

### Debug Windows (mit Konsolenfenster)
```bash
gcc -DSDL_MAIN_HANDLED src/viewer.c src/effects.c src/functions.c src/license.c vendor/tinyfiledialogs.c assets/icon.res -o viewer.exe -g -O0 -I C:/msys64/mingw64/include -I include/ -I . -L C:/msys64/mingw64/lib -lSDL2 -lSDL2_ttf -lcomdlg32 -lole32 -lwinhttp -mwindows -mconsole
```

### Release Windows (kein Konsolenfenster, optimiert)
```bash
gcc -DSDL_MAIN_HANDLED src/viewer.c src/effects.c src/functions.c src/license.c vendor/tinyfiledialogs.c assets/icon.res -o viewer.exe -O2 -s -I C:/msys64/mingw64/include -I include/ -I . -L C:/msys64/mingw64/lib -lSDL2 -lSDL2_ttf -lcomdlg32 -lole32 -lwinhttp -mwindows
```

### Debug Mac
```bash
gcc -DSDL_MAIN_HANDLED src/viewer.c src/effects.c src/functions.c src/license.c vendor/tinyfiledialogs.c -o viewer_mac -g -O0 -I/opt/homebrew/include -I include/ -I . -L/opt/homebrew/lib -lSDL2
```

### Release Mac
```bash
gcc -DSDL_MAIN_HANDLED src/viewer.c src/effects.c src/functions.c src/license.c vendor/tinyfiledialogs.c -o viewer_mac -O2 -I/opt/homebrew/include -I include/ -I . -L/opt/homebrew/lib -lSDL2
```

### icon.ico neu generieren (wenn icon.png ausgetauscht wird)
```bash
python3 -c "
import struct
with open('assets/icon.png','rb') as f: png=f.read()
hdr=struct.pack('<HHH',0,1,1)
ent=struct.pack('<BBBBHHII',0,0,0,0,1,32,len(png),22)
open('assets/icon.ico','wb').write(hdr+ent+png)
"
```

## keygen.exe

```bash
gcc src/keygen.c -o keygen.exe
```

### Key generieren
```bash
./keygen.exe <seriennummer>

./keygen.exe 1
./keygen.exe 42
./keygen.exe 1000
```

## Release-Paket erstellen

Diese 3 Dateien zusammen verschicken (als ZIP):
- `viewer.exe`
- `SDL2.dll`  — aus `C:/msys64/mingw64/bin/`
- `libwinpthread-1.dll`  — aus `C:/msys64/mingw64/bin/`

Das Icon ist direkt im `viewer.exe` eingebettet — keine `icon.png` nötig.


# Commands
```text
e102
102


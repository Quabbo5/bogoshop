# Build Commands

## viewer.exe

Die Quelldateien müssen alle zusammen kompiliert werden:
- `viewer.c` — main, Render-Hilfsfunktionen
- `effects.c` — Bildeffekte, Undo/Reset
- `license.c` — Produktschlüssel, Lizenz-Screen
- `icon.res` — eingebettetes App-Icon (aus icon.rc + icon.ico)

### Schritt 1: icon.res generieren (nur einmal nötig, bzw. wenn icon.png geändert wird)
```bash
windres icon.rc -O coff -o icon.res
```

### Debug (mit Konsolenfenster)
```bash
gcc -DSDL_MAIN_HANDLED viewer.c effects.c license.c icon.res -o viewer.exe -g -O0 -I C:/msys64/mingw64/include -L C:/msys64/mingw64/lib -lSDL2 -lcomdlg32 -mconsole
```

### Release (kein Konsolenfenster, optimiert)
```bash
gcc -DSDL_MAIN_HANDLED viewer.c effects.c license.c icon.res -o viewer.exe -O2 -s -I C:/msys64/mingw64/include -L C:/msys64/mingw64/lib -lSDL2 -lcomdlg32 -mwindows
```

### icon.ico neu generieren (wenn icon.png ausgetauscht wird)
```bash
python3 -c "
import struct
with open('assets/icon.png','rb') as f: png=f.read()
hdr=struct.pack('<HHH',0,1,1)
ent=struct.pack('<BBBBHHII',0,0,0,0,1,32,len(png),22)
open('icon.ico','wb').write(hdr+ent+png)
"
```

## keygen.exe

```bash
gcc keygen.c -o keygen.exe
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

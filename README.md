# mkConversions

Kleiner Qt6-Dialog zum Konvertieren von Bild-, Audio- und Videodateien zwischen Formaten.
Linux-Portierung des früheren AutoHotkey-v1.1-Scripts `Conversions.ahk`.

## Bedienung

```
mkConversions [-r|--recursive] <Dateien und/oder Ordner...>
```

- Ordner werden nach Dateien durchsucht (mit `-r` auch Unterordner).
- Der Dialog zeigt nur die Spalten (Bild/Audio/Video) und Quellformate (Dateiendungen), die in den übergebenen
  Dateien vorkommen. Je Quellformat gibt es eine Zeile `( ) PNG -> [Zielformat ▾]`; die Liste enthält alle
  Formate der Kategorie außer dem Quellformat (verlustfreie zuerst). Wählt man ein verlustbehaftetes Ziel,
  wird die Zeile **rot** und der Start muss bestätigt werden.
- **Start** → **Pause** → **Fortsetzen** … → **Schließen**. Pause greift, sobald die aktuelle Datei fertig ist.
- Nach erfolgreicher Konvertierung wird die **Quelldatei gelöscht**. Existiert die Zieldatei bereits, wird die Datei übersprungen.
- Schließen/Escape während der Konvertierung bricht ab: der laufende Prozess wird beendet, die unfertige Zieldatei gelöscht,
  die Quelldatei bleibt erhalten.
- Fehler und Hinweise werden gesammelt und am Ende in einem Bericht angezeigt.

## Konvertierungen

**Bilder** – Quellen: BMP, PNG, JPG, JXL, WEBP. Ziele (jeweils ohne das Quellformat):

| Ziel                | Aufruf                                                                 |
|---------------------|------------------------------------------------------------------------|
| PNG                 | `magick SRC [PNG24:]DST` (PNG24 nur aus BMP/JPG, sonst bleiben Alpha/16 Bit erhalten) |
| BMP                 | `magick SRC DST`                                                       |
| JXL Lossless        | `magick SRC -quality 100 DST`; aus JPG: `cjxl --lossless_jpeg=1 SRC DST` (umkehrbar per `djxl`) |
| WEBP Lossless       | `magick SRC -define webp:lossless=true DST` (WebP kann max. 8 Bit/Kanal) |
| JPG (Q92) *(lossy)* | `magick SRC -background white -alpha remove -alpha off -quality 92 DST` |
| JXL (Q96) *(lossy)* | `magick SRC -quality 96 DST`                                           |
| WEBP (Q90) *(lossy)*| `magick SRC -quality 90 DST`                                           |

**Audio** – Quellen: AAC, FLAC, MP3, WAV.

| Ziel                     | Aufruf                                                            |
|--------------------------|-------------------------------------------------------------------|
| WAV                      | AAC: `faad -q -d`, FLAC: `flac -s -d`, MP3: `lame --decode`       |
| FLAC                     | WAV: `flac -s`; AAC/MP3: `ffmpeg -c:a flac -sample_fmt s16`       |
| M4A                      | AAC: `ffmpeg -c:a copy` (verlustfrei umverpackt)                  |
| M4A (AAC 256k) *(lossy)* | `ffmpeg -c:a aac -b:a 256k -movflags +faststart`                  |
| AAC (256k) *(lossy)*     | `ffmpeg -c:a aac -b:a 256k`                                       |
| MP3 (V0) *(lossy)*       | WAV: `lame -V 0` (= `--preset extreme`); sonst `ffmpeg -c:a libmp3lame -q:a 0` |

**Video** – Quelle: MP4 → M4A: `ffmpeg -vn -c:a copy -movflags +faststart` (Audiospur verlustfrei extrahieren).

Alle ffmpeg-Aufrufe laufen mit `-n -vn` (nie überschreiben, nur Audio).
**Animierte** JXL/WebP-Dateien werden übersprungen (Prüfung per `jxlinfo` bzw. `magick identify`), da ImageMagick
sonst nur das erste Frame übernehmen bzw. mehrere Einzeldateien schreiben würde und die Quelle anschließend gelöscht wird.

Endungen werden ohne Beachtung der Groß-/Kleinschreibung erkannt; `.jpeg` zählt als JPG.
Eine Konvertierung gilt nur als erfolgreich, wenn das Werkzeug mit Code 0 endet **und** eine nicht-leere Zieldatei existiert.

Arch-Pakete: `imagemagick` (mit libjxl/libwebp), `libjxl` (für `jxlinfo`, `cjxl`), `ffmpeg`, `flac`, `lame`, `faad2`.

## Bauen

```
./build.sh            # baut nach build/bin/ und kopiert nach ~/.local/bin/
./build_lupdate.sh    # aktualisiert i18n/*.ts
```

## Dolphin-Kontextmenü

```
./servicemenu-install.sh   # "Formate konvertieren" (Dateien + Ordner), "… (inkl. Unterordner)" (nur Ordner)
./servicemenu-remove.sh
```

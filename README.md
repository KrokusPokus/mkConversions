# mkConversions

Small Qt6 dialog for converting image, audio and video files between formats.
Linux port of the former AutoHotkey v1.1 script `Conversions.ahk`.

## Usage

```
mkConversions [-r|--recursive] <files and/or folders...>
```

- Folders are searched for files (with `-r` also subfolders).
- The dialog shows only the columns (image/audio/video) and source formats (file extensions) that occur in the given
  files. For each source format there is a row `( ) PNG -> [target format ▾]`; the list contains all
  formats of the category except the source format (lossless ones first). If a lossy target is selected,
  the row turns **red** and the start has to be confirmed.
- **Start** → **Pause** → **Continue** … → **Close**. Pause takes effect as soon as the current file is finished.
- After a successful conversion the **source file is deleted**. If the target file already exists, the file is skipped.
- Close/Escape during the conversion aborts it: the running process is terminated, the unfinished target file is deleted,
  the source file is kept.
- Errors and notes are collected and shown in a report at the end.

## Conversions

**Images** – sources: BMP, PNG, JPG, JXL, WEBP. Targets (each excluding the source format):

| Target              | Command                                                                |
|---------------------|------------------------------------------------------------------------|
| PNG                 | `magick SRC [PNG24:]DST` (PNG24 only from BMP/JPG, otherwise alpha/16 bit are preserved) |
| BMP                 | `magick SRC DST`                                                       |
| JXL Lossless        | `magick SRC -quality 100 DST`; from JPG: `cjxl --lossless_jpeg=1 SRC DST` (reversible via `djxl`) |
| WEBP Lossless       | `magick SRC -define webp:lossless=true DST` (WebP supports max. 8 bit/channel) |
| JPG (Q92) *(lossy)* | `magick SRC -background white -alpha remove -alpha off -quality 92 DST` |
| JXL (Q96) *(lossy)* | `magick SRC -quality 96 DST`                                           |
| WEBP (Q90) *(lossy)*| `magick SRC -quality 90 DST`                                           |

**Audio** – sources: AAC, FLAC, MP3, WAV.

| Target                   | Command                                                           |
|--------------------------|-------------------------------------------------------------------|
| WAV                      | AAC: `faad -q -d`, FLAC: `flac -s -d`, MP3: `lame --decode`       |
| FLAC                     | WAV: `flac -s`; AAC/MP3: `ffmpeg -c:a flac -sample_fmt s16`       |
| M4A                      | AAC: `ffmpeg -c:a copy` (losslessly repackaged)                   |
| M4A (AAC 256k) *(lossy)* | `ffmpeg -c:a aac -b:a 256k -movflags +faststart`                  |
| AAC (256k) *(lossy)*     | `ffmpeg -c:a aac -b:a 256k`                                       |
| MP3 (V0) *(lossy)*       | WAV: `lame -V 0` (= `--preset extreme`); otherwise `ffmpeg -c:a libmp3lame -q:a 0` |

**Video** – source: MP4 → M4A: `ffmpeg -vn -c:a copy -movflags +faststart` (extract the audio track losslessly).

All ffmpeg calls run with `-n -vn` (never overwrite, audio only).
**Animated** JXL/WebP files are skipped (checked via `jxlinfo` or `magick identify`), because ImageMagick
would otherwise take over only the first frame or write several individual files, and the source is deleted afterwards.

Extensions are recognized case-insensitively; `.jpeg` counts as JPG.
A conversion is only considered successful if the tool exits with code 0 **and** a non-empty target file exists.

Arch packages: `imagemagick` (with libjxl/libwebp), `libjxl` (for `jxlinfo`, `cjxl`), `ffmpeg`, `flac`, `lame`, `faad2`.

## Building

```
./build.sh            # builds into build/bin/ and copies to ~/.local/bin/
./build_lupdate.sh    # updates i18n/*.ts
```

## Dolphin context menu

```
./servicemenu-install.sh   # "Convert formats" (files + folders), "… (including subfolders)" (folders only)
./servicemenu-remove.sh
```

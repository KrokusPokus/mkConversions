#include "Conversions.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>

#include <cstdlib>

QString SConversion::TargetPathFor(const QString& sSourcePath) const
{
    const QFileInfo fi(sSourcePath);
    // completeBaseName() entspricht FNameNoExt aus SplitPath bzw. ${FBase%.*}
    return fi.path() + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('.') + sTargetExt;
}

namespace {

using ArgsFn = std::function<QStringList(const QString& sSource, const QString& sTarget)>;

// ===================================================================================== Hilfsfunktionen

// Animierte WebP/JXL-Dateien ablehnen: ImageMagick würde bei JXL stillschweigend nur das erste Frame lesen
// bzw. bei WebP mehrere Zieldateien (name-0.png, name-1.png, ...) schreiben. Da die Quelldatei nach
// erfolgreicher Konvertierung gelöscht wird, ginge die Animation verloren.
QString CheckStillImage(const QString& sSource)
{
    constexpr int kTimeoutMs = 30000;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setStandardInputFile(QProcess::nullDevice());

    if (QFileInfo(sSource).suffix().compare(QLatin1String("jxl"), Qt::CaseInsensitive) == 0) {
        // magick zählt bei JXL nur 1 Frame, daher libjxl selbst fragen
        proc.start(QStringLiteral("jxlinfo"), { sSource });
        if (!proc.waitForStarted())
            return QCoreApplication::translate("Conversions", "jxlinfo could not be started (needed to detect animated JXL files).");
        proc.waitForFinished(kTimeoutMs);
        if (proc.readAll().contains("JPEG XL animation"))
            return QCoreApplication::translate("Conversions", "File is an animation (only still images are supported).");
        return {};
    }

    proc.start(QStringLiteral("magick"), { QStringLiteral("identify"), QStringLiteral("-ping"),
                                           QStringLiteral("-format"), QStringLiteral("%n\n"), sSource });
    if (!proc.waitForStarted())
        return {};  // magick fehlt -> wird bereits beim Start geprüft
    proc.waitForFinished(kTimeoutMs);
    const int nFrames = QString::fromLocal8Bit(proc.readAll()).section(QLatin1Char('\n'), 0, 0).trimmed().toInt();
    if (nFrames > 1)
        return QCoreApplication::translate("Conversions", "File is an animation with %1 frames (only still images are supported).").arg(nFrames);
    return {};  // nicht lesbar -> die eigentliche Konvertierung schlägt fehl und meldet den Fehler
}

// ImageMagick 7: magick SRC [Optionen] [PREFIX:]DST
ArgsFn Magick(const QStringList& aOptions, const QString& sTargetPrefix = {})
{
    return [aOptions, sTargetPrefix](const QString& s, const QString& t) {
        return QStringList{ s } + aOptions + QStringList{ sTargetPrefix + t };
    };
}

// ffmpeg: -n statt -y -> niemals eine vorhandene Datei überschreiben (wird vorher ohnehin geprüft).
// -vn: nur Audio (verhindert u.a., dass eingebettete Cover-Bilder in .aac/.m4a scheitern).
ArgsFn FFmpeg(const QStringList& aOutputOptions)
{
    return [aOutputOptions](const QString& s, const QString& t) {
        return QStringList{ "-nostdin", "-hide_banner", "-loglevel", "error", "-n", "-i", s, "-vn" }
               + aOutputOptions + QStringList{ t };
    };
}

// ===================================================================================== Formate

// Ein Quellformat (= eine Zeile im Dialog)
struct SSourceFormat {
    QString     sKey;           // interner Schlüssel, z.B. "png"
    QString     sName;          // Anzeige, z.B. "PNG"
    QStringList aExts;
};

// Eine Zielvariante (= ein Eintrag in der DropDown-Liste)
struct STargetVariant {
    QString sKey;               // interner Schlüssel, z.B. "jxl_lossless"
    QString sFormatKey;         // Formatschlüssel -> wird beim gleichen Quellformat ausgeblendet
    QString sLabel;
    QString sExt;
    bool    bLossy;
};

// Konkrete Umsetzung eines (Quelle, Ziel)-Paars
struct SCommand {
    QString     sProgram;
    ArgsFn      fnArgs;
    QStringList aHelperPrograms = {};
    QString     sLabelOverride = {};    // abweichender DropDown-Text
    int         nLossyOverride = -1;    // -1 = bLossy der Zielvariante, 0/1 = überschreiben
};

const QString kMagick = QStringLiteral("magick");
const QString kFFmpeg = QStringLiteral("ffmpeg");
const QString kFlac   = QStringLiteral("flac");
const QString kLame   = QStringLiteral("lame");
const QString kFaad   = QStringLiteral("faad");
const QString kCjxl   = QStringLiteral("cjxl");

// ------------------------------------------------------------------------------------- Bilder
// Reihenfolge der DropDown-Liste: verlustfreie Ziele zuerst
const QList<STargetVariant> kImageTargets = {
    { "png",          "png",  "PNG",           "png",  false },
    { "bmp",          "bmp",  "BMP",           "bmp",  false },
    { "jxl_lossless", "jxl",  "JXL Lossless",  "jxl",  false },
    { "webp_lossless","webp", "WEBP Lossless", "webp", false },
    { "jpg",          "jpg",  "JPG (Q92)",     "jpg",  true  },
    { "jxl",          "jxl",  "JXL (Q96)",     "jxl",  true  },
    { "webp",         "webp", "WEBP (Q90)",    "webp", true  },
};

SCommand ImageCommand(const QString& sSrc, const QString& sTgt)
{
    // JPG -> JXL verlustfrei: cjxl packt den JPEG-Datenstrom verlustfrei (und umkehrbar) um, ~20 % kleiner.
    // magick -quality 100 würde stattdessen die dekodierten Pixel speichern (deutlich größer als das JPG).
    if (sSrc == "jpg" && sTgt == "jxl_lossless")
        return { kCjxl, [](const QString& s, const QString& t) {
                     return QStringList{ "--quiet", "--lossless_jpeg=1", s, t }; } };

    // Transparenz auf weißem Hintergrund verrechnen, sonst erscheinen transparente Bereiche z.B. schwarz.
    // JPG-Qualität explizit setzen (= ImageMagick-Standard 92), damit die Angabe im Dialog immer stimmt.
    if (sTgt == "jpg")
        return { kMagick, Magick({ "-background", "white", "-alpha", "remove", "-alpha", "off", "-quality", "92" }) };
    if (sTgt == "png")  // PNG24 wie im Original für BMP/JPG; bei JXL/WebP Alphakanal und 16 Bit erhalten
        return { kMagick, Magick({}, (sSrc == "bmp" || sSrc == "jpg") ? QStringLiteral("PNG24:") : QString()) };
    if (sTgt == "jxl_lossless")
        return { kMagick, Magick({ "-quality", "100" }) };
    if (sTgt == "jxl")
        return { kMagick, Magick({ "-quality", "96" }) };
    if (sTgt == "webp_lossless")
        return { kMagick, Magick({ "-define", "webp:lossless=true" }) };
    if (sTgt == "webp")
        return { kMagick, Magick({ "-quality", "90" }) };
    return { kMagick, Magick({}) };     // bmp
}

// ------------------------------------------------------------------------------------- Audio
const QList<STargetVariant> kAudioTargets = {
    { "wav",  "wav",  "WAV",            "wav",  false },
    { "flac", "flac", "FLAC",           "flac", false },
    { "m4a",  "m4a",  "M4A (AAC 256k)", "m4a",  true  },
    { "aac",  "aac",  "AAC (256k)",     "aac",  true  },
    { "mp3",  "mp3",  "MP3 (V0)",       "mp3",  true  },
};

SCommand AudioCommand(const QString& sSrc, const QString& sTgt)
{
    // Bewährte Werkzeuge aus dem Original, wo sie direkt passen
    if (sSrc == "aac" && sTgt == "wav")     // faad -d: Downmix von Mehrkanal auf Stereo wie im Original
        return { kFaad, [](const QString& s, const QString& t) { return QStringList{ "-q", "-d", "-o", t, s }; } };
    if (sSrc == "flac" && sTgt == "wav")    // erhält die Bittiefe (z.B. 24 Bit)
        return { kFlac, [](const QString& s, const QString& t) { return QStringList{ "-s", "-d", "-o", t, s }; } };
    if (sSrc == "mp3" && sTgt == "wav")     // unter Windows: madplay
        return { kLame, [](const QString& s, const QString& t) { return QStringList{ "--nohist", "--decode", s, t }; } };
    if (sSrc == "wav" && sTgt == "flac")
        return { kFlac, [](const QString& s, const QString& t) { return QStringList{ "-s", "-o", t, s }; } };
    if (sSrc == "wav" && sTgt == "mp3")     // -V 0 entspricht --preset extreme aus dem Original
        return { kLame, [](const QString& s, const QString& t) { return QStringList{ "--nohist", "-V", "0", s, t }; } };
    if (sSrc == "aac" && sTgt == "m4a")     // nur umverpacken -> verlustfrei
        return { kFFmpeg, FFmpeg({ "-c:a", "copy", "-movflags", "+faststart" }), {}, "M4A", 0 };

    // Alle übrigen Kombinationen über ffmpeg
    if (sTgt == "flac")     // Quellen hier nur AAC/MP3 (verlustbehaftet dekodiert) -> 16 Bit genügen
        return { kFFmpeg, FFmpeg({ "-c:a", "flac", "-sample_fmt", "s16" }) };
    if (sTgt == "mp3")      // -q:a 0 = VBR V0 (wie lame -V 0)
        return { kFFmpeg, FFmpeg({ "-c:a", "libmp3lame", "-q:a", "0" }) };
    if (sTgt == "aac")
        return { kFFmpeg, FFmpeg({ "-c:a", "aac", "-b:a", "256k" }) };
    if (sTgt == "m4a")
        return { kFFmpeg, FFmpeg({ "-c:a", "aac", "-b:a", "256k", "-movflags", "+faststart" }) };
    return { kFFmpeg, FFmpeg({ "-c:a", "pcm_s16le" }) };   // wav (wird oben bereits vollständig abgedeckt)
}

// ------------------------------------------------------------------------------------- Video
const QList<STargetVariant> kVideoTargets = {
    { "m4a", "m4a", "M4A", "m4a", false },
};

SCommand VideoCommand(const QString& /*sSrc*/, const QString& /*sTgt*/)
{
    // Audiospur ohne Re-Encoding in einen M4A-Container kopieren
    return { kFFmpeg, FFmpeg({ "-c:a", "copy", "-movflags", "+faststart" }) };
}

// ===================================================================================== Matrix

void AddCategory(QList<SConversion>& aOut, EMediaCategory eCategory,
                 const QList<SSourceFormat>& aSources, const QList<STargetVariant>& aTargets,
                 const std::function<SCommand(const QString&, const QString&)>& fnCommand)
{
    for (const SSourceFormat& src : aSources) {
        const bool bAnimatable = (src.sKey == "jxl" || src.sKey == "webp");

        for (const STargetVariant& tgt : aTargets) {
            if (tgt.sFormatKey == src.sKey)
                continue;   // kein Ziel = Quellformat

            SCommand cmd = fnCommand(src.sKey, tgt.sKey);

            SConversion conv;
            conv.sId          = src.sName + QStringLiteral("_to_") + tgt.sKey.toUpper();
            conv.eCategory    = eCategory;
            conv.sSourceName  = src.sName;
            conv.aSourceExts  = src.aExts;
            conv.sTargetLabel = cmd.sLabelOverride.isEmpty() ? tgt.sLabel : cmd.sLabelOverride;
            conv.sTargetExt   = tgt.sExt;
            conv.bLossy       = (cmd.nLossyOverride < 0) ? tgt.bLossy : (cmd.nLossyOverride != 0);
            conv.sProgram     = cmd.sProgram;
            conv.fnArgs       = cmd.fnArgs;
            conv.aHelperPrograms = cmd.aHelperPrograms;
            if (bAnimatable) {
                conv.fnCheckSource = CheckStillImage;
                if (src.sKey == "jxl")
                    conv.aHelperPrograms.append(QStringLiteral("jxlinfo"));
            }
            aOut.append(conv);
        }
    }
}

QList<SConversion> BuildConversions()
{
    QList<SConversion> a;

    AddCategory(a, EMediaCategory::Image,
                { { "bmp",  "BMP",  { "bmp" } },
                  { "png",  "PNG",  { "png" } },
                  { "jpg",  "JPG",  { "jpg", "jpeg" } },
                  { "jxl",  "JXL",  { "jxl" } },
                  { "webp", "WEBP", { "webp" } } },
                kImageTargets, ImageCommand);

    AddCategory(a, EMediaCategory::Audio,
                { { "aac",  "AAC",  { "aac" } },
                  { "flac", "FLAC", { "flac" } },
                  { "mp3",  "MP3",  { "mp3" } },
                  { "wav",  "WAV",  { "wav" } } },
                kAudioTargets, AudioCommand);

    AddCategory(a, EMediaCategory::Video,
                { { "mp4",  "MP4",  { "mp4" } } },
                kVideoTargets, VideoCommand);

    return a;
}

} // namespace

const QList<SConversion>& AllConversions()
{
    static const QList<SConversion> s_aConversions = BuildConversions();
    return s_aConversions;
}

QString FormatBytes(qint64 nBytes)
{
    static const char* const s_aUnits[] = { "KB", "MB", "GB", "TB", "PB" };

    const qint64 nAbs = std::llabs(nBytes);
    if (nAbs < 1024)
        return QStringLiteral("%1 B").arg(nBytes);

    double dValue = static_cast<double>(nBytes) / 1024.0;
    int nUnit = 0;
    while (std::abs(dValue) >= 1024.0 && nUnit < 4) {
        dValue /= 1024.0;
        ++nUnit;
    }
    return QStringLiteral("%1 %2").arg(QLocale().toString(dValue, 'f', 2), QLatin1String(s_aUnits[nUnit]));
}

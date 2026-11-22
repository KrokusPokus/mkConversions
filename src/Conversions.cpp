#include "Conversions.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QSet>

#include <algorithm>
#include <cstdlib>

QString SConversion::TargetPathFor(const QString& sSourcePath) const
{
    const QFileInfo fi(sSourcePath);
    // completeBaseName() strips only the last extension (e.g. "a.tar.gz" -> "a.tar")
    return fi.path() + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('.') + sTargetExt;
}

QStringList CollectFiles(const QStringList& aPaths, bool bRecursive)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    QStringList aFiles;
    QSet<QString> aSeen;
    auto addFile = [&](const QString& sPath) {
        const QString sAbs = QFileInfo(sPath).absoluteFilePath();
        if (!aSeen.contains(sAbs)) {
            aSeen.insert(sAbs);
            aFiles.append(sAbs);
        }
    };

    for (const QString& sArg : aPaths) {
        const QFileInfo fi(sArg);
        if (!fi.exists())
            continue;

        if (fi.isDir()) {
            QStringList aDirFiles;
            QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::Hidden | QDir::System,
                            bRecursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
            while (it.hasNext())
                aDirFiles.append(it.next());
            std::sort(aDirFiles.begin(), aDirFiles.end(), collator);
            for (const QString& sFile : std::as_const(aDirFiles))
                addFile(sFile);
        }
        else {
            addFile(fi.absoluteFilePath());
        }
    }
    return aFiles;
}

namespace {

using ArgsFn = std::function<QStringList(const QString& sSource, const QString& sTarget)>;

// ===================================================================================== Helper functions

// Reject animated WebP/JXL files: for JXL, ImageMagick would silently read only the first frame,
// for WebP it would write several target files (name-0.png, name-1.png, ...). Since the source file
// is deleted after a successful conversion, the animation would be lost.
QString CheckStillImage(const QString& sSource)
{
    constexpr int kTimeoutMs = 30000;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setStandardInputFile(QProcess::nullDevice());

    if (QFileInfo(sSource).suffix().compare(QLatin1String("jxl"), Qt::CaseInsensitive) == 0) {
        // magick counts only 1 frame for JXL, so ask libjxl directly
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
        return {};  // magick missing -> already checked on start
    proc.waitForFinished(kTimeoutMs);
    const int nFrames = QString::fromLocal8Bit(proc.readAll()).section(QLatin1Char('\n'), 0, 0).trimmed().toInt();
    if (nFrames > 1)
        return QCoreApplication::translate("Conversions", "File is an animation with %1 frames (only still images are supported).").arg(nFrames);
    return {};  // not readable -> the actual conversion fails and reports the error
}

// ImageMagick 7: magick SRC [options] [PREFIX:]DST
ArgsFn Magick(const QStringList& aOptions, const QString& sTargetPrefix = {})
{
    return [aOptions, sTargetPrefix](const QString& s, const QString& t) {
        return QStringList{ s } + aOptions + QStringList{ sTargetPrefix + t };
    };
}

// ffmpeg: -n instead of -y -> never overwrite an existing file (checked beforehand anyway).
// -vn: audio only (among other things, prevents failures due to embedded cover images in .aac/.m4a).
ArgsFn FFmpeg(const QStringList& aOutputOptions)
{
    return [aOutputOptions](const QString& s, const QString& t) {
        return QStringList{ "-nostdin", "-hide_banner", "-loglevel", "error", "-n", "-i", s, "-vn" }
               + aOutputOptions + QStringList{ t };
    };
}

// ===================================================================================== Formats

// A source format (= one row in the dialog)
struct SSourceFormat {
    QString     sKey;           // internal key, e.g. "png"
    QString     sName;          // display name, e.g. "PNG"
    QStringList aExts;
};

// A target variant (= one entry in the drop-down list)
struct STargetVariant {
    QString sKey;               // internal key, e.g. "jxl_lossless"
    QString sFormatKey;         // format key -> hidden for the same source format
    QString sLabel;
    QString sExt;
    bool    bLossy;
};

// Concrete implementation of a (source, target) pair
struct SCommand {
    QString     sProgram;
    ArgsFn      fnArgs;
    QStringList aHelperPrograms = {};
    QString     sLabelOverride = {};    // differing drop-down text
    int         nLossyOverride = -1;    // -1 = bLossy of the target variant, 0/1 = override
};

const QString kMagick = QStringLiteral("magick");
const QString kFFmpeg = QStringLiteral("ffmpeg");
const QString kFlac   = QStringLiteral("flac");
const QString kLame   = QStringLiteral("lame");
const QString kFaad   = QStringLiteral("faad");
const QString kCjxl   = QStringLiteral("cjxl");

// ------------------------------------------------------------------------------------- Images
// Drop-down list order: lossless targets first
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
    // JPG -> lossless JXL: cjxl repackages the JPEG data stream losslessly (and reversibly), ~20 % smaller.
    // magick -quality 100 would store the decoded pixels instead (considerably larger than the JPG).
    if (sSrc == "jpg" && sTgt == "jxl_lossless")
        return { kCjxl, [](const QString& s, const QString& t) {
                     return QStringList{ "--quiet", "--lossless_jpeg=1", s, t }; } };

    // Flatten transparency onto a white background, otherwise transparent areas may appear black, for example.
    // Set the JPG quality explicitly (= ImageMagick default 92) so the value shown in the dialog is always correct.
    if (sTgt == "jpg")
        return { kMagick, Magick({ "-background", "white", "-alpha", "remove", "-alpha", "off", "-quality", "92" }) };
    if (sTgt == "png")  // PNG24 for BMP/JPG; keep alpha channel and 16 bit for JXL/WebP
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
    // Dedicated tools where they fit directly
    if (sSrc == "aac" && sTgt == "wav")     // faad -d: downmix from multichannel to stereo
        return { kFaad, [](const QString& s, const QString& t) { return QStringList{ "-q", "-d", "-o", t, s }; } };
    if (sSrc == "flac" && sTgt == "wav")    // keeps the bit depth (e.g. 24 bit)
        return { kFlac, [](const QString& s, const QString& t) { return QStringList{ "-s", "-d", "-o", t, s }; } };
    if (sSrc == "mp3" && sTgt == "wav")
        return { kLame, [](const QString& s, const QString& t) { return QStringList{ "--nohist", "--decode", s, t }; } };
    if (sSrc == "wav" && sTgt == "flac")
        return { kFlac, [](const QString& s, const QString& t) { return QStringList{ "-s", "-o", t, s }; } };
    if (sSrc == "wav" && sTgt == "mp3")     // -V 0 corresponds to --preset extreme
        return { kLame, [](const QString& s, const QString& t) { return QStringList{ "--nohist", "-V", "0", s, t }; } };
    if (sSrc == "aac" && sTgt == "m4a")     // repackage only -> lossless
        return { kFFmpeg, FFmpeg({ "-c:a", "copy", "-movflags", "+faststart" }), {}, "M4A", 0 };

    // All remaining combinations via ffmpeg
    if (sTgt == "flac")     // sources here are only AAC/MP3 (lossy decoded) -> 16 bit is sufficient
        return { kFFmpeg, FFmpeg({ "-c:a", "flac", "-sample_fmt", "s16" }) };
    if (sTgt == "mp3")      // -q:a 0 = VBR V0 (like lame -V 0)
        return { kFFmpeg, FFmpeg({ "-c:a", "libmp3lame", "-q:a", "0" }) };
    if (sTgt == "aac")
        return { kFFmpeg, FFmpeg({ "-c:a", "aac", "-b:a", "256k" }) };
    if (sTgt == "m4a")
        return { kFFmpeg, FFmpeg({ "-c:a", "aac", "-b:a", "256k", "-movflags", "+faststart" }) };
    return { kFFmpeg, FFmpeg({ "-c:a", "pcm_s16le" }) };   // wav (already fully covered above)
}

// ------------------------------------------------------------------------------------- Video
const QList<STargetVariant> kVideoTargets = {
    { "m4a", "m4a", "M4A", "m4a", false },
};

SCommand VideoCommand(const QString& /*sSrc*/, const QString& /*sTgt*/)
{
    // Copy the audio track into an M4A container without re-encoding
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
                continue;   // no target = source format

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

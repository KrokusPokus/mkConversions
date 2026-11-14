#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// Media category = one column in the dialog
enum class EMediaCategory {
    Image,
    Audio,
    Video
};

// A conversion from exactly one source format to exactly one target variant.
// In the dialog, each source format forms one row ("PNG  ->"); the associated
// conversions appear in that row's target drop-down list.
struct SConversion {
    QString         sId;                // e.g. "PNG_to_JXL" (diagnostics/tests)
    EMediaCategory  eCategory = EMediaCategory::Image;
    QString         sSourceName;        // display name of the source format, e.g. "PNG" (= row in the dialog)
    QStringList     aSourceExts;        // source extensions, lowercase, without dot
    QString         sTargetLabel;       // entry in the drop-down list, e.g. "JXL (Q96)"
    QString         sTargetExt;         // target extension, lowercase, without dot
    bool            bLossy = true;      // quality loss -> red text + confirmation prompt (defaults to lossy to be on the safe side)
    QString         sProgram;           // program to run (looked up in PATH)
    std::function<QStringList(const QString& sSource, const QString& sTarget)> fnArgs;
    // Optional pre-check of the source file: empty string = OK, otherwise the reason for skipping it
    std::function<QString(const QString& sSource)> fnCheckSource = {};
    // Additionally required programs (e.g. for fnCheckSource)
    QStringList     aHelperPrograms = {};

    bool AcceptsSuffix(const QString& sSuffix) const { return aSourceExts.contains(sSuffix.toLower()); }
    QString TargetPathFor(const QString& sSourcePath) const;
};

// All known conversions; grouped by category and source format, and within a
// source format in drop-down list order (lossless targets first).
const QList<SConversion>& AllConversions();

// Byte count in human-readable form (replacement for String_AutoByteFormat() from the AHK script)
QString FormatBytes(qint64 nBytes);

#endif // CONVERSIONS_H

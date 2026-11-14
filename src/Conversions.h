#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// Medienkategorie = eine Spalte im Dialog
enum class EMediaCategory {
    Image,
    Audio,
    Video
};

// Eine Konvertierung von genau einem Quellformat in genau eine Zielvariante.
// Im Dialog bildet jedes Quellformat eine Zeile ("PNG  ->"), die zugehörigen
// Konvertierungen erscheinen in deren Ziel-DropDown-Liste.
struct SConversion {
    QString         sId;                // z.B. "PNG_to_JXL" (Diagnose/Tests)
    EMediaCategory  eCategory = EMediaCategory::Image;
    QString         sSourceName;        // Anzeigename des Quellformats, z.B. "PNG" (= Zeile im Dialog)
    QStringList     aSourceExts;        // Quell-Endungen, kleingeschrieben, ohne Punkt
    QString         sTargetLabel;       // Eintrag in der DropDown-Liste, z.B. "JXL (Q96)"
    QString         sTargetExt;         // Ziel-Endung, kleingeschrieben, ohne Punkt
    bool            bLossy = true;      // Qualitätsverlust -> rote Schrift + Sicherheitsabfrage (Standard: sicherheitshalber verlustbehaftet)
    QString         sProgram;           // auszuführendes Programm (wird im PATH gesucht)
    std::function<QStringList(const QString& sSource, const QString& sTarget)> fnArgs;
    // Optionale Vorab-Prüfung der Quelldatei: leerer String = OK, sonst Grund fürs Überspringen
    std::function<QString(const QString& sSource)> fnCheckSource = {};
    // Zusätzlich benötigte Programme (z.B. für fnCheckSource)
    QStringList     aHelperPrograms = {};

    bool AcceptsSuffix(const QString& sSuffix) const { return aSourceExts.contains(sSuffix.toLower()); }
    QString TargetPathFor(const QString& sSourcePath) const;
};

// Alle bekannten Konvertierungen; gruppiert nach Kategorie und Quellformat, innerhalb
// eines Quellformats in der Reihenfolge der DropDown-Liste (verlustfreie Ziele zuerst).
const QList<SConversion>& AllConversions();

// Byte-Angabe in lesbarer Form (Ersatz für String_AutoByteFormat() aus dem AHK-Script)
QString FormatBytes(qint64 nBytes);

#endif // CONVERSIONS_H

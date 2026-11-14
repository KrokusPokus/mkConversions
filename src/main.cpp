#include "ConversionDlg.h"

#include <QApplication>
#include <QCollator>
#include <QCommandLineParser>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSet>
#include <QTranslator>

#include <cstdio>

namespace {

// Übergebene Pfade auflösen: Dateien direkt, Ordner werden nach Dateien durchsucht
// (wie "loop,files,%param%\*.*" im AHK-Script; mit --recursive auch Unterordner wie in den Bash-Scripts).
QStringList CollectFiles(const QStringList& aArgs, bool bRecursive)
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

    for (const QString& sArg : aArgs) {
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

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("mkConversions"));
    QApplication::setApplicationVersion(QStringLiteral(MKCONVERSIONS_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("mkConversions"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/mkConversions.svg")));

    // Übersetzungen: eigene + Qt (für Standard-Buttons wie Ja/Nein)
    QTranslator translatorQt;
    if (translatorQt.load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&translatorQt);

    QTranslator translator;
    if (translator.load(QLocale::system(), QStringLiteral("mkConversions"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Converts image, audio and video files between formats."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption optRecursive({ QStringLiteral("r"), QStringLiteral("recursive") },
                                          QCoreApplication::translate("main", "Also search subfolders of the given folders."));
    parser.addOption(optRecursive);
    parser.addPositionalArgument(QStringLiteral("paths"), QCoreApplication::translate("main", "Files and/or folders to convert."),
                                 QStringLiteral("<paths...>"));
    parser.process(app);

    // Ohne Argumente gibt es nichts zu tun (ExitApp im Original)
    const QStringList aArgs = parser.positionalArguments();
    if (aArgs.isEmpty()) {
        std::fputs(qPrintable(parser.helpText()), stderr);
        return 1;
    }

    const QStringList aFiles = CollectFiles(aArgs, parser.isSet(optRecursive));

    CConversionDlg dlg(aFiles);
    if (!dlg.HasAnyConversion()) {
        QMessageBox::information(nullptr, QCoreApplication::translate("main", "Conversions"),
                                 QCoreApplication::translate("main", "None of the given files can be converted."));
        return 1;
    }

    dlg.exec();
    return 0;
}

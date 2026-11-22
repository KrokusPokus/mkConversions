#include "ConversionDlg.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTranslator>

#include <cstdio>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("mkConversions"));
    QApplication::setApplicationVersion(QStringLiteral(MKCONVERSIONS_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("mkConversions"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/mkConversions.svg")));

    // Translations: our own + Qt's (for standard buttons like Yes/No)
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
                                          QCoreApplication::translate("main", "Also search subfolders of the given folders (initial state of the checkbox)."));
    parser.addOption(optRecursive);
    parser.addPositionalArgument(QStringLiteral("paths"), QCoreApplication::translate("main", "Files and/or folders to convert."),
                                 QStringLiteral("<paths...>"));
    parser.process(app);

    // Without arguments there is nothing to do
    const QStringList aArgs = parser.positionalArguments();
    if (aArgs.isEmpty()) {
        std::fputs(qPrintable(parser.helpText()), stderr);
        return 1;
    }

    // The dialog collects the files itself (the "Include subfolders" checkbox can re-read the folders).
    // --recursive only sets the initial state of that checkbox.
    CConversionDlg dlg(aArgs, parser.isSet(optRecursive));
    if (!dlg.HasAnyConversion() && !dlg.CanToggleRecursive()) {
        QMessageBox::information(nullptr, QCoreApplication::translate("main", "Conversions"),
                                 QCoreApplication::translate("main", "None of the given files can be converted."));
        return 1;
    }

    dlg.exec();
    return 0;
}

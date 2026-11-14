#ifndef CONVERTER_H
#define CONVERTER_H

#include "Conversions.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

// Arbeitet die Dateiliste asynchron ab (entspricht DoConversion() im AHK-Script).
// Pro Datei wird genau ein externer Prozess gestartet; Pause/Abbruch greifen
// jeweils zwischen zwei Dateien bzw. beenden den laufenden Prozess sauber.
class CConverter : public QObject
{
    Q_OBJECT

public:
    struct SStats {
        int    nFilesFound     = 0;
        int    nFilesProcessed = 0;
        qint64 nSizeDeleted    = 0;
        qint64 nSizeCreated    = 0;
    };

    CConverter(const QStringList& aFiles, const SConversion& conversion, QObject* pParent = nullptr);
    ~CConverter() override;

    void Start();
    void RequestPause();    // wirkt, sobald die aktuelle Datei fertig ist
    void Resume();
    void Abort();           // laufenden Prozess beenden, halbfertige Zieldatei löschen

    const SStats&      Stats()  const { return m_stats; }
    const QStringList& Errors() const { return m_aErrors; }

signals:
    void CurrentFileChanged(const QString& sFileName);
    void StatsChanged();
    void Paused();
    void Finished();

private:
    void ProcessNext();
    void OnProcessFinished(int nExitCode, QProcess::ExitStatus eStatus);
    void OnProcessError(QProcess::ProcessError eError);
    void CompleteCurrent(bool bSuccess, const QString& sDetails);
    void AddError(const QString& sMessage);
    QString TailOfOutput() const;

    QStringList       m_aFiles;
    const SConversion m_conversion;
    SStats            m_stats;
    QStringList       m_aErrors;

    qsizetype         m_nIndex = 0;
    QProcess*         m_pProcess = nullptr;
    QString           m_sCurrentSource;
    QString           m_sCurrentTarget;
    QByteArray        m_baOutput;

    bool              m_bPauseRequested = false;
    bool              m_bAborted = false;
    bool              m_bFinished = false;
};

#endif // CONVERTER_H

#include "Converter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

namespace {
constexpr qsizetype kMaxOutputBytes = 64 * 1024;   // nur das Ende der Prozessausgabe aufheben
constexpr int       kMaxOutputLines = 5;           // Zeilen davon im Fehlerbericht
constexpr int       kKillTimeoutMs  = 3000;

bool PathOccupied(const QString& sPath)
{
    const QFileInfo fi(sPath);
    return fi.exists() || fi.isSymLink();   // auch "kaputte" Symlinks nicht überschreiben
}
} // namespace

CConverter::CConverter(const QStringList& aFiles, const SConversion& conversion, QObject* pParent)
    : QObject(pParent)
    , m_aFiles(aFiles)
    , m_conversion(conversion)
{
}

CConverter::~CConverter()
{
    Abort();
}

void CConverter::Start()
{
    m_stats = {};
    for (const QString& sPath : std::as_const(m_aFiles)) {
        if (m_conversion.AcceptsSuffix(QFileInfo(sPath).suffix()))
            ++m_stats.nFilesFound;
    }
    emit StatsChanged();

    // Erst zurück in die Ereignisschleife, damit der Dialog seinen Zustand vollständig setzen kann
    QTimer::singleShot(0, this, &CConverter::ProcessNext);
}

void CConverter::RequestPause()
{
    if (!m_bFinished && !m_bAborted)
        m_bPauseRequested = true;
}

void CConverter::Resume()
{
    if (!m_bPauseRequested || m_pProcess)
        return;
    m_bPauseRequested = false;
    ProcessNext();
}

void CConverter::Abort()
{
    if (m_bAborted)
        return;
    m_bAborted = true;

    if (m_pProcess) {
        m_pProcess->disconnect(this);
        m_pProcess->kill();
        m_pProcess->waitForFinished(kKillTimeoutMs);
        delete m_pProcess;
        m_pProcess = nullptr;

        // Die Zieldatei existierte vor dem Start nachweislich nicht -> Fragment entfernen
        if (!m_sCurrentTarget.isEmpty() && PathOccupied(m_sCurrentTarget))
            QFile::remove(m_sCurrentTarget);
    }
}

void CConverter::ProcessNext()
{
    if (m_bAborted || m_bFinished || m_pProcess)
        return;

    if (m_bPauseRequested) {
        emit Paused();
        return;
    }

    while (m_nIndex < m_aFiles.size()) {
        const QString sSource = m_aFiles.at(m_nIndex++);
        const QFileInfo fiSource(sSource);

        if (!m_conversion.AcceptsSuffix(fiSource.suffix()))
            continue;

        emit CurrentFileChanged(fiSource.fileName());

        const QString sTarget = m_conversion.TargetPathFor(sSource);
        if (PathOccupied(sTarget)) {
            AddError(tr("'%1': Target file '%2' already exists (skipped).")
                         .arg(fiSource.fileName(), QFileInfo(sTarget).fileName()));
            continue;
        }

        if (!fiSource.isFile()) {
            AddError(tr("File '%1' does not exist.").arg(sSource));
            continue;
        }

        if (m_conversion.fnCheckSource) {
            const QString sReason = m_conversion.fnCheckSource(sSource);
            if (!sReason.isEmpty()) {
                AddError(tr("'%1' skipped: %2").arg(fiSource.fileName(), sReason));
                continue;
            }
        }

        m_sCurrentSource = sSource;
        m_sCurrentTarget = sTarget;
        m_baOutput.clear();

        m_pProcess = new QProcess(this);
        m_pProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_pProcess->setStandardInputFile(QProcess::nullDevice());
        connect(m_pProcess, &QProcess::readyRead, this, [this]() {
            m_baOutput.append(m_pProcess->readAll());
            if (m_baOutput.size() > kMaxOutputBytes)
                m_baOutput.remove(0, m_baOutput.size() - kMaxOutputBytes);
        });
        connect(m_pProcess, &QProcess::finished, this, &CConverter::OnProcessFinished);
        connect(m_pProcess, &QProcess::errorOccurred, this, &CConverter::OnProcessError);
        m_pProcess->start(m_conversion.sProgram, m_conversion.fnArgs(sSource, sTarget));
        return;     // weiter geht's in OnProcessFinished()/OnProcessError()
    }

    m_bFinished = true;
    m_sCurrentSource.clear();
    m_sCurrentTarget.clear();
    emit Finished();
}

void CConverter::OnProcessFinished(int nExitCode, QProcess::ExitStatus eStatus)
{
    if (!m_pProcess)
        return;
    m_baOutput.append(m_pProcess->readAll());

    const QFileInfo fiTarget(m_sCurrentTarget);
    const bool bOk = (eStatus == QProcess::NormalExit) && (nExitCode == 0)
                     && fiTarget.isFile() && fiTarget.size() > 0;

    QString sDetails;
    if (!bOk) {
        if (eStatus != QProcess::NormalExit)
            sDetails = tr("%1 crashed.").arg(m_conversion.sProgram);
        else if (nExitCode != 0)
            sDetails = tr("%1 exited with code %2.").arg(m_conversion.sProgram).arg(nExitCode);
        else
            sDetails = tr("No (or an empty) target file was created.");
    }
    CompleteCurrent(bOk, sDetails);
}

void CConverter::OnProcessError(QProcess::ProcessError eError)
{
    // Alle anderen Fehlerarten enden ebenfalls in finished() und werden dort behandelt
    if (eError == QProcess::FailedToStart && m_pProcess)
        CompleteCurrent(false, tr("%1 could not be started: %2").arg(m_conversion.sProgram, m_pProcess->errorString()));
}

void CConverter::CompleteCurrent(bool bSuccess, const QString& sDetails)
{
    m_pProcess->disconnect(this);
    m_pProcess->deleteLater();
    m_pProcess = nullptr;

    if (m_bAborted)
        return;

    const QFileInfo fiSource(m_sCurrentSource);
    const QString sName = fiSource.fileName();

    if (bSuccess) {
        ++m_stats.nFilesProcessed;
        m_stats.nSizeCreated += QFileInfo(m_sCurrentTarget).size();

        // Keine Fehler -> Quelldatei löschen
        const qint64 nSourceSize = fiSource.size();
        QFile fSource(m_sCurrentSource);
        if (fSource.remove())
            m_stats.nSizeDeleted += nSourceSize;
        else
            AddError(tr("Could not delete '%1': %2").arg(m_sCurrentSource, fSource.errorString()));
    }
    else {
        // Beschädigte Zieldatei aufräumen, falls angelegt
        if (PathOccupied(m_sCurrentTarget))
            QFile::remove(m_sCurrentTarget);

        QString sMessage = tr("Conversion of '%1' failed.").arg(sName);
        if (!sDetails.isEmpty())
            sMessage += QLatin1Char(' ') + sDetails;
        const QString sTail = TailOfOutput();
        if (!sTail.isEmpty())
            sMessage += QLatin1Char('\n') + sTail;
        AddError(sMessage);
    }

    m_sCurrentSource.clear();
    m_sCurrentTarget.clear();
    emit StatsChanged();
    ProcessNext();
}

void CConverter::AddError(const QString& sMessage)
{
    m_aErrors.append(QStringLiteral("• ") + sMessage);
}

QString CConverter::TailOfOutput() const
{
    static const QRegularExpression s_reLineBreak(QStringLiteral("[\r\n]+"));

    QStringList aLines;
    const QStringList aAll = QString::fromLocal8Bit(m_baOutput).split(s_reLineBreak, Qt::SkipEmptyParts);
    for (qsizetype i = aAll.size() - 1; i >= 0 && aLines.size() < kMaxOutputLines; --i) {
        const QString sLine = aAll.at(i).trimmed();
        if (!sLine.isEmpty())
            aLines.prepend(QStringLiteral("    ") + sLine);
    }
    return aLines.join(QLatin1Char('\n'));
}

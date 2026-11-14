#ifndef CONVERSIONDLG_H
#define CONVERSIONDLG_H

#include <QColor>
#include <QDialog>
#include <QPalette>
#include <QHash>
#include <QStringList>

class CConverter;
class CElidedLabel;
class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
struct SConversion;

// Main dialog:
//  - one column per media category present in the given files
//  - one row per source format present in the files: file count + radio button ("PNG") + arrow
//    + drop-down list with all target formats of the category (red if quality is lost)
//  - Start/Pause/Continue/Close button in the top right corner
//  - two status lines (current file, progress and size balance)
class CConversionDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CConversionDlg(const QStringList& aFiles, QWidget* pParent = nullptr);
    ~CConversionDlg() override;

    // true if at least one column is shown (otherwise there is nothing to do)
    bool HasAnyConversion() const { return !m_aRows.isEmpty(); }

public slots:
    void reject() override;

protected:
    bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

private:
    enum class EState {
        Idle,               // mode not selected yet
        Running,            // conversion running
        PauseRequested,     // pause requested, current file still running
        Paused,             // paused between two files
        Finished            // done -> button closes the dialog
    };

    void BuildUi(const QHash<QString, int>& aSuffixCounts);
    void OnStartPause();
    bool StartConversion();
    void OnStatsChanged();
    void OnPaused();
    void OnFinished();
    void SetButton(const QString& sText, bool bEnabled);
    void UpdateRowColor(qsizetype nRow);
    void RestoreLastTargets();
    void SaveLastTarget(const SConversion& conv);
    const SConversion* SelectedConversion() const;

    // One row in the dialog = one source format
    struct SRow {
        QLabel*       pCount = nullptr;      // "(123 files)", right-aligned
        QRadioButton* pRadio = nullptr;
        QLabel*       pArrow = nullptr;      // separate label so the arrow can be drawn larger
        QComboBox*    pTargets = nullptr;    // item data: index into AllConversions()
    };

    QStringList            m_aFiles;
    EState                 m_eState = EState::Idle;
    CConverter*            m_pConverter = nullptr;

    QButtonGroup*          m_pRadioGroup = nullptr;
    QList<SRow>            m_aRows;
    QPalette               m_paletteNormal;
    QPalette               m_paletteLossy;
    QColor                 m_colLossy;
    QPushButton*           m_pStartPauseButton = nullptr;
    CElidedLabel*          m_pInfoLine1 = nullptr;
    CElidedLabel*          m_pInfoLine2 = nullptr;
};

#endif // CONVERSIONDLG_H

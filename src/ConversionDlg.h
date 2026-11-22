#ifndef CONVERSIONDLG_H
#define CONVERSIONDLG_H

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QPalette>
#include <QHash>
#include <QStringList>

class CConverter;
class CElidedLabel;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QRadioButton;
struct SConversion;

// Main dialog:
//  - one column per media category present in the given files
//  - one row per source format present in the files: file count + radio button ("PNG") + arrow
//    + drop-down list with all target formats of the category (red if quality is lost)
//  - "Include subfolders" checkbox (only if a folder was given): re-reads the folders and rebuilds the columns
//  - two status lines (current file, progress and size balance)
//  - Start/Pause/Continue/Close button in the bottom right corner
class CConversionDlg : public QDialog
{
    Q_OBJECT

public:
    // aPaths: files and/or folders as given on the command line
    CConversionDlg(const QStringList& aPaths, bool bRecursive, QWidget* pParent = nullptr);
    ~CConversionDlg() override;

    // true if at least one column is shown (otherwise there is nothing to do)
    bool HasAnyConversion() const { return !m_aRows.isEmpty(); }
    // true if a folder was given, i.e. the "Include subfolders" checkbox is shown
    bool CanToggleRecursive() const;

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

    void BuildUi(bool bRecursive);
    void BuildColumns();
    void OnRecursiveToggled(bool bRecursive);
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

    QStringList            m_aPaths;              // as given on the command line
    QStringList            m_aFiles;              // collected from m_aPaths
    EState                 m_eState = EState::Idle;
    CConverter*            m_pConverter = nullptr;

    QFont                  m_fontHeading;
    QFont                  m_fontFixed;
    QFont                  m_fontArrow;
    QHBoxLayout*           m_pTopLayout = nullptr;
    QWidget*               m_pColumns = nullptr;  // all columns; recreated by BuildColumns()
    QCheckBox*             m_pRecursiveCheck = nullptr;
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

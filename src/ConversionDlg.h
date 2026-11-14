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

// Hauptdialog (entspricht "Gui,2" im AHK-Script):
//  - eine Spalte je Medienkategorie, die in den übergebenen Dateien vorkommt
//  - je Quellformat, das in den Dateien vorkommt, eine Zeile: Dateianzahl + Radio-Button ("PNG") + Pfeil
//    + DropDown-Liste mit allen Zielformaten der Kategorie (rot, wenn Qualität verloren geht)
//  - Start/Pause/Continue/Close-Button oben rechts
//  - zwei Statuszeilen (aktuelle Datei, Fortschritt und Größenbilanz)
class CConversionDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CConversionDlg(const QStringList& aFiles, QWidget* pParent = nullptr);
    ~CConversionDlg() override;

    // true, wenn mindestens eine Spalte angezeigt wird (sonst gibt es nichts zu tun)
    bool HasAnyConversion() const { return !m_aRows.isEmpty(); }

public slots:
    void reject() override;

protected:
    bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

private:
    enum class EState {
        Idle,               // Modus noch nicht gewählt
        Running,            // Konvertierung läuft
        PauseRequested,     // Pause angefordert, aktuelle Datei läuft noch
        Paused,             // pausiert zwischen zwei Dateien
        Finished            // fertig -> Button schließt den Dialog
    };

    void BuildUi(const QHash<QString, int>& aSuffixCounts);
    void OnStartPause();
    bool StartConversion();
    void OnStatsChanged();
    void OnPaused();
    void OnFinished();
    void SetButton(const QString& sText, bool bEnabled);
    void UpdateRowColor(qsizetype nRow);
    const SConversion* SelectedConversion() const;

    // Eine Zeile im Dialog = ein Quellformat
    struct SRow {
        QLabel*       pCount = nullptr;      // "(123 files)", rechtsbündig
        QRadioButton* pRadio = nullptr;
        QLabel*       pArrow = nullptr;      // eigenes Label, damit der Pfeil größer dargestellt werden kann
        QComboBox*    pTargets = nullptr;    // Item-Daten: Index in AllConversions()
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

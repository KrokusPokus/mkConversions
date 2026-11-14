#include "ConversionDlg.h"

#include "Conversions.h"
#include "Converter.h"
#include "ElidedLabel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <array>

namespace {
constexpr int kMargin            = 20;
constexpr int kColumnMinWidth    = 150;
constexpr int kRadioSpacing      = 10;
constexpr int kGroupSpacing      = 20;
constexpr int kStatusMinWidth    = 428;
constexpr int kButtonWidth       = 80;
constexpr int kButtonHeight      = 40;

// Arrow between source format and target list. At its original size "→" looks too small next to the text;
// at 150 % its stroke width roughly matches that of the font.
const QString     kArrow         = QStringLiteral("\u2192");
constexpr double  kArrowScale    = 1.5;

QColor LossyTextColor(const QPalette& palette)
{
    // Pure red is hard to read on a dark background
    return palette.color(QPalette::Window).lightness() < 128 ? QColor(0xff, 0x6b, 0x6b) : QColor(0xd0, 0x00, 0x00);
}
} // namespace

CConversionDlg::CConversionDlg(const QStringList& aFiles, QWidget* pParent)
    : QDialog(pParent)
    , m_aFiles(aFiles)
{
    setWindowTitle(tr("Format Conversion"));
    setWindowFlag(Qt::WindowMinimizeButtonHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    // Determine the existing extensions
    QHash<QString, int> aSuffixCounts;
    for (const QString& sPath : aFiles)
        ++aSuffixCounts[QFileInfo(sPath).suffix().toLower()];

    BuildUi(aSuffixCounts);
}

CConversionDlg::~CConversionDlg() = default;

void CConversionDlg::BuildUi(const QHash<QString, int>& aSuffixCounts)
{
    auto* pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    pMainLayout->setSizeConstraint(QLayout::SetFixedSize);   // not resizable

    auto* pTopLayout = new QHBoxLayout();
    pTopLayout->setSpacing(0);
    pMainLayout->addLayout(pTopLayout);

    // Fonts: headings bold and slightly larger, format rows in a fixed-width font
    QFont fontHeading = font();
    fontHeading.setBold(true);
    if (fontHeading.pointSizeF() > 0)
        fontHeading.setPointSizeF(fontHeading.pointSizeF() + 1.0);
    const QFont fontFixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QFont fontArrow = fontFixed;
    if (fontArrow.pointSizeF() > 0)
        fontArrow.setPointSizeF(fontArrow.pointSizeF() * kArrowScale);
    else
        fontArrow.setPixelSize(qRound(fontArrow.pixelSize() * kArrowScale));

    m_paletteNormal = palette();
    m_paletteLossy  = palette();
    m_colLossy = LossyTextColor(m_paletteLossy);
    for (QPalette::ColorGroup eGroup : { QPalette::Active, QPalette::Inactive }) {
        m_paletteLossy.setColor(eGroup, QPalette::WindowText, m_colLossy);
        m_paletteLossy.setColor(eGroup, QPalette::ButtonText, m_colLossy);
        m_paletteLossy.setColor(eGroup, QPalette::Text, m_colLossy);
    }

    m_pRadioGroup = new QButtonGroup(this);
    m_pRadioGroup->setExclusive(true);

    const std::array<EMediaCategory, 3> aCategories = { EMediaCategory::Image, EMediaCategory::Audio, EMediaCategory::Video };
    const QList<SConversion>& aConversions = AllConversions();

    // Number of given files per source format (e.g. .jpg + .jpeg)
    auto sourceCount = [&aSuffixCounts](const SConversion& conv) {
        int nCount = 0;
        for (const QString& sExt : conv.aSourceExts)
            nCount += aSuffixCounts.value(sExt, 0);
        return nCount;
    };
    auto sourcePresent = [&sourceCount](const SConversion& conv) { return sourceCount(conv) > 0; };

    for (EMediaCategory eCategory : aCategories) {
        // Show the column only if at least one matching source format is present
        bool bCategoryPresent = false;
        for (const SConversion& conv : aConversions)
            bCategoryPresent = bCategoryPresent || (conv.eCategory == eCategory && sourcePresent(conv));
        if (!bCategoryPresent)
            continue;

        auto* pColumn = new QWidget(this);
        pColumn->setMinimumWidth(kColumnMinWidth);
        auto* pColumnLayout = new QVBoxLayout(pColumn);
        pColumnLayout->setContentsMargins(0, 0, kMargin, 0);
        pColumnLayout->setSpacing(0);

        QString sHeading;
        switch (eCategory) {
            case EMediaCategory::Image: sHeading = tr("Image formats"); break;
            case EMediaCategory::Audio: sHeading = tr("Audio formats"); break;
            case EMediaCategory::Video: sHeading = tr("Video formats"); break;
        }
        auto* pHeading = new QLabel(sHeading, pColumn);
        pHeading->setFont(fontHeading);
        pColumnLayout->addWidget(pHeading);
        pColumnLayout->addSpacing(kRadioSpacing + 4);

        auto* pGrid = new QGridLayout();
        pGrid->setContentsMargins(0, 0, 0, 0);
        pGrid->setHorizontalSpacing(6);
        pGrid->setVerticalSpacing(kRadioSpacing);
        pColumnLayout->addLayout(pGrid);

        // Show only source formats that occur in the given files.
        // AllConversions() is grouped by source format -> new row on every change.
        QString sCurrentSource;
        for (qsizetype i = 0; i < aConversions.size(); ++i) {
            const SConversion& conv = aConversions.at(i);
            if (conv.eCategory != eCategory || !sourcePresent(conv))
                continue;

            if (conv.sSourceName != sCurrentSource) {
                sCurrentSource = conv.sSourceName;

                SRow row;
                row.pCount = new QLabel(tr("(%n file(s))", nullptr, sourceCount(conv)), pColumn);
                row.pCount->setFont(fontFixed);
                row.pCount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                row.pCount->installEventFilter(this);   // a click selects the row
                row.pRadio = new QRadioButton(conv.sSourceName.leftJustified(4), pColumn);
                row.pRadio->setFont(fontFixed);
                row.pArrow = new QLabel(kArrow, pColumn);
                row.pArrow->setFont(fontArrow);
                row.pArrow->installEventFilter(this);   // a click on the arrow selects the row
                row.pTargets = new QComboBox(pColumn);
                row.pTargets->setFont(fontFixed);
                row.pTargets->setSizeAdjustPolicy(QComboBox::AdjustToContents);

                const auto nRow = m_aRows.size();
                m_pRadioGroup->addButton(row.pRadio, static_cast<int>(nRow));
                pGrid->addWidget(row.pCount, static_cast<int>(nRow), 0);
                pGrid->addWidget(row.pRadio, static_cast<int>(nRow), 1);
                pGrid->addWidget(row.pArrow, static_cast<int>(nRow), 2, Qt::AlignCenter);
                pGrid->addWidget(row.pTargets, static_cast<int>(nRow), 3);

                // Selecting an item in the list also selects the row
                connect(row.pTargets, &QComboBox::activated, row.pRadio, [pRadio = row.pRadio]() { pRadio->setChecked(true); });
                connect(row.pTargets, &QComboBox::currentIndexChanged, this, [this, nRow]() { UpdateRowColor(nRow); });
                m_aRows.append(row);
            }

            QComboBox* pTargets = m_aRows.last().pTargets;
            pTargets->addItem(conv.sTargetLabel, static_cast<int>(i));
            if (conv.bLossy) {
                const int nItem = pTargets->count() - 1;
                pTargets->setItemData(nItem, m_colLossy, Qt::ForegroundRole);
                pTargets->setItemData(nItem, tr("Lossy: quality will be lost."), Qt::ToolTipRole);
            }
        }
        for (qsizetype nRow = 0; nRow < m_aRows.size(); ++nRow)
            UpdateRowColor(nRow);

        pColumnLayout->addStretch(1);
        pTopLayout->addWidget(pColumn, 0, Qt::AlignTop);
    }
    pTopLayout->addStretch(1);

    m_pStartPauseButton = new QPushButton(tr("Start"), this);
    m_pStartPauseButton->setFont(fontHeading);
    m_pStartPauseButton->setFixedSize(kButtonWidth, kButtonHeight);
    m_pStartPauseButton->setDefault(true);
    connect(m_pStartPauseButton, &QPushButton::clicked, this, &CConversionDlg::OnStartPause);
    pTopLayout->addWidget(m_pStartPauseButton, 0, Qt::AlignTop | Qt::AlignRight);

    // Status lines
    pMainLayout->addSpacing(kGroupSpacing);
    m_pInfoLine1 = new CElidedLabel(this);
    m_pInfoLine2 = new CElidedLabel(this);
    m_pInfoLine1->setMinimumWidth(kStatusMinWidth);
    m_pInfoLine2->setMinimumWidth(kStatusMinWidth);
    pMainLayout->addWidget(m_pInfoLine1);
    pMainLayout->addSpacing(kRadioSpacing);
    pMainLayout->addWidget(m_pInfoLine2);
}

// Show the row in red if the selected target is lossy
void CConversionDlg::UpdateRowColor(qsizetype nRow)
{
    const SRow& row = m_aRows.at(nRow);
    const int nConv = row.pTargets->currentData().toInt();
    const bool bLossy = row.pTargets->currentIndex() >= 0 && AllConversions().at(nConv).bLossy;
    const QPalette& pal = bLossy ? m_paletteLossy : m_paletteNormal;
    row.pRadio->setPalette(pal);
    row.pArrow->setPalette(pal);
    row.pTargets->setPalette(pal);
    row.pTargets->setToolTip(bLossy ? tr("Lossy: quality will be lost.") : QString());
}

const SConversion* CConversionDlg::SelectedConversion() const
{
    const int nRow = m_pRadioGroup->checkedId();
    if (nRow < 0 || nRow >= m_aRows.size())
        return nullptr;
    const QComboBox* pTargets = m_aRows.at(nRow).pTargets;
    if (pTargets->currentIndex() < 0)
        return nullptr;
    return &AllConversions().at(pTargets->currentData().toInt());
}

void CConversionDlg::SetButton(const QString& sText, bool bEnabled)
{
    m_pStartPauseButton->setText(sText);
    m_pStartPauseButton->setEnabled(bEnabled);
}

void CConversionDlg::OnStartPause()
{
    switch (m_eState) {
        case EState::Idle:
            StartConversion();
            break;

        case EState::Running:          // request pause; takes effect after the current file
            m_eState = EState::PauseRequested;
            m_pConverter->RequestPause();
            m_pStartPauseButton->setEnabled(false);
            break;

        case EState::Paused:           // resume
            m_eState = EState::Running;
            SetButton(tr("Pause"), true);
            m_pConverter->Resume();
            break;

        case EState::Finished:
            accept();
            break;

        case EState::PauseRequested:   // button is disabled
            break;
    }
}

bool CConversionDlg::StartConversion()
{
    const SConversion* pConv = SelectedConversion();
    if (!pConv) {
        QMessageBox::warning(this, tr("Conversions Error"), tr("Please select a conversion mode first!"));
        return false;
    }
    const SConversion& conv = *pConv;

    // Check dependencies
    QStringList aMissing;
    for (const QString& sProgram : QStringList{ conv.sProgram } + conv.aHelperPrograms) {
        if (QStandardPaths::findExecutable(sProgram).isEmpty())
            aMissing.append(sProgram);
    }
    if (!aMissing.isEmpty()) {
        QMessageBox::critical(this, tr("Conversions Error"),
                              tr("The following required programs are missing on the system:\n\n%1").arg(aMissing.join(QLatin1Char(' '))));
        return false;
    }

    if (conv.bLossy) {
        const auto eAnswer = QMessageBox::question(this, tr("Conversions"),
                                                   tr("The selected conversion mode loses quality!\nReally continue?"),
                                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (eAnswer != QMessageBox::Yes)
            return false;
    }

    for (const SRow& row : std::as_const(m_aRows)) {
        row.pRadio->setEnabled(false);
        row.pArrow->setEnabled(false);
        row.pTargets->setEnabled(false);
    }

    m_pConverter = new CConverter(m_aFiles, conv, this);
    connect(m_pConverter, &CConverter::CurrentFileChanged, m_pInfoLine1, &CElidedLabel::SetFullText);
    connect(m_pConverter, &CConverter::StatsChanged, this, &CConversionDlg::OnStatsChanged);
    connect(m_pConverter, &CConverter::Paused, this, &CConversionDlg::OnPaused);
    connect(m_pConverter, &CConverter::Finished, this, &CConversionDlg::OnFinished);

    m_eState = EState::Running;
    SetButton(tr("Pause"), true);
    m_pConverter->Start();
    return true;
}

void CConversionDlg::OnStatsChanged()
{
    const CConverter::SStats& st = m_pConverter->Stats();
    const qint64 nDelta = st.nSizeCreated - st.nSizeDeleted;
    const QString sDelta = (nDelta >= 0 ? QStringLiteral("+") : QString()) + FormatBytes(nDelta);

    m_pInfoLine2->SetFullText(tr("%1 of %2 converted. -%3 + %4 = %5")
                                  .arg(st.nFilesProcessed)
                                  .arg(st.nFilesFound)
                                  .arg(FormatBytes(st.nSizeDeleted), FormatBytes(st.nSizeCreated), sDelta));
}

void CConversionDlg::OnPaused()
{
    m_eState = EState::Paused;
    SetButton(tr("Continue"), true);
}

void CConversionDlg::OnFinished()
{
    m_eState = EState::Finished;
    m_pInfoLine1->SetFullText(QString());
    OnStatsChanged();
    SetButton(tr("Close"), true);
    m_pStartPauseButton->setFocus();

    const QStringList& aErrors = m_pConverter->Errors();
    if (!aErrors.isEmpty()) {
        QMessageBox box(QMessageBox::Warning, tr("Conversion report"),
                        tr("Processing finished (%1 successful).\n%n error(s)/note(s) occurred:", nullptr, aErrors.size())
                            .arg(m_pConverter->Stats().nFilesProcessed),
                        QMessageBox::Ok, this);
        // Show a few messages directly, otherwise via "Show Details..."
        constexpr qsizetype kMaxInlineErrors = 5;
        if (aErrors.size() <= kMaxInlineErrors)
            box.setInformativeText(aErrors.join(QLatin1Char('\n')));
        box.setDetailedText(aErrors.join(QLatin1Char('\n')));
        box.exec();
    }
}

bool CConversionDlg::eventFilter(QObject* pWatched, QEvent* pEvent)
{
    if (pEvent->type() == QEvent::MouseButtonPress) {
        for (const SRow& row : std::as_const(m_aRows)) {
            if ((row.pArrow == pWatched || row.pCount == pWatched) && row.pRadio->isEnabled()) {
                row.pRadio->setChecked(true);
                row.pRadio->setFocus();
                return true;
            }
        }
    }
    return QDialog::eventFilter(pWatched, pEvent);
}

void CConversionDlg::reject()
{
    // Escape / closing the window: a running conversion is aborted
    // (the current process is terminated, the unfinished target file deleted; the source file is kept).
    if (m_pConverter && m_eState != EState::Finished)
        m_pConverter->Abort();
    QDialog::reject();
}

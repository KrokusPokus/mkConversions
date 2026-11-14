#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>

// Single-line label that elides overly long texts (e.g. file names) in the middle with "…"
// instead of widening the dialog. The full text is shown in the tooltip.
class CElidedLabel : public QLabel
{
public:
    explicit CElidedLabel(QWidget* pParent = nullptr);

    void SetFullText(const QString& sText);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* pEvent) override;

private:
    QString m_sFullText;
};

#endif // ELIDEDLABEL_H

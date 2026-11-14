#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QLabel>

// Einzeiliges Label, das zu lange Texte (z.B. Dateinamen) mittig mit "…" kürzt,
// statt den Dialog zu verbreitern. Der vollständige Text steht im Tooltip.
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

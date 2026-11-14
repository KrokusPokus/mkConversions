#include "ElidedLabel.h"

#include <QPainter>
#include <QStyle>

CElidedLabel::CElidedLabel(QWidget* pParent)
    : QLabel(pParent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CElidedLabel::SetFullText(const QString& sText)
{
    if (sText == m_sFullText)
        return;
    m_sFullText = sText;
    setToolTip(sText);
    update();
}

QSize CElidedLabel::minimumSizeHint() const
{
    return { 0, fontMetrics().height() };
}

QSize CElidedLabel::sizeHint() const
{
    return minimumSizeHint();
}

void CElidedLabel::paintEvent(QPaintEvent* /*pEvent*/)
{
    QPainter painter(this);
    const QRect rc = contentsRect();
    const QString sElided = fontMetrics().elidedText(m_sFullText, Qt::ElideMiddle, rc.width());
    style()->drawItemText(&painter, rc, Qt::AlignLeft | Qt::AlignVCenter, palette(), isEnabled(), sElided, foregroundRole());
}

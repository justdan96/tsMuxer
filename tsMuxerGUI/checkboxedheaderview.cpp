#include "checkboxedheaderview.h"

#include <QPainter>

QnCheckBoxedHeaderView::QnCheckBoxedHeaderView(QWidget *parent)
    : base_type(Qt::Horizontal, parent), m_checkState(Qt::Unchecked), m_checkColumnIndex(0)
{
    connect(this, &QnCheckBoxedHeaderView::sectionClicked, this, &QnCheckBoxedHeaderView::at_sectionClicked);
}

void QnCheckBoxedHeaderView::setCheckState(Qt::CheckState state)
{
    if (state == m_checkState)
        return;
    m_checkState = state;
    emit checkStateChanged(state);

    viewport()->update();
}

void QnCheckBoxedHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    painter->save();
    base_type::paintSection(painter, rect, logicalIndex);
    painter->restore();

    if (logicalIndex == m_checkColumnIndex)
    {
        if (!rect.isValid())
            return;
        QStyleOptionButton opt;
        opt.initFrom(this);

        QStyle::State state = QStyle::State_Raised;
        if (isEnabled())
            state |= QStyle::State_Enabled;
        if (window()->isActiveWindow())
            state |= QStyle::State_Active;

        switch (m_checkState)
        {
        case Qt::Checked:
            state |= QStyle::State_On;
            break;
        case Qt::Unchecked:
            state |= QStyle::State_Off;
            break;
        default:
            state |= QStyle::State_NoChange;
            break;
        }

        opt.rect = rect.adjusted(4, 0, 0, 0);
        opt.state |= state;
        opt.text = QString();
        painter->save();
        style()->drawControl(QStyle::CE_CheckBox, &opt, painter, this);
        painter->restore();
    }
}

QSize QnCheckBoxedHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    QSize size = base_type::sectionSizeFromContents(logicalIndex);
    if (logicalIndex != m_checkColumnIndex)
        return size;
    size.setWidth(15);
    return size;
}

void QnCheckBoxedHeaderView::at_sectionClicked(int logicalIndex)
{
    if (logicalIndex != m_checkColumnIndex)
        return;
    if (m_checkState != Qt::Checked)
        setCheckState(Qt::Checked);
    else
        setCheckState(Qt::Unchecked);
}

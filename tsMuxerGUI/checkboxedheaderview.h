#ifndef CHECKBOXEDHEADERVIEW_H
#define CHECKBOXEDHEADERVIEW_H

#include <QHeaderView>

class QnCheckBoxedHeaderView : public QHeaderView
{
    Q_OBJECT
    typedef QHeaderView base_type;

   public:
    explicit QnCheckBoxedHeaderView(QWidget *parent = nullptr);

    Qt::CheckState checkState() const { return m_checkState; }
    void setCheckState(Qt::CheckState state);
   signals:
    void checkStateChanged(Qt::CheckState state);

   protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;

   private:
    void at_sectionClicked(int logicalIndex);
    Qt::CheckState m_checkState;
    int m_checkColumnIndex;
};

#endif

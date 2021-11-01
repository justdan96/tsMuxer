#ifndef FONT_SETTINGS_TABLE_MODEL_H
#define FONT_SETTINGS_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QFont>

class FontSettingsTableModel : public QAbstractTableModel
{
   public:
    FontSettingsTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    quint32 color() const { return m_color; }
    const QFont &font() const { return m_font; }
    void setFont(const QFont &font);
    void setColor(quint32 color);

   private:
    QFont m_font = QFont{"Arial", 65};
    quint32 m_color = ~0;
};

#endif

#include "fontsettingstablemodel.h"

#include <QBrush>
#include <QColor>

namespace
{
constexpr int NUM_ROWS = 4;
constexpr int NUM_COLUMNS = 2;

QString fontOptions(const QFont &font)
{
    QString optStr;
    if (font.italic())
        optStr += "Italic";
    if (font.bold())
    {
        if (!optStr.isEmpty())
            optStr += ',';
        optStr += "Bold";
    }
    if (font.underline())
    {
        if (!optStr.isEmpty())
            optStr += ',';
        optStr += "Underline";
    }
    if (font.strikeOut())
    {
        if (!optStr.isEmpty())
            optStr += ',';
        optStr += "Strikeout";
    }
    return optStr;
}

int colorLight(QColor color) { return (0.257 * color.red()) + (0.504 * color.green()) + (0.098 * color.blue()) + 16; }
}  // namespace

int FontSettingsTableModel::rowCount(const QModelIndex &) const { return NUM_ROWS; }

int FontSettingsTableModel::columnCount(const QModelIndex &) const { return NUM_COLUMNS; }

QVariant FontSettingsTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    switch (role)
    {
    case Qt::BackgroundRole:
        if (index.row() == 2 && index.column() == 1)
        {
            return QBrush(m_color);
        }
        else
        {
            return QVariant();
        }

    case Qt::ForegroundRole:
        if (index.row() == 2 && index.column() == 1)
        {
            if (colorLight(QColor(m_color)) < 128)
            {
                return QBrush(QColor(255, 255, 255, 255));
            }
            else
            {
                return QBrush(QColor(0, 0, 0, 255));
            }
        }
        else
        {
            return QVariant();
        }

    case Qt::DisplayRole:
    {
        static const std::array<std::function<QVariant()>, 8> providers = {
            // column 0
            []() { return tr("Name:"); }, []() { return tr("Size:"); }, []() { return tr("Color:"); },
            []() { return tr("Options:"); },
            // column 1
            [this]() { return m_font.family(); }, [this]() { return m_font.pointSize(); },
            [this]() { return QString("0x%1").arg(m_color, 8, 16, QLatin1Char('0')); },
            [this]() { return fontOptions(m_font); }};
        return providers[index.column() * NUM_ROWS + index.row()]();
    }

    default:
        return QVariant();
    }
}

Qt::ItemFlags FontSettingsTableModel::flags(const QModelIndex &index) const
{
    return QAbstractTableModel::flags(index) & (~Qt::ItemIsEditable);
}

void FontSettingsTableModel::setFont(const QFont &font)
{
    m_font = font;
    emit dataChanged(index(0, 1), index(3, 1));
}

void FontSettingsTableModel::setColor(quint32 color)
{
    m_color = color;
    emit dataChanged(index(2, 1), index(2, 1));
}

#include <QColor>
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QPageLayout>
#include <QRect>

#include "fileutility.h"

namespace Backend::Utility
{

//! Open a file and check its extension
QSharedPointer<QFile> openFile(QString const& pathFile, QString const& expectedSuffix, QIODevice::OpenModeFlag const& mode)
{
    // Check if the output file has the correct extension
    QFileInfo info(pathFile);
    if (info.suffix() != expectedSuffix)
    {
        qWarning() << QObject::tr("Unknown extension was specified for the file: %1").arg(pathFile);
        return nullptr;
    }

    // Open the file for the specified mode
    QSharedPointer<QFile> pFile(new QFile(pathFile));
    if (!pFile->open(mode))
    {
        qWarning() << QObject::tr("Could not open the file: %1").arg(pathFile);
        return nullptr;
    }
    return pFile;
}

QJsonValue toJson(QUuid const& id)
{
    return id.toString();
}

QJsonValue toJson(QRect const& rect)
{
    QJsonObject obj;
    obj["x"] = rect.x();
    obj["y"] = rect.y();
    obj["width"] = rect.width();
    obj["height"] = rect.height();
    return obj;
}

QJsonValue toJson(QFont const& font)
{
    QJsonObject obj;
    obj["family"] = font.family();
    obj["pointSize"] = font.pointSize();
    obj["pixelSize"] = font.pixelSize();
    obj["bold"] = font.bold();
    obj["italic"] = font.italic();
    obj["underline"] = font.underline();
    obj["strikeOut"] = font.strikeOut();
    obj["weight"] = font.weight();
    obj["styleHint"] = (int) font.styleHint();
    return obj;
}

QJsonValue toJson(QPageLayout const& layout)
{
    QJsonObject obj;
    obj["pageSizeId"] = layout.pageSize().id();
    obj["orientation"] = (int) layout.orientation();
    obj["units"] = (int) layout.units();
    obj["mode"] = (int) layout.mode();
    obj["margins"] = Utility::toJson(layout.margins());
    return obj;
}

QJsonValue toJson(QMarginsF const& margins)
{
    QJsonObject obj;
    obj["left"] = margins.left();
    obj["top"] = margins.top();
    obj["right"] = margins.right();
    obj["bottom"] = margins.bottom();
    return obj;
}

QJsonValue toJson(Qt::Alignment const& align)
{
    return align.toInt();
}

QJsonValue toJson(QColor const& color)
{
    return color.name(QColor::HexArgb);
}

QJsonValue toJson(QPair<double, double> const& pair)
{
    QJsonObject obj;
    obj["first"] = pair.first;
    obj["second"] = pair.second;
    return obj;
}

QJsonValue toJson(QByteArray const& data)
{
    return QString::fromLatin1(data.toBase64());
}

QJsonValue toJson(QList<double> const& data)
{
    QVariantList vars(data.begin(), data.end());
    return QJsonArray::fromVariantList(vars);
}

QJsonValue toJson(QList<bool> const& data)
{
    QVariantList vars(data.begin(), data.end());
    return QJsonArray::fromVariantList(vars);
}

void fromJson(QUuid& id, QJsonValue const& obj)
{
    id = QUuid::fromString(obj.toString());
}

void fromJson(QRect& rect, QJsonValue const& obj)
{
    rect.setX(obj["x"].toInt());
    rect.setY(obj["y"].toInt());
    rect.setWidth(obj["width"].toInt());
    rect.setHeight(obj["height"].toInt());
}

void fromJson(QFont& font, QJsonValue const& obj)
{
    font.setFamily(obj["family"].toString());
    font.setPointSize(obj["pointSize"].toInt());
    int pixelSize = obj["pixelSize"].toInt();
    if (pixelSize > 0)
        font.setPixelSize(pixelSize);
    font.setBold(obj["bold"].toBool());
    font.setItalic(obj["italic"].toBool());
    font.setUnderline(obj["underline"].toBool());
    font.setStrikeOut(obj["strikeOut"].toBool());
    font.setWeight((QFont::Weight) obj["weight"].toInt());
    font.setStyleHint((QFont::StyleHint) obj["styleHint"].toInt());
}

void fromJson(QPageLayout& layout, QJsonValue const& obj)
{
    QPageSize size((QPageSize::PageSizeId) obj["pageSizeId"].toInt());
    layout.setPageSize(size);
    layout.setOrientation((QPageLayout::Orientation) obj["orientation"].toInt());
    layout.setUnits((QPageLayout::Unit) obj["units"].toInt());
    layout.setMode((QPageLayout::Mode) obj["mode"].toInt());
    QMarginsF margins;
    Utility::fromJson(margins, obj["margins"]);
    layout.setMargins(margins);
}

void fromJson(QMarginsF& margins, QJsonValue const& obj)
{
    margins.setLeft(obj["left"].toDouble());
    margins.setTop(obj["top"].toDouble());
    margins.setRight(obj["right"].toDouble());
    margins.setBottom(obj["bottom"].toDouble());
}

void fromJson(Qt::Alignment& align, QJsonValue const& obj)
{
    align = Qt::Alignment::fromInt(obj.toInt());
}

void fromJson(QColor& color, QJsonValue const& obj)
{
    color = QColor(obj.toString());
}

void fromJson(QPair<double, double>& pair, QJsonValue const& obj)
{
    pair.first = obj["first"].toDouble();
    pair.second = obj["second"].toDouble();
}

void fromJson(QByteArray& data, QJsonValue const& obj)
{
    data = QByteArray::fromBase64(obj.toString().toLatin1());
}

void fromJson(QStringList& data, QJsonValue const& obj)
{
    QVariantList vars = obj.toArray().toVariantList();
    int count = vars.size();
    data.resize(count);
    for (int i = 0; i != count; ++i)
        data[i] = vars[i].toString();
}

void fromJson(QList<double>& data, QJsonValue const& obj)
{
    QVariantList vars = obj.toArray().toVariantList();
    int count = vars.size();
    data.resize(count);
    for (int i = 0; i != count; ++i)
        data[i] = vars[i].toDouble();
}

void fromJson(QList<bool>& data, QJsonValue const& obj)
{
    QVariantList vars = obj.toArray().toVariantList();
    int count = vars.size();
    data.resize(count);
    for (int i = 0; i != count; ++i)
        data[i] = vars[i].toBool();
}
}


#ifndef FILEUTILITY_H
#define FILEUTILITY_H

#include <QDir>

QT_FORWARD_DECLARE_CLASS(QPageLayout)
QT_FORWARD_DECLARE_CLASS(QMarginsF)

namespace Backend::Utility
{

QSharedPointer<QFile> openFile(QString const& pathFile, QString const& expectedSuffix, QIODevice::OpenModeFlag const& mode);

//! Base case for combining a filepath
template<typename T>
QString combineFilePath(T const& value)
{
    return value;
}

//! Combine several components of a filepath, adding slashes if necessary
template<typename T, typename... Args>
QString combineFilePath(T const& first, Args... args)
{
    return QDir(first).filePath(combineFilePath(args...));
}

// Convert to Json
QJsonValue toJson(QUuid const& id);
QJsonValue toJson(QRect const& rect);
QJsonValue toJson(QFont const& font);
QJsonValue toJson(QPageLayout const& layout);
QJsonValue toJson(QMarginsF const& margins);
QJsonValue toJson(Qt::Alignment const& align);
QJsonValue toJson(QColor const& color);
QJsonValue toJson(QPair<double, double> const& pair);
QJsonValue toJson(QByteArray const& data);
QJsonValue toJson(QList<double> const& data);
QJsonValue toJson(QList<bool> const& data);

// Convert from Json
void fromJson(QUuid& id, QJsonValue const& obj);
void fromJson(QRect& rect, QJsonValue const& obj);
void fromJson(QFont& font, QJsonValue const& obj);
void fromJson(QPageLayout& layout, QJsonValue const& obj);
void fromJson(QMarginsF& margins, QJsonValue const& obj);
void fromJson(Qt::Alignment& align, QJsonValue const& obj);
void fromJson(QColor& color, QJsonValue const& obj);
void fromJson(QPair<double, double>& pair, QJsonValue const& obj);
void fromJson(QByteArray& data, QJsonValue const& obj);
void fromJson(QStringList& data, QJsonValue const& obj);
void fromJson(QList<double>& data, QJsonValue const& obj);
void fromJson(QList<bool>& data, QJsonValue const& obj);
}

#endif // FILEUTILITY_H

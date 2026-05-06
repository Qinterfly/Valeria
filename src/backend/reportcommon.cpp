#include <QJsonArray>
#include <QJsonObject>

#include "fileutility.h"
#include "reportcommon.h"

using namespace Backend;
using namespace Backend::Core;

ReportPoint::ReportPoint()
{
}

ReportPoint::ReportPoint(QString const& uName)
{
    QStringList tokens = uName.split(':');
    if (tokens.size() == 2)
    {
        component = tokens[0];
        node = tokens[1];
    }
    else
    {
        node = uName;
    }
}

ReportPoint::ReportPoint(QString const& uComponent, QString const& uNode)
    : component(uComponent)
    , node(uNode)
{
}

bool ReportPoint::isEmpty() const
{
    return name().isEmpty();
}

QString ReportPoint::name() const
{
    return QString("%1:%2").arg(component, node);
}

QJsonObject ReportPoint::toJson() const
{
    QJsonObject obj;
    obj["component"] = component;
    obj["node"] = node;
    return obj;
}

void ReportPoint::fromJson(QJsonObject const& obj)
{
    component = obj["component"].toString();
    node = obj["node"].toString();
}

bool ReportPoint::operator==(ReportPoint const& another) const
{
    return std::tie(component, node) == std::tie(another.component, another.node);
}

bool ReportPoint::operator!=(ReportPoint const& another) const
{
    return !(*this == another);
}

bool ReportPoint::operator<(ReportPoint const& another) const
{
    return std::tie(component, node) < std::tie(another.component, another.node);
}

bool ReportPoint::operator>(ReportPoint const& another) const
{
    return !(*this < another) && *this != another;
}

bool ReportPoint::operator<=(ReportPoint const& another) const
{
    return *this < another || *this == another;
}

bool ReportPoint::operator>=(ReportPoint const& another) const
{
    return *this > another || *this == another;
}

ReportCurve::ReportCurve()
{
    lineStyle = Qt::SolidLine;
    lineWidth = 1.25;
    lineColor = Qt::red;
    markerShape = ReportMarkerShape::kDisc;
    markerSize = 6.0;
    markerFill = false;
    markerSkip = 0;
}

ReportCurve::ReportCurve(QList<ReportPoint> const& uPoints, QString const& uName)
    : ReportCurve()
{
    name = uName;
    points = uPoints;
}

ReportCurve::ReportCurve(QList<QString> const& uPoints, QString const& uName)
    : ReportCurve()
{
    name = uName;
    int numPoints = uPoints.size();
    points.resize(numPoints);
    for (int i = 0; i != numPoints; ++i)
        points[i] = ReportPoint(uPoints[i]);
}

ReportCurve::ReportCurve(QColor const& uLineColor, ReportMarkerShape const& uMarkerShape, bool uMarkerFill)
    : ReportCurve()
{
    lineColor = uLineColor;
    markerShape = uMarkerShape;
    markerFill = uMarkerFill;
}

ReportCurve::ReportCurve(QList<QString> const& uPoints, QColor const& uLineColor, ReportMarkerShape const& uMarkerShape, bool uMarkerFill)
    : ReportCurve(uLineColor, uMarkerShape, uMarkerFill)
{
    int numPoints = uPoints.size();
    points.resize(numPoints);
    for (int i = 0; i != numPoints; ++i)
        points[i] = ReportPoint(uPoints[i]);
}

bool ReportCurve::isEmpty() const
{
    return points.isEmpty();
}

QJsonObject ReportCurve::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    QJsonArray jsonPoints;
    for (auto const& p : points)
        jsonPoints.push_back(p.toJson());
    obj["points"] = jsonPoints;

    // Line
    obj["lineStyle"] = (int) lineStyle;
    obj["lineWidth"] = lineWidth;
    obj["lineColor"] = Utility::toJson(lineColor);

    // Marker
    obj["markerShape"] = (int) markerShape;
    obj["markerSize"] = markerSize;
    obj["markerFill"] = markerFill;
    obj["markerSkip"] = markerSkip;

    return obj;
}

void ReportCurve::fromJson(QJsonObject const& obj)
{
    name = obj["name"].toString();
    QJsonArray jsonPoints = obj["points"].toArray();
    int numPoints = jsonPoints.size();
    points.resize(numPoints);
    for (int i = 0; i != numPoints; ++i)
        points[i].fromJson(jsonPoints[i].toObject());

    // Line
    lineStyle = (Qt::PenStyle) obj["lineStyle"].toInt();
    lineWidth = obj["lineWidth"].toDouble();
    Utility::fromJson(lineColor, obj["lineColor"]);

    // Marker
    markerShape = (ReportMarkerShape) obj["markerShape"].toInt();
    markerSize = obj["markerSize"].toDouble();
    markerFill = obj["markerFill"].toBool();
    markerSkip = obj["markerSkip"].toInt();
}

ReportSection::ReportSection()
{
    coordDir = ReportDirection::kN;
    responseDir = ReportDirection::kNone;
    sign = 1;
}

ReportSection::ReportSection(QString const& uFirstPoint, QString const& uSecondPoint, ReportDirection uResponseDir, int uSign,
                             QString const& uName)
    : ReportSection()
{
    name = uName;
    coordDir = ReportDirection::kN;
    responseDir = uResponseDir;
    sign = uSign;
    firstPoint = ReportPoint(uFirstPoint);
    secondPoint = ReportPoint(uSecondPoint);
}

QJsonObject ReportSection::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["coordDir"] = (int) coordDir;
    obj["responseDir"] = (int) responseDir;
    obj["sign"] = sign;
    obj["firstPoint"] = firstPoint.toJson();
    obj["secondPoint"] = secondPoint.toJson();
    return obj;
}

void ReportSection::fromJson(QJsonObject const& obj)
{
    name = obj["name"].toString();
    coordDir = (ReportDirection) obj["coordDir"].toInt();
    responseDir = (ReportDirection) obj["responseDir"].toInt();
    sign = obj["sign"].toInt();
    firstPoint.fromJson(obj["firstPoint"].toObject());
    secondPoint.fromJson(obj["secondPoint"].toObject());
}

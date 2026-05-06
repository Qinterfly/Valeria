#ifndef REPORTCOMMON_H
#define REPORTCOMMON_H

#include <QColor>
#include <QString>

#include "reportinterface.h"

namespace Backend::Core
{

enum class ReportDirection
{
    kNone,
    kX,
    kY,
    kZ,
    kN
};

enum class ReportMarkerShape
{
    kNone,
    kDot,
    kCross,
    kPlus,
    kCircle,
    kDisc,
    kSquare,
    kDiamond,
    kStar,
    kTriangle,
    kTriangleInverted,
    kCrossSquare,
    kPlusSquare,
    kCrossCircle,
    kPlusCircle,
    kPeace
};

enum class ReportView
{
    kFront,
    kRear,
    kTop,
    kBottom,
    kLeft,
    kRight,
    kIsometric
};

enum class ReportColorMap
{
    kCoolToWarm,
    kBlueToRed1,
    kBlueToRed2,
    kBlueToRed3,
    kVaradis,
    kJet,
    kPlasma,
};

//! Class to a define a layout of a point
class ReportPoint : public ISerializable
{
public:
    ReportPoint();
    ReportPoint(QString const& uName);
    ReportPoint(QString const& uComponent, QString const& uNode);
    ~ReportPoint() = default;

    bool isEmpty() const;
    QString name() const;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

    bool operator==(ReportPoint const& another) const;
    bool operator!=(ReportPoint const& another) const;
    bool operator<(ReportPoint const& another) const;
    bool operator>(ReportPoint const& another) const;
    bool operator<=(ReportPoint const& another) const;
    bool operator>=(ReportPoint const& another) const;

public:
    QString component;
    QString node;
};

//! Class to define a layout of a curve
class ReportCurve : public ISerializable
{
public:
    ReportCurve();
    ReportCurve(QList<ReportPoint> const& uPoints, QString const& uName = QString());
    ReportCurve(QList<QString> const& uPoints, QString const& uName = QString());
    ReportCurve(QColor const& uLineColor, ReportMarkerShape const& uMarkerShape, bool uMarkerFill = false);
    ReportCurve(QList<QString> const& uPoints, QColor const& uLineColor, ReportMarkerShape const& uMarkerShape, bool uMarkerFill = false);
    ~ReportCurve() = default;

    bool isEmpty() const;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QString name;
    QList<ReportPoint> points;

    // Line
    Qt::PenStyle lineStyle;
    double lineWidth;
    QColor lineColor;

    // Marker
    ReportMarkerShape markerShape;
    double markerSize;
    bool markerFill;
    int markerSkip;
};

//! Class to define a layout of a section
class ReportSection : public ISerializable
{
public:
    ReportSection();
    ReportSection(QString const& uFirstPoint, QString const& uSecondPoint, ReportDirection uCoordDir, ReportDirection uResponseDir,
                  int uSign = 1, QString const& uName = QString());
    ReportSection(QString const& uFirstPoint, QString const& uSecondPoint, ReportDirection uResponseDir, int uSign = 1,
                  QString const& uName = QString());
    ReportSection(QString const& uFirstPoint, ReportDirection uCoordDir, ReportDirection uResponseDir, int uSign = 1,
                  QString const& uName = QString());
    ~ReportSection() = default;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QString name;
    ReportDirection coordDir;
    ReportDirection responseDir;
    int sign;
    ReportPoint firstPoint;
    ReportPoint secondPoint;
};

}

#endif // REPORTCOMMON_H

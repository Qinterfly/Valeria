#ifndef REPORTITEM_H
#define REPORTITEM_H

#include <QColor>
#include <QFont>
#include <QList>
#include <QRect>
#include <QUuid>

#include "reportinterface.h"

namespace Backend::Core
{

// Common types
enum class ReportDirection
{
    kNone,
    kX,
    kY,
    kZ
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
    kBlueToRed,
    kVaradis,
    kJet,
    kPlasma
};

//! Base class for items
class ReportItem : public ISerializable
{
public:
    enum Type
    {
        kText,
        kGraph,
        kPicture,
        kTable,
        kMode
    };
    ReportItem();
    ReportItem(ReportItem const* pAnother);
    virtual ~ReportItem() = default;

    virtual Type type() const = 0;
    virtual ReportItem* clone() const = 0;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QUuid id;
    QString name;
    QRect rect;
    double angle;
    QFont font;
    QUuid link;
};

//! Class to define a layout of a text element
class TextReportItem : public ReportItem
{
public:
    TextReportItem();
    TextReportItem(ReportItem const* pAnother);
    virtual ~TextReportItem() = default;

    Type type() const override;
    ReportItem* clone() const override;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    Qt::Alignment align;
    QString text;
};

//! Class to a define a layout of a graph point
class GraphReportPoint : public ISerializable
{
public:
    GraphReportPoint();
    GraphReportPoint(QString const& uName);
    GraphReportPoint(QString const& uComponent, QString const& uNode);
    ~GraphReportPoint() = default;

    bool isEmpty() const;
    QString name() const;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QString component;
    QString node;
};

//! Class to define a layout of a graph curve
class GraphReportCurve : public ISerializable
{
public:
    GraphReportCurve();
    GraphReportCurve(QList<GraphReportPoint> const& uPoints, QString const& uName = QString());
    GraphReportCurve(QList<QString> const& uPoints, QString const& uName = QString());
    GraphReportCurve(QColor const& uLineColor, ReportMarkerShape const& uMarkerShape, bool uMarkerFill = false);
    GraphReportCurve(QList<QString> const& uPoints, QColor const& uLineColor, ReportMarkerShape const& uMarkerShape, bool uMarkerFill = false);
    ~GraphReportCurve() = default;

    bool isEmpty() const;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QString name;
    QList<GraphReportPoint> points;

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

//! Class to define a layout of a graph element
class GraphReportItem : public ReportItem
{
public:
    enum SubType
    {
        kNone,
        kReal,
        kImag,
        kMultiReal,
        kMultiImag,
        kFreqAmp,
        kModeshape
    };
    GraphReportItem();
    GraphReportItem(ReportItem const* pAnother);
    virtual ~GraphReportItem() = default;

    Type type() const override;
    ReportItem* clone() const override;

    bool isMultiPointCurve() const;
    void addCurve(GraphReportCurve const& curve);
    GraphReportCurve& addCurve(QStringList const& points, QString const& name = QString());
    GraphReportCurve& addPoint(QString const& point, QString const& name = QString());

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QList<GraphReportCurve> curves;

    // Header
    SubType subType;
    ReportDirection coordDir;
    ReportDirection responseDir;
    QString unit;

    // Axes
    PairDouble xRange;
    PairDouble yRange;
    QString xLabel;
    QString yLabel;
    double scaleRange;
    int numTicks;
    double gridWidth;
    double gridZeroWidth;
    bool swapAxes;
    bool reverseX;
    bool reverseY;
    Qt::Alignment legendAlign;

    // View
    bool showLegend;
    bool showBundleFreq;
};

//! Class to define a layout of a picture element
class PictureReportItem : public ReportItem
{
public:
    PictureReportItem();
    PictureReportItem(ReportItem const* pAnother);
    virtual ~PictureReportItem() = default;

    Type type() const override;
    ReportItem* clone() const override;

    bool load(QString const& pathFile);

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QByteArray data;
    QString format;
};

//! Class to define a layout of a table element
class TableReportItem : public ReportItem
{
public:
    TableReportItem();
    TableReportItem(ReportItem const* pAnother);
    virtual ~TableReportItem() = default;

    Type type() const override;
    ReportItem* clone() const override;

    bool isEmpty() const;
    int numRows() const;
    int numCols() const;
    void resize(int nRows, int nCols);
    void setNumRows(int nRows);
    void setNumCols(int nCols);

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QList<QStringList> data;
    QString midLabel;
    QStringList horLabels;
    QStringList verLabels;
    double gridWidth;
    bool showLabels;
};

//! Class to define a layout of a mode element
class ModeReportItem : public ReportItem
{
public:
    ModeReportItem();
    ModeReportItem(ReportItem const* pAnother);
    virtual ~ModeReportItem() = default;

    Type type() const override;
    ReportItem* clone() const override;

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    // Header
    QString unit;
    ReportView view;
    ReportColorMap colorMap;
    double zoom;
    double scale;
    QUuid link;

    // Settings
    QString title;
    QString sLabel;
    PairDouble sRange;
    double quality;
    QColor edgeColor;
    QColor undeformedColor;
    double edgeOpacity;
    double vertexSize;
    double lineWidth;
    bool showUndeformed;
    bool showVertices;
    bool showLines;
    bool showTrias;
    bool showQuads;
};

ReportItem* createItem(ReportItem::Type type);
}

#endif // REPORTITEM_H

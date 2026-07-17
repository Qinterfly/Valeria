#ifndef REPORTITEM_H
#define REPORTITEM_H

#include <QFont>
#include <QList>
#include <QRect>
#include <QUuid>

#include "reportcommon.h"

namespace Backend::Core
{

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
        kMode,
        kDiagram
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
    void addCurve(ReportCurve const& curve);
    ReportCurve& addCurve(QStringList const& points, QString const& name = QString());
    ReportCurve& addPoint(QString const& point, QString const& name = QString());

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QList<ReportCurve> curves;

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
    ReportColorTransform colorTransform;
    double viewAngle;
    double scale;
    double amplitude;

    // Selector
    QList<bool> maskComponents;

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
    bool showScalarBar;
};

//! Class to define a layout of a diagram element
class DiagramReportItem : public ReportItem
{
public:
    DiagramReportItem();
    DiagramReportItem(ReportItem const* pAnother);
    virtual ~DiagramReportItem() = default;

    Type type() const override;
    ReportItem* clone() const override;

    void addSection(ReportSection const& section);

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QList<ReportSection> sections;

    // Header
    QString unit;
    ReportView view;
    ReportColorMap colorMap;
    double viewAngle;
    double scale;
    double amplitude;

    // Selector
    QList<bool> maskComponents;

    // Settings
    QString title;
    QString sLabel;
    PairDouble sRange;
    double quality;
    QColor undeformedColor;
    double undeformedOpacity;
    double lineWidth;
    double barWidth;
    bool showRuler;
    bool showScalarBar;
};

ReportItem* createItem(ReportItem::Type type);
}

#endif // REPORTITEM_H

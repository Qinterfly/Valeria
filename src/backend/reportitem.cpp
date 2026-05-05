#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include "fileutility.h"
#include "reportitem.h"

using namespace Backend::Core;

ReportItem::ReportItem()
    : id(QUuid::createUuid())
    , name(QString())
    , rect(0, 0, 0, 0)
    , angle(0)
    , font("Times New Roman", 12)
    , link(QUuid())
{
}

ReportItem::ReportItem(ReportItem const* pAnother)
{
    id = pAnother->id;
    name = pAnother->name;
    rect = pAnother->rect;
    angle = pAnother->angle;
    font = pAnother->font;
    link = pAnother->link;
}

QJsonObject ReportItem::toJson() const
{
    QJsonObject obj;
    obj["type"] = type();
    obj["id"] = Utility::toJson(id);
    obj["name"] = name;
    obj["rect"] = Utility::toJson(rect);
    obj["angle"] = angle;
    obj["font"] = Utility::toJson(font);
    obj["link"] = Utility::toJson(link);
    return obj;
}

void ReportItem::fromJson(QJsonObject const& obj)
{
    Utility::fromJson(id, obj["id"]);
    name = obj["name"].toString();
    Utility::fromJson(rect, obj["rect"]);
    angle = obj["angle"].toDouble();
    Utility::fromJson(font, obj["font"]);
    Utility::fromJson(link, obj["link"]);
}

TextReportItem::TextReportItem()
{
    align = Qt::AlignHCenter | Qt::AlignVCenter;
}

TextReportItem::TextReportItem(ReportItem const* pAnother)
    : ReportItem(pAnother)
{
}

ReportItem::Type TextReportItem::type() const
{
    return ReportItem::kText;
}

ReportItem* TextReportItem::clone() const
{
    TextReportItem* pResult = new TextReportItem(this);
    pResult->align = align;
    pResult->text = text;
    return pResult;
}

QJsonObject TextReportItem::toJson() const
{
    QJsonObject obj = ReportItem::toJson();
    obj["align"] = Utility::toJson(align);
    obj["text"] = text;
    return obj;
}

void TextReportItem::fromJson(QJsonObject const& obj)
{
    ReportItem::fromJson(obj);
    Utility::fromJson(align, obj["align"]);
    text = obj["text"].toString();
}

GraphReportItem::GraphReportItem()
{
    // Header
    subType = kNone;
    coordDir = ReportDirection::kNone;
    responseDir = ReportDirection::kNone;
    unit = QString();

    // Axes
    xRange = {0.0, 0.0};
    yRange = {0.0, 0.0};
    scaleRange = 1.1;
    numTicks = 5;
    gridWidth = 1.0;
    gridZeroWidth = 2.0;
    swapAxes = false;
    reverseX = false;
    reverseY = false;
    legendAlign = Qt::AlignRight | Qt::AlignTop;

    // Flags
    showLegend = true;
    showBundleFreq = false;
}

GraphReportItem::GraphReportItem(ReportItem const* pAnother)
    : ReportItem(pAnother)
{
}

ReportItem::Type GraphReportItem::type() const
{
    return ReportItem::kGraph;
}

ReportItem* GraphReportItem::clone() const
{
    GraphReportItem* pResult = new GraphReportItem(this);
    pResult->curves = curves;

    // Header
    pResult->subType = subType;
    pResult->coordDir = coordDir;
    pResult->responseDir = responseDir;
    pResult->unit = unit;

    // Axes
    pResult->xRange = xRange;
    pResult->yRange = yRange;
    pResult->xLabel = xLabel;
    pResult->yLabel = yLabel;
    pResult->scaleRange = scaleRange;
    pResult->numTicks = numTicks;
    pResult->gridWidth = gridWidth;
    pResult->gridZeroWidth = gridZeroWidth;
    pResult->swapAxes = swapAxes;
    pResult->reverseX = reverseX;
    pResult->reverseY = reverseY;
    pResult->legendAlign = legendAlign;

    // Flags
    pResult->showLegend = showLegend;
    pResult->showBundleFreq = showBundleFreq;

    return pResult;
}

bool GraphReportItem::isMultiPointCurve() const
{
    return subType == GraphReportItem::kModeshape;
}

void GraphReportItem::addCurve(ReportCurve const& curve)
{
    curves.push_back(curve);
}

ReportCurve& GraphReportItem::addCurve(QStringList const& points, QString const& name)
{
    return curves.emplace_back(ReportCurve(points, name));
}

ReportCurve& GraphReportItem::addPoint(QString const& point, QString const& name)
{
    return addCurve({point}, name);
}

QJsonObject GraphReportItem::toJson() const
{
    QJsonObject obj = ReportItem::toJson();

    QJsonArray jsonCurves;
    for (auto const& c : curves)
        jsonCurves.push_back(c.toJson());
    obj["curves"] = jsonCurves;

    // Header
    obj["subType"] = (int) subType;
    obj["coordDir"] = (int) coordDir;
    obj["responseDir"] = (int) responseDir;
    obj["unit"] = unit;

    // Axes
    obj["xRange"] = Utility::toJson(xRange);
    obj["yRange"] = Utility::toJson(yRange);
    obj["xLabel"] = xLabel;
    obj["yLabel"] = yLabel;
    obj["scaleRange"] = scaleRange;
    obj["numTicks"] = numTicks;
    obj["gridWidth"] = gridWidth;
    obj["gridZeroWidth"] = gridZeroWidth;
    obj["swapAxes"] = swapAxes;
    obj["reverseX"] = reverseX;
    obj["reverseY"] = reverseY;
    obj["legendAlign"] = Utility::toJson(legendAlign);

    // View
    obj["showLegend"] = showLegend;
    obj["showBundleFreq"] = showBundleFreq;

    return obj;
}

void GraphReportItem::fromJson(QJsonObject const& obj)
{
    ReportItem::fromJson(obj);

    QJsonArray jsonCurves = obj["curves"].toArray();
    int numCurves = jsonCurves.size();
    curves.resize(numCurves);
    for (int i = 0; i != numCurves; ++i)
        curves[i].fromJson(jsonCurves[i].toObject());

    // Header
    subType = (GraphReportItem::SubType) obj["subType"].toInt();
    coordDir = (ReportDirection) obj["coordDir"].toInt();
    responseDir = (ReportDirection) obj["responseDir"].toInt();
    unit = obj["unit"].toString();

    // Axes
    Utility::fromJson(xRange, obj["xRange"]);
    Utility::fromJson(yRange, obj["yRange"]);
    xLabel = obj["xLabel"].toString();
    yLabel = obj["yLabel"].toString();
    scaleRange = obj["scaleRange"].toDouble();
    numTicks = obj["numTicks"].toInt();
    gridWidth = obj["gridWidth"].toDouble();
    gridZeroWidth = obj["gridZeroWidth"].toDouble();
    swapAxes = obj["swapAxes"].toBool();
    reverseX = obj["reverseX"].toBool();
    reverseY = obj["reverseY"].toBool();
    Utility::fromJson(legendAlign, obj["legendAlign"]);

    // View
    showLegend = obj["showLegend"].toBool();
    showBundleFreq = obj["showBundleFreq"].toBool();
}

PictureReportItem::PictureReportItem()
{
}

PictureReportItem::PictureReportItem(ReportItem const* pAnother)
    : ReportItem(pAnother)
{
}

ReportItem::Type PictureReportItem::type() const
{
    return ReportItem::kPicture;
}

ReportItem* PictureReportItem::clone() const
{
    PictureReportItem* pResult = new PictureReportItem(this);
    pResult->data = data;
    pResult->format = format;
    return pResult;
}

bool PictureReportItem::load(QString const& pathFile)
{
    QFile file(pathFile);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    data = file.readAll();
    format = QFileInfo(pathFile).suffix();
    file.close();
    return true;
}

QJsonObject PictureReportItem::toJson() const
{
    QJsonObject obj = ReportItem::toJson();
    obj["data"] = Utility::toJson(data);
    obj["format"] = format;
    return obj;
}

void PictureReportItem::fromJson(QJsonObject const& obj)
{
    ReportItem::fromJson(obj);
    Utility::fromJson(data, obj["data"]);
    format = obj["format"].toString();
}

TableReportItem::TableReportItem()
{
    gridWidth = 0.0;
    showLabels = true;
}

TableReportItem::TableReportItem(ReportItem const* pAnother)
    : ReportItem(pAnother)
{
}

ReportItem::Type TableReportItem::type() const
{
    return ReportItem::kTable;
}

ReportItem* TableReportItem::clone() const
{
    TableReportItem* pResult = new TableReportItem(this);
    pResult->data = data;
    pResult->midLabel = midLabel;
    pResult->horLabels = horLabels;
    pResult->verLabels = verLabels;
    pResult->gridWidth = gridWidth;
    pResult->showLabels = showLabels;
    return pResult;
}

bool TableReportItem::isEmpty() const
{
    return numRows() == 0 || numCols() == 0;
}

int TableReportItem::numRows() const
{
    return data.size();
}

int TableReportItem::numCols() const
{
    if (data.isEmpty())
        return 0;
    return data.first().size();
}

void TableReportItem::resize(int nRows, int nCols)
{
    data.resize(nRows);
    for (int i = 0; i != nRows; ++i)
        data[i].resize(nCols);
    horLabels.resize(nCols);
    verLabels.resize(nRows);
}

void TableReportItem::setNumRows(int nRows)
{
    resize(nRows, numCols());
}

void TableReportItem::setNumCols(int nCols)
{
    resize(numRows(), nCols);
}

QJsonObject TableReportItem::toJson() const
{
    QJsonObject obj = ReportItem::toJson();
    QJsonArray jsonData;
    for (auto const& row : data)
        jsonData.push_back(QJsonArray::fromStringList(row));
    obj["data"] = jsonData;
    obj["midLabel"] = midLabel;
    obj["horLabels"] = QJsonArray::fromStringList(horLabels);
    obj["verLabels"] = QJsonArray::fromStringList(verLabels);
    obj["gridWidth"] = gridWidth;
    obj["showLabels"] = showLabels;
    return obj;
}

void TableReportItem::fromJson(QJsonObject const& obj)
{
    ReportItem::fromJson(obj);
    QJsonArray jsonData = obj["data"].toArray();
    int nRows = jsonData.size();
    data.resize(nRows);
    for (int i = 0; i != nRows; ++i)
        Utility::fromJson(data[i], jsonData[i]);
    midLabel = obj["midLabel"].toString();
    Utility::fromJson(horLabels, obj["horLabels"]);
    Utility::fromJson(verLabels, obj["verLabels"]);
    gridWidth = obj["gridWidth"].toDouble();
    showLabels = obj["showLabels"].toBool();
}

ModeReportItem::ModeReportItem()
{
    // View
    view = ReportView::kIsometric;
    colorMap = ReportColorMap::kJet;
    scale = 1.5;
    amplitude = 0.1;
    phase = 0.0;

    // Settings
    sRange = {0.0, 0.0};
    quality = 2.0;
    edgeColor = QColor("grey");
    undeformedColor = QColor("grey");
    edgeOpacity = 0.5;
    vertexSize = 5.0;
    lineWidth = 2.0;
    showUndeformed = true;
    showVertices = true;
    showLines = true;
    showTrias = true;
    showQuads = true;
}

ModeReportItem::ModeReportItem(ReportItem const* pAnother)
    : ReportItem(pAnother)
{
}

ReportItem::Type ModeReportItem::type() const
{
    return ReportItem::kMode;
}

ReportItem* ModeReportItem::clone() const
{
    ModeReportItem* pResult = new ModeReportItem(this);

    // Header
    pResult->unit = unit;
    pResult->view = view;
    pResult->colorMap = colorMap;
    pResult->scale = scale;
    pResult->amplitude = amplitude;
    pResult->phase = phase;

    // Settings
    pResult->title = title;
    pResult->sLabel = sLabel;
    pResult->sRange = sRange;
    pResult->quality = quality;
    pResult->edgeColor = edgeColor;
    pResult->undeformedColor = undeformedColor;
    pResult->edgeOpacity = edgeOpacity;
    pResult->vertexSize = vertexSize;
    pResult->lineWidth = lineWidth;
    pResult->showUndeformed = showUndeformed;
    pResult->showVertices = showVertices;
    pResult->showLines = showLines;
    pResult->showTrias = showTrias;
    pResult->showQuads = showQuads;

    return pResult;
}

QJsonObject ModeReportItem::toJson() const
{
    QJsonObject obj = ReportItem::toJson();

    // Header
    obj["unit"] = unit;
    obj["view"] = (int) view;
    obj["colorMap"] = (int) colorMap;
    obj["scale"] = scale;
    obj["amplitude"] = amplitude;
    obj["phase"] = phase;

    // Settings
    obj["title"] = title;
    obj["sLabel"] = sLabel;
    obj["sRange"] = Utility::toJson(sRange);
    obj["quality"] = quality;
    obj["edgeColor"] = Utility::toJson(edgeColor);
    obj["undeformedColor"] = Utility::toJson(undeformedColor);
    obj["edgeOpacity"] = edgeOpacity;
    obj["vertexSize"] = vertexSize;
    obj["lineWidth"] = lineWidth;
    obj["showUndeformed"] = showUndeformed;
    obj["showVertices"] = showVertices;
    obj["showLines"] = showLines;
    obj["showTrias"] = showTrias;
    obj["showQuads"] = showQuads;

    return obj;
}

void ModeReportItem::fromJson(QJsonObject const& obj)
{
    ReportItem::fromJson(obj);

    // Header
    unit = obj["unit"].toString();
    view = (ReportView) obj["view"].toInt();
    colorMap = (ReportColorMap) obj["colorMap"].toInt();
    scale = obj["scale"].toDouble();
    amplitude = obj["amplitude"].toDouble();
    phase = obj["phase"].toDouble();
    Utility::fromJson(link, obj["link"]);

    // Settings
    title = obj["title"].toString();
    sLabel = obj["sLabel"].toString();
    Utility::fromJson(sRange, obj["sRange"]);
    quality = obj["quality"].toDouble();
    Utility::fromJson(edgeColor, obj["edgeColor"]);
    Utility::fromJson(undeformedColor, obj["undeformedColor"]);
    edgeOpacity = obj["edgeOpacity"].toDouble();
    vertexSize = obj["vertexSize"].toDouble();
    lineWidth = obj["lineWidth"].toDouble();
    showUndeformed = obj["showUndeformed"].toBool();
    showVertices = obj["showVertices"].toBool();
    showLines = obj["showLines"].toBool();
    showTrias = obj["showTrias"].toBool();
    showQuads = obj["showQuads"].toBool();
}

DiagramReportItem::DiagramReportItem()
{
    // View
    view = ReportView::kTop;
    colorMap = ReportColorMap::kJet;
    scale = 1.5;
    amplitude = 0.1;
    phase = 0.0;

    // Settings
    sRange = {0.0, 0.0};
    quality = 2.0;
}

DiagramReportItem::DiagramReportItem(ReportItem const* pAnother)
    : ReportItem(pAnother)
{
}

ReportItem::Type DiagramReportItem::type() const
{
    return ReportItem::kDiagram;
}

ReportItem* DiagramReportItem::clone() const
{
    DiagramReportItem* pResult = new DiagramReportItem(this);

    // Header
    pResult->unit = unit;
    pResult->view = view;
    pResult->colorMap = colorMap;
    pResult->scale = scale;
    pResult->amplitude = amplitude;
    pResult->phase = phase;

    // Settings
    pResult->title = title;
    pResult->sLabel = sLabel;
    pResult->sRange = sRange;
    pResult->quality = quality;

    return pResult;
}

QJsonObject DiagramReportItem::toJson() const
{
    QJsonObject obj = ReportItem::toJson();

    QJsonArray jsonSections;
    for (auto const& s : sections)
        jsonSections.push_back(s.toJson());
    obj["sections"] = jsonSections;

    // Header
    obj["unit"] = unit;
    obj["view"] = (int) view;
    obj["colorMap"] = (int) colorMap;
    obj["scale"] = scale;
    obj["amplitude"] = amplitude;
    obj["phase"] = phase;

    // Settings
    obj["title"] = title;
    obj["sLabel"] = sLabel;
    obj["sRange"] = Utility::toJson(sRange);
    obj["quality"] = quality;

    return obj;
}

void DiagramReportItem::fromJson(QJsonObject const& obj)
{
    ReportItem::fromJson(obj);

    QJsonArray jsonSections = obj["sections"].toArray();
    int numSections = jsonSections.size();
    sections.resize(numSections);
    for (int i = 0; i != numSections; ++i)
        sections[i].fromJson(jsonSections[i].toObject());

    // Header
    unit = obj["unit"].toString();
    view = (ReportView) obj["view"].toInt();
    colorMap = (ReportColorMap) obj["colorMap"].toInt();
    scale = obj["scale"].toDouble();
    amplitude = obj["amplitude"].toDouble();
    phase = obj["phase"].toDouble();
    Utility::fromJson(link, obj["link"]);

    // Settings
    title = obj["title"].toString();
    sLabel = obj["sLabel"].toString();
    Utility::fromJson(sRange, obj["sRange"]);
    quality = obj["quality"].toDouble();
}

ReportItem* Backend::Core::createItem(ReportItem::Type type)
{
    switch (type)
    {
    case ReportItem::kText:
        return new TextReportItem;
    case ReportItem::kGraph:
        return new GraphReportItem;
    case ReportItem::kPicture:
        return new PictureReportItem;
    case ReportItem::kTable:
        return new TableReportItem;
    case ReportItem::kMode:
        return new ModeReportItem;
    case ReportItem::kDiagram:
        return new DiagramReportItem;
    default:
        break;
    }
    return nullptr;
}

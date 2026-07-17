#include <QVBoxLayout>

#include <customvariantpropertymanager.h>
#include <magic_enum/magic_enum.hpp>
#include <qttreepropertybrowser.h>
#include <qtvariantproperty.h>

#include "reportdocument.h"
#include "reportitem.h"
#include "reportpropertyeditor.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Backend::Core;
using namespace Frontend;

// Helper function
PairDouble convert(QString text);
QString convert(PairDouble const& data);

ReportPropertyEditor::ReportPropertyEditor(QWidget* pParent)
    : QWidget(pParent)
{
    setFont(Utility::getFont());
    createContent();
    createConnections();
}

//! Set the item to edit
void ReportPropertyEditor::setItemGetter(ReportItemGetter getter)
{
    mItemGetter = std::move(getter);
    refresh();
}

//! Create all the widgets
void ReportPropertyEditor::createContent()
{
    // Create the widgets
    mpManager = new CustomVariantPropertyManager;
    mpFactory = new QtVariantEditorFactory;
    mpEditor = new QtTreePropertyBrowser;

    // Initialize the widgets
    mpEditor->setFactoryForManager((QtVariantPropertyManager*) mpManager, mpFactory);
    mpEditor->setFont(font());
    mpEditor->setTreeWidgetFont(font());

    // Combine the widgets
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->addWidget(mpEditor);
    setLayout(pLayout);
}

//! Specify connections between widgets
void ReportPropertyEditor::createConnections()
{
    connect(mpManager, &CustomVariantPropertyManager::valueChanged, this, &ReportPropertyEditor::setValue);
}

//! Update the widgets content
void ReportPropertyEditor::refresh()
{
    // Clear all the properties
    QSignalBlocker blockerEditor(mpEditor);
    QSignalBlocker blockerManager(mpManager);
    mpEditor->clear();
    mpManager->clear();

    // Get the item
    if (!mItemGetter)
        return;
    ReportItem* pItem = mItemGetter();
    if (!pItem)
        return;

    // Add properties
    addBaseProperties(pItem);
    switch (pItem->type())
    {
    case ReportItem::kGraph:
        addGraphProperties((GraphReportItem*) pItem);
        break;
    case ReportItem::kTable:
        addTableProperties((TableReportItem*) pItem);
        break;
    case ReportItem::kMode:
        addModeProperties((ModeReportItem*) pItem);
        break;
    case ReportItem::kDiagram:
        addDiagramProperties((DiagramReportItem*) pItem);
        break;
    default:
        break;
    }
}

//! Create the properties which are common for all item types
void ReportPropertyEditor::addBaseProperties(ReportItem* pItem)
{
    QtVariantProperty* pRectProperty = mpManager->addProperty(kRect, QMetaType::QRect, tr("Position"));
    pRectProperty->setValue(pItem->rect);
    mpEditor->addProperty(pRectProperty);

    QtVariantProperty* pAngleProperty = mpManager->addProperty(kAngle, QMetaType::Double, tr("Rotation, %1").arg(Constants::Symbol::skDeg));
    pAngleProperty->setValue(pItem->angle);
    mpEditor->addProperty(pAngleProperty);

    QtVariantProperty* pFontProperty = mpManager->addProperty(kFont, QMetaType::QFont, tr("Font"));
    pFontProperty->setValue(pItem->font);
    QtBrowserItem* pFontItem = mpEditor->addProperty(pFontProperty);
    mpEditor->setExpanded(pFontItem, false);
}

//! Create properties specific for graph items
void ReportPropertyEditor::addGraphProperties(GraphReportItem* pItem)
{
    QStringList const kAlignmentNames = {tr("Top right"), tr("Top left"), tr("Bottom right"), tr("Bottom left"),
                                         tr("Right"),     tr("Left"),     tr("Top"),          tr("Bottom")};

    QtVariantProperty* pXRangeProperty = mpManager->addProperty(kGraphXRange, QMetaType::QString, tr("X range"));
    pXRangeProperty->setValue(convert(pItem->xRange));
    mpEditor->addProperty(pXRangeProperty);

    QtVariantProperty* pYRangeProperty = mpManager->addProperty(kGraphYRange, QMetaType::QString, tr("Y range"));
    pYRangeProperty->setValue(convert(pItem->yRange));
    mpEditor->addProperty(pYRangeProperty);

    QtVariantProperty* pXLabelProperty = mpManager->addProperty(kGraphXLabel, QMetaType::QString, tr("X label"));
    pXLabelProperty->setValue(pItem->xLabel);
    mpEditor->addProperty(pXLabelProperty);

    QtVariantProperty* pYLabelProperty = mpManager->addProperty(kGraphYLabel, QMetaType::QString, tr("Y label"));
    pYLabelProperty->setValue(pItem->yLabel);
    mpEditor->addProperty(pYLabelProperty);

    QtVariantProperty* pScaleRangeProperty = mpManager->addProperty(kGraphScaleRange, QMetaType::Double, tr("Scale range"));
    pScaleRangeProperty->setValue(pItem->scaleRange);
    mpEditor->addProperty(pScaleRangeProperty);

    QtVariantProperty* pNumTicksProperty = mpManager->addProperty(kGraphNumTicks, QMetaType::Int, tr("Ticks number"));
    pNumTicksProperty->setValue(pItem->numTicks);
    mpEditor->addProperty(pNumTicksProperty);

    QtVariantProperty* pGridWidthProperty = mpManager->addProperty(kGraphGridWidth, QMetaType::Double, tr("Grid width"));
    pGridWidthProperty->setValue(pItem->gridWidth);
    mpEditor->addProperty(pGridWidthProperty);

    QtVariantProperty* pGridZeroWidthProperty = mpManager->addProperty(kGraphGridZeroWidth, QMetaType::Double, tr("Grid zero width"));
    pGridZeroWidthProperty->setValue(pItem->gridZeroWidth);
    mpEditor->addProperty(pGridZeroWidthProperty);

    QtVariantProperty* pSwapAxesProperty = mpManager->addProperty(kGraphSwapAxes, QMetaType::Bool, tr("Swap axes"));
    pSwapAxesProperty->setValue(pItem->swapAxes);
    mpEditor->addProperty(pSwapAxesProperty);

    QtVariantProperty* pReverseXProperty = mpManager->addProperty(kGraphReverseX, QMetaType::Bool, tr("Reverse X axis"));
    pReverseXProperty->setValue(pItem->reverseX);
    mpEditor->addProperty(pReverseXProperty);

    QtVariantProperty* pReverseYProperty = mpManager->addProperty(kGraphReverseY, QMetaType::Bool, tr("Reverse Y axis"));
    pReverseYProperty->setValue(pItem->reverseY);
    mpEditor->addProperty(pReverseYProperty);

    QtVariantProperty* pLegendAlignProperty = mpManager->addProperty(kGraphLegendAlign, QtVariantPropertyManager::enumTypeId(),
                                                                     tr("Legend alignment"));
    pLegendAlignProperty->setAttribute("enumNames", kAlignmentNames);
    auto aligns = magic_enum::enum_values<Align>();
    int numAligns = aligns.size();
    for (int i = 0; i != numAligns; ++i)
    {
        if (getAlignValue(aligns[i]) == pItem->legendAlign)
        {
            pLegendAlignProperty->setValue(i);
            break;
        }
    }
    mpEditor->addProperty(pLegendAlignProperty);

    QtVariantProperty* pShowLegendProperty = mpManager->addProperty(kGraphShowLegend, QMetaType::Bool, tr("Legend"));
    pShowLegendProperty->setValue(pItem->showLegend);
    mpEditor->addProperty(pShowLegendProperty);

    QtVariantProperty* pShowBundleFreqProperty = mpManager->addProperty(kGraphShowBundleFreq, QMetaType::Bool, tr("Bundle freq."));
    pShowBundleFreqProperty->setValue(pItem->showBundleFreq);
    mpEditor->addProperty(pShowBundleFreqProperty);
}

//! Create properties specific for table items
void ReportPropertyEditor::addTableProperties(TableReportItem* pItem)
{
    QtVariantProperty* pNumRowsProperty = mpManager->addProperty(kTableNumRows, QMetaType::Int, tr("Number of rows"));
    pNumRowsProperty->setValue(pItem->numRows());
    mpEditor->addProperty(pNumRowsProperty);

    QtVariantProperty* pNumColsProperty = mpManager->addProperty(kTableNumCols, QMetaType::Int, tr("Number of columns"));
    pNumColsProperty->setValue(pItem->numCols());
    mpEditor->addProperty(pNumColsProperty);

    QtVariantProperty* pShowLabelsProperty = mpManager->addProperty(kTableShowLabels, QMetaType::Bool, tr("Header"));
    pShowLabelsProperty->setValue(pItem->showLabels);
    mpEditor->addProperty(pShowLabelsProperty);
}

//! Create properties specific for mode items
void ReportPropertyEditor::addModeProperties(ModeReportItem* pItem)
{
    QtVariantProperty* pTitleProperty = mpManager->addProperty(kModeTitle, QMetaType::QString, tr("Title"));
    pTitleProperty->setValue(pItem->title);
    mpEditor->addProperty(pTitleProperty);

    QtVariantProperty* pSLabelProperty = mpManager->addProperty(kModeSLabel, QMetaType::QString, tr("Label"));
    pSLabelProperty->setValue(pItem->sLabel);
    mpEditor->addProperty(pSLabelProperty);

    QtVariantProperty* pQualityProperty = mpManager->addProperty(kModeQuality, QMetaType::Double, tr("Quality"));
    pQualityProperty->setValue(pItem->quality);
    mpEditor->addProperty(pQualityProperty);

    QtVariantProperty* pVertexSizeProperty = mpManager->addProperty(kModeVertexSize, QMetaType::Double, tr("Vertex size"));
    pVertexSizeProperty->setValue(pItem->vertexSize);
    mpEditor->addProperty(pVertexSizeProperty);

    QtVariantProperty* pLineWidthProperty = mpManager->addProperty(kModeLineWidth, QMetaType::Double, tr("Line width"));
    pLineWidthProperty->setValue(pItem->lineWidth);
    mpEditor->addProperty(pLineWidthProperty);

    QtVariantProperty* pShowUndeformedProperty = mpManager->addProperty(kModeShowUndeformed, QMetaType::Bool, tr("Show undeformed"));
    pShowUndeformedProperty->setValue(pItem->showUndeformed);
    mpEditor->addProperty(pShowUndeformedProperty);

    QtVariantProperty* pShowScalarBarProperty = mpManager->addProperty(kModeShowScalarBar, QMetaType::Bool, tr("Show scalar bar"));
    pShowScalarBarProperty->setValue(pItem->showScalarBar);
    mpEditor->addProperty(pShowScalarBarProperty);
}

//! Create properties specific for diagram items
void ReportPropertyEditor::addDiagramProperties(DiagramReportItem* pItem)
{
    QtVariantProperty* pTitleProperty = mpManager->addProperty(kDiagramTitle, QMetaType::QString, tr("Title"));
    pTitleProperty->setValue(pItem->title);
    mpEditor->addProperty(pTitleProperty);

    QtVariantProperty* pSLabelProperty = mpManager->addProperty(kDiagramSLabel, QMetaType::QString, tr("Label"));
    pSLabelProperty->setValue(pItem->sLabel);
    mpEditor->addProperty(pSLabelProperty);

    QtVariantProperty* pQualityProperty = mpManager->addProperty(kDiagramQuality, QMetaType::Double, tr("Quality"));
    pQualityProperty->setValue(pItem->quality);
    mpEditor->addProperty(pQualityProperty);

    QtVariantProperty* pUndeformedOpacityProperty = mpManager->addProperty(kDiagramUndeformedOpacity, QMetaType::Double,
                                                                           tr("Undeformed opacity"));
    pUndeformedOpacityProperty->setValue(pItem->undeformedOpacity);
    mpEditor->addProperty(pUndeformedOpacityProperty);

    QtVariantProperty* pLineWidthProperty = mpManager->addProperty(kDiagramLineWidth, QMetaType::Double, tr("Line width"));
    pLineWidthProperty->setValue(pItem->lineWidth);
    mpEditor->addProperty(pLineWidthProperty);

    QtVariantProperty* pBarWidthProperty = mpManager->addProperty(kDiagramBarWidth, QMetaType::Double, tr("Bar width"));
    pBarWidthProperty->setValue(pItem->barWidth);
    mpEditor->addProperty(pBarWidthProperty);

    QtVariantProperty* pShowRulerProperty = mpManager->addProperty(kDiagramShowRuler, QMetaType::Bool, tr("Show ruler"));
    pShowRulerProperty->setValue(pItem->showRuler);
    mpEditor->addProperty(pShowRulerProperty);

    QtVariantProperty* pShowScalarBarProperty = mpManager->addProperty(kDiagramShowScalarBar, QMetaType::Bool, tr("Show scalar bar"));
    pShowScalarBarProperty->setValue(pItem->showScalarBar);
    mpEditor->addProperty(pShowScalarBarProperty);
}

//! Change the item property value
void ReportPropertyEditor::setValue(QtProperty* pProperty, QVariant value)
{
    // Get the item
    if (!mItemGetter)
        return;
    ReportItem* pItem = mItemGetter();
    if (!pItem)
        return;

    // Get the property id
    if (!mpManager->contains(pProperty))
        return;
    int propertyID = mpManager->id(pProperty);
    switch (propertyID)
    {
    // Base
    case kRect:
        pItem->rect = value.toRect();
        break;
    case kAngle:
        pItem->angle = value.toDouble();
        break;
    case kFont:
        pItem->font = value.value<QFont>();
        break;

    // Text
    case kTextText:
        static_cast<TextReportItem*>(pItem)->text = value.toString();
        break;

    // Graph
    case kGraphXRange:
        static_cast<GraphReportItem*>(pItem)->xRange = convert(value.toString());
        break;
    case kGraphYRange:
        static_cast<GraphReportItem*>(pItem)->yRange = convert(value.toString());
        break;
    case kGraphXLabel:
        static_cast<GraphReportItem*>(pItem)->xLabel = value.toString();
        break;
    case kGraphYLabel:
        static_cast<GraphReportItem*>(pItem)->yLabel = value.toString();
        break;
    case kGraphScaleRange:
        static_cast<GraphReportItem*>(pItem)->scaleRange = value.toDouble();
        break;
    case kGraphNumTicks:
        static_cast<GraphReportItem*>(pItem)->numTicks = value.toInt();
        break;
    case kGraphGridWidth:
        static_cast<GraphReportItem*>(pItem)->gridWidth = value.toDouble();
        break;
    case kGraphGridZeroWidth:
        static_cast<GraphReportItem*>(pItem)->gridZeroWidth = value.toDouble();
        break;
    case kGraphSwapAxes:
        static_cast<GraphReportItem*>(pItem)->swapAxes = value.toBool();
        break;
    case kGraphReverseX:
        static_cast<GraphReportItem*>(pItem)->reverseX = value.toBool();
        break;
    case kGraphReverseY:
        static_cast<GraphReportItem*>(pItem)->reverseY = value.toBool();
        break;
    case kGraphLegendAlign:
        static_cast<GraphReportItem*>(pItem)->legendAlign = getAlignValue((Align) value.toInt());
        break;
    case kGraphShowLegend:
        static_cast<GraphReportItem*>(pItem)->showLegend = value.toBool();
        break;
    case kGraphShowBundleFreq:
        static_cast<GraphReportItem*>(pItem)->showBundleFreq = value.toBool();
        break;

    // Table
    case kTableNumRows:
        static_cast<TableReportItem*>(pItem)->setNumRows(value.toInt());
        break;
    case kTableNumCols:
        static_cast<TableReportItem*>(pItem)->setNumCols(value.toInt());
        break;
    case kTableShowLabels:
        static_cast<TableReportItem*>(pItem)->showLabels = value.toBool();
        break;

    // Mode
    case kModeTitle:
        static_cast<ModeReportItem*>(pItem)->title = value.toString();
        break;
    case kModeSLabel:
        static_cast<ModeReportItem*>(pItem)->sLabel = value.toString();
        break;
    case kModeQuality:
        static_cast<ModeReportItem*>(pItem)->quality = value.toDouble();
        break;
    case kModeVertexSize:
        static_cast<ModeReportItem*>(pItem)->vertexSize = value.toDouble();
        break;
    case kModeLineWidth:
        static_cast<ModeReportItem*>(pItem)->lineWidth = value.toDouble();
        break;
    case kModeShowUndeformed:
        static_cast<ModeReportItem*>(pItem)->showUndeformed = value.toBool();
        break;
    case kModeShowScalarBar:
        static_cast<ModeReportItem*>(pItem)->showScalarBar = value.toBool();
        break;

    // Diagram
    case kDiagramTitle:
        static_cast<DiagramReportItem*>(pItem)->title = value.toString();
        break;
    case kDiagramSLabel:
        static_cast<DiagramReportItem*>(pItem)->sLabel = value.toString();
        break;
    case kDiagramQuality:
        static_cast<DiagramReportItem*>(pItem)->quality = value.toDouble();
        break;
    case kDiagramUndeformedOpacity:
        static_cast<DiagramReportItem*>(pItem)->undeformedOpacity = value.toDouble();
        break;
    case kDiagramLineWidth:
        static_cast<DiagramReportItem*>(pItem)->lineWidth = value.toDouble();
        break;
    case kDiagramBarWidth:
        static_cast<DiagramReportItem*>(pItem)->barWidth = value.toDouble();
        break;
    case kDiagramShowRuler:
        static_cast<DiagramReportItem*>(pItem)->showRuler = value.toBool();
        break;
    case kDiagramShowScalarBar:
        static_cast<DiagramReportItem*>(pItem)->showScalarBar = value.toBool();
        break;

    default:
        return;
    }
    emit edited();
}

//! Get corner alignment value by enum key
Qt::Alignment ReportPropertyEditor::getAlignValue(Align key)
{
    switch (key)
    {
    case kTopRight:
        return Qt::AlignTop | Qt::AlignRight;
    case kTopLeft:
        return Qt::AlignTop | Qt::AlignLeft;
    case kBottomRight:
        return Qt::AlignBottom | Qt::AlignRight;
    case kBottomLeft:
        return Qt::AlignBottom | Qt::AlignLeft;
    case kRight:
        return Qt::AlignRight;
    case kLeft:
        return Qt::AlignLeft;
    case kTop:
        return Qt::AlignTop;
    case kBottom:
        return Qt::AlignBottom;
    default:
        break;
    };
    return Qt::Alignment();
}

//! Helper function to convert text to pair of double values
PairDouble convert(QString text)
{
    PairDouble result(0.0, 0.0);
    text.replace(',', '.');
    QStringList tokens = text.split(' ');
    if (tokens.size() == 2)
    {
        result.first = tokens.first().toDouble();
        result.second = tokens.last().toDouble();
    }
    return result;
}

//! Helper function to convert pair of double values to text
QString convert(PairDouble const& data)
{
    return QString("%1 %2").arg(QString::number(data.first), QString::number(data.second));
}

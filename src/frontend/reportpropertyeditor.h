#ifndef REPORTPROPERTYEDITOR_H
#define REPORTPROPERTYEDITOR_H

#include <QUuid>
#include <QWidget>

#include "reportinterface.h"

class CustomVariantPropertyManager;
class QtVariantEditorFactory;
class QtTreePropertyBrowser;
class QtProperty;

namespace Backend::Core
{
class ReportPage;
class TextReportItem;
class GraphReportItem;
class TableReportItem;
class ModeReportItem;
class DiagramReportItem;
}

namespace Frontend
{

class ReportPropertyEditor : public QWidget
{
    Q_OBJECT

public:
    enum Align
    {
        kTopRight,
        kTopLeft,
        kBottomRight,
        kBottomLeft,
        kRight,
        kLeft,
        kTop,
        kBottom
    };
    enum Type
    {
        // Base
        kRect,
        kAngle,
        kFont,

        // Text
        kTextText,

        // Graph
        kGraphXRange,
        kGraphYRange,
        kGraphXLabel,
        kGraphYLabel,
        kGraphScaleRange,
        kGraphNumTicks,
        kGraphGridWidth,
        kGraphGridZeroWidth,
        kGraphSwapAxes,
        kGraphReverseX,
        kGraphReverseY,
        kGraphLegendAlign,
        kGraphShowLegend,
        kGraphShowBundleFreq,
        kGraphShowLabels,

        // Table
        kTableNumRows,
        kTableNumCols,
        kTableShowLabels,

        // Mode
        kModeTitle,
        kModeSLabel,
        kModeQuality,
        kModeVertexSize,
        kModeLineWidth,
        kModeShowUndeformed,
        kModeShowScalarBar,

        // Diagram
        kDiagramTitle,
        kDiagramSLabel,
        kDiagramQuality,
        kDiagramUndeformedOpacity,
        kDiagramLineWidth,
        kDiagramBarWidth,
        kDiagramShowRuler,
        kDiagramShowScalarBar
    };

    ReportPropertyEditor(QWidget* pParent = nullptr);
    virtual ~ReportPropertyEditor() = default;

    void refresh();
    void setItemGetter(Backend::Core::ReportItemGetter getter);

signals:
    void edited();

private:
    void createContent();
    void createConnections();
    void addBaseProperties(Backend::Core::ReportItem* pItem);
    void addGraphProperties(Backend::Core::GraphReportItem* pItem);
    void addTableProperties(Backend::Core::TableReportItem* pItem);
    void addModeProperties(Backend::Core::ModeReportItem* pItem);
    void addDiagramProperties(Backend::Core::DiagramReportItem* pItem);
    void setValue(QtProperty* pProperty, QVariant value);
    Qt::Alignment getAlignValue(Align key);

private:
    Backend::Core::ReportItemGetter mItemGetter;
    CustomVariantPropertyManager* mpManager;
    QtVariantEditorFactory* mpFactory;
    QtTreePropertyBrowser* mpEditor;
};

}

#endif // REPORTPROPERTYEDITOR_H

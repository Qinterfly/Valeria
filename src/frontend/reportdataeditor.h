#ifndef REPORTDATAEDITOR_H
#define REPORTDATAEDITOR_H

#include <QWidget>

#include "reportinterface.h"
#include "reportitem.h"
#include "uialiasdata.h"

QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QTreeWidget)
QT_FORWARD_DECLARE_CLASS(QListWidget)

class QtTreePropertyBrowser;
class CustomVariantPropertyManager;
class QtVariantEditorFactory;
class QtProperty;

namespace Backend::Core
{
class ReportPage;
}

namespace Frontend
{

class GeometryView;
class ReportCurvePropertyEditor;

//! General class to edit report item data
class ReportDataEditor : public QWidget
{
    Q_OBJECT

public:
    ReportDataEditor(QWidget* pParent = nullptr);
    virtual ~ReportDataEditor() = default;

    virtual Backend::Core::ReportItem::Type type() const = 0;
    virtual void refresh() = 0;
    void setItemGetter(Backend::Core::ReportItemGetter itemGetter);

signals:
    void edited();

protected:
    Backend::Core::ReportItemGetter mItemGetter;
};

//! Class to edit graph item data
class GraphReportDataEditor : public ReportDataEditor
{
    Q_OBJECT

public:
    GraphReportDataEditor(GeometryView* pGeometryView, Backend::Core::ReportPage const& page, QWidget* pParent = nullptr);
    virtual ~GraphReportDataEditor() = default;

    Backend::Core::ReportItem::Type type() const override;
    void refresh() override;

    void addCurve();
    void editSelected();
    void replaceSelectedCurve();
    void removeSelected();
    void removeAllCurves();

private:
    void createContent();
    void createConnections();
    QLayout* createHeaderLayout();
    QWidget* createToolBar();
    QLayout* createCurveLayout();

    // Widgets
    void refreshHeader();
    void refreshTree();
    void closeCurveEditor();

    // Slots
    Backend::Core::GraphReportItem* getItem();
    PairInt getTreeSelected();
    void setTreeSelected(int iCurve, int iPoint = -1);
    void processTreeSelected();
    void processHeaderChanged();
    void processCurveEdited();

    // Help
    Backend::Core::ReportCurveGetter createCurveGetter(int iCurve);

private:
    GeometryView* mpGeometryView;
    Backend::Core::ReportPage const& mPage;

    // Header
    QComboBox* mpSubTypeSelector;
    QComboBox* mpCoordDirSelector;
    QComboBox* mpResponseDirSelector;
    QComboBox* mpUnitSelector;
    QComboBox* mpLinkSelector;

    // Curves
    QTreeWidget* mpCurveTree;
    ReportCurvePropertyEditor* mpCurveEditor;
};

//! Class to edit mode item data
class ModeReportDataEditor : public ReportDataEditor
{
    Q_OBJECT

public:
    ModeReportDataEditor(Backend::Core::ReportPage const& page, QWidget* pParent = nullptr);
    virtual ~ModeReportDataEditor() = default;

    Backend::Core::ReportItem::Type type() const override;
    void refresh() override;

private:
    void createContent();
    void createConnections();

    // Slots
    Backend::Core::ModeReportItem* getItem();
    void processChanged();

private:
    Backend::Core::ReportPage const& mPage;

    // Widgets
    QComboBox* mpUnitSelector;
    QComboBox* mpViewSelector;
    QComboBox* mpColorMapSelector;
    Edit1d* mpScaleEdit;
    Edit1d* mpAmplitudeEdit;
    Edit1d* mpPhaseEdit;
    QComboBox* mpLinkSelector;
};

//! Class to edit diagram item data
class DiagramReportDataEditor : public ReportDataEditor
{
    Q_OBJECT

public:
    DiagramReportDataEditor(GeometryView* pGeometryView, Backend::Core::ReportPage const& page, QWidget* pParent = nullptr);
    virtual ~DiagramReportDataEditor() = default;

    Backend::Core::ReportItem::Type type() const override;
    void refresh() override;

    void addSection();
    void editSection();
    void reverseSection();
    void removeSection();
    void removeAllSections();

private:
    void createContent();
    void createConnections();
    QLayout* createHeaderLayout();
    QWidget* createToolBar();
    QLayout* createSectionLayout();

    // Widgets
    void refreshHeader();
    void refreshList();

    // Slots
    Backend::Core::DiagramReportItem* getItem();
    void processChanged();

private:
    GeometryView* mpGeometryView;
    Backend::Core::ReportPage const& mPage;

    // Widgets
    QComboBox* mpUnitSelector;
    QComboBox* mpViewSelector;
    QComboBox* mpColorMapSelector;
    Edit1d* mpScaleEdit;
    Edit1d* mpAmplitudeEdit;
    Edit1d* mpPhaseEdit;
    QComboBox* mpLinkSelector;

    // Sections
    QListWidget* mpSectionList;
};

//! Class to edit curve properties
class ReportCurvePropertyEditor : public QWidget
{
    Q_OBJECT

public:
    enum Type
    {
        kName,
        kLineStyle,
        kLineWidth,
        kLineColor,
        kMarkerShape,
        kMarkerSize,
        kMarkerFill,
        kMarkerSkip
    };

    ReportCurvePropertyEditor(QWidget* pParent = nullptr);
    virtual ~ReportCurvePropertyEditor() = default;

    void setCurveGetter(Backend::Core::ReportCurveGetter curveGetter);

signals:
    void edited();

protected:
    QSize sizeHint() const override;

private:
    void createContent();
    void createProperties();
    void createConnections();

    void setValue(QtProperty* pProperty, QVariant value);

private:
    Backend::Core::ReportCurveGetter mCurveGetter;
    CustomVariantPropertyManager* mpManager;
    QtVariantEditorFactory* mpFactory;
    QtTreePropertyBrowser* mpEditor;
};
}

#endif // REPORTDATAEDITOR_H

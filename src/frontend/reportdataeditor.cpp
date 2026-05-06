#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QToolBar>

#include <customvariantpropertymanager.h>
#include <qttreepropertybrowser.h>
#include <qtvariantproperty.h>

#include "constants.h"
#include "customlineedit.h"
#include "customplot.h"
#include "geometryview.h"
#include "reportdataeditor.h"
#include "reportdefaults.h"
#include "reportdocument.h"
#include "reportitem.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Backend::Constants;
using namespace Backend::Core;
using namespace Frontend;

// Constants
int const skCurveRole = Qt::UserRole + 1;
int const skPointRole = skCurveRole + 1;

// Helper function
QComboBox* createDirSelector();
QComboBox* createUnitSelector();
QComboBox* createViewSelector();
QComboBox* createColorMapSelector();
void refreshUnitSelector(QComboBox* pSelector, QString const& unit);
void refreshLinkSelector(QComboBox* pSelector, ReportPage const& page, ReportItem* pItem);

ReportDataEditor::ReportDataEditor(QWidget* pParent)
    : QWidget(pParent)
{
}

void ReportDataEditor::setItemGetter(ReportItemGetter itemGetter)
{
    mItemGetter = std::move(itemGetter);
    refresh();
}

GraphReportDataEditor::GraphReportDataEditor(GeometryView* pGeometryView, ReportPage const& page, QWidget* pParent)
    : ReportDataEditor(pParent)
    , mpGeometryView(pGeometryView)
    , mPage(page)
{
    setFont(Utility::getFont());
    createContent();
    createConnections();
}

//! Update the widgets state
void GraphReportDataEditor::refresh()
{
    refreshHeader();
    refreshTree();
}

ReportItem::Type GraphReportDataEditor::type() const
{
    return ReportItem::kGraph;
}

//! Create all the widgets
void GraphReportDataEditor::createContent()
{
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->addLayout(createHeaderLayout());
    pLayout->addWidget(createToolBar());
    pLayout->addLayout(createCurveLayout());
    setLayout(pLayout);
}

//! Specify the connections between widgets
void GraphReportDataEditor::createConnections()
{
    // Header
    connect(mpSubTypeSelector, &QComboBox::currentIndexChanged, this, &GraphReportDataEditor::processHeaderChanged);
    connect(mpCoordDirSelector, &QComboBox::currentIndexChanged, this, &GraphReportDataEditor::processHeaderChanged);
    connect(mpResponseDirSelector, &QComboBox::currentIndexChanged, this, &GraphReportDataEditor::processHeaderChanged);
    connect(mpUnitSelector, &QComboBox::currentIndexChanged, this, &GraphReportDataEditor::processHeaderChanged);
    connect(mpLinkSelector, &QComboBox::currentIndexChanged, this, &GraphReportDataEditor::processHeaderChanged);

    // Curve
    connect(mpCurveTree, &QTreeWidget::itemSelectionChanged, this, &GraphReportDataEditor::processTreeSelected);
    connect(mpCurveTree, &QTreeWidget::itemDoubleClicked, this, &GraphReportDataEditor::editSelected);
    connect(mpCurveEditor, &ReportCurvePropertyEditor::edited, this, &GraphReportDataEditor::processCurveEdited);
}

//! Create the layout of header widgets
QLayout* GraphReportDataEditor::createHeaderLayout()
{
    // Create the widgets
    mpSubTypeSelector = new QComboBox;
    mpCoordDirSelector = createDirSelector();
    mpResponseDirSelector = createDirSelector();
    mpUnitSelector = createUnitSelector();
    mpLinkSelector = new QComboBox;

    // Initialize the widgets
    mpSubTypeSelector->addItem(QString(), GraphReportItem::kNone);
    mpSubTypeSelector->addItem(tr("Re"), GraphReportItem::kReal);
    mpSubTypeSelector->addItem(tr("Im"), GraphReportItem::kImag);
    mpSubTypeSelector->addItem(tr("Multi Re"), GraphReportItem::kMultiReal);
    mpSubTypeSelector->addItem(tr("Multi Im"), GraphReportItem::kMultiImag);
    mpSubTypeSelector->addItem(tr("Freq Amp"), GraphReportItem::kFreqAmp);
    mpSubTypeSelector->addItem(tr("Modeshape"), GraphReportItem::kModeshape);

    // Combine the widgets
    QGridLayout* pLayout = new QGridLayout;
    pLayout->addWidget(new QLabel(tr("Type: ")), 0, 0);
    pLayout->addWidget(mpSubTypeSelector, 0, 1);
    pLayout->addWidget(new QLabel(tr("Unit: ")), 1, 0);
    pLayout->addWidget(mpUnitSelector, 1, 1);
    pLayout->addWidget(new QLabel(tr("Coord dir: ")), 2, 0);
    pLayout->addWidget(mpCoordDirSelector, 2, 1);
    pLayout->addWidget(new QLabel(tr("Response dir: ")), 3, 0);
    pLayout->addWidget(mpResponseDirSelector, 3, 1);
    pLayout->addWidget(new QLabel(tr("Link: ")), 4, 0);
    pLayout->addWidget(mpLinkSelector, 4, 1);
    pLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Preferred), 0, 2);

    return pLayout;
}

//! Create a set of actions to manipulate curves
QWidget* GraphReportDataEditor::createToolBar()
{
    QToolBar* pToolBar = new QToolBar;
    pToolBar->setIconSize(Constants::Size::skToolBarIcon);
    pToolBar->addAction(QIcon(":/icons/data-add.svg"), tr("Add curve"), Qt::SHIFT | Qt::Key_A, this, &GraphReportDataEditor::addCurve);
    pToolBar->addAction(QIcon(":/icons/data-edit.svg"), tr("Edit object"), Qt::SHIFT | Qt::Key_E, this, &GraphReportDataEditor::editSelected);
    pToolBar->addAction(QIcon(":/icons/data-replace.svg"), tr("Replace curve"), Qt::SHIFT | Qt::Key_Q, this,
                        &GraphReportDataEditor::replaceSelectedCurve);
    pToolBar->addAction(QIcon(":/icons/data-remove.svg"), tr("Remove object"), Qt::SHIFT | Qt::Key_D, this,
                        &GraphReportDataEditor::removeSelected);
    pToolBar->addAction(QIcon(":/icons/data-clear.png"), tr("Remove all curves"), this, &GraphReportDataEditor::removeAllCurves);
    Utility::setShortcutHints(pToolBar);
    return pToolBar;
}

//! Create the layout of curve widgets
QLayout* GraphReportDataEditor::createCurveLayout()
{
    // Constants
    QSize const kIconSize(32, 32);

    // Create the widgets
    mpCurveTree = new QTreeWidget;
    mpCurveEditor = new ReportCurvePropertyEditor(this);

    // Initialize the tree
    mpCurveTree->setFont(font());
    mpCurveTree->setSelectionMode(QListWidget::SingleSelection);
    mpCurveTree->setIconSize(kIconSize);
    mpCurveTree->setHeaderHidden(true);
    mpCurveTree->setColumnCount(1);

    // Initialize the editor
    mpCurveEditor->hide();

    // Combine the widgets
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->addWidget(mpCurveTree);

    return pLayout;
}

//! Update the header widgets
void GraphReportDataEditor::refreshHeader()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Set the subtype
    QSignalBlocker blockerSubType(mpSubTypeSelector);
    Utility::setIndexByKey(mpSubTypeSelector, pItem->subType);

    // Set the unit
    QSignalBlocker blockerUnit(mpUnitSelector);
    refreshUnitSelector(mpUnitSelector, pItem->unit);

    // Set the coordinate direction
    QSignalBlocker blockerCoordDir(mpCoordDirSelector);
    Utility::setIndexByKey(mpCoordDirSelector, (int) pItem->coordDir);

    // Set the response direction
    QSignalBlocker blockerResponseDir(mpResponseDirSelector);
    Utility::setIndexByKey(mpResponseDirSelector, (int) pItem->responseDir);

    // Set the link
    QSignalBlocker blockerLink(mpLinkSelector);
    refreshLinkSelector(mpLinkSelector, mPage, pItem);
}

//! Update the hierarchy widgets
void GraphReportDataEditor::refreshTree()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Set the curve tree
    QSignalBlocker blockerCurveTree(mpCurveTree);
    mpCurveTree->setEnabled(pItem->link.isNull());
    auto [iCurve, iPoint] = getTreeSelected();
    mpCurveTree->clear();
    int numCurves = pItem->curves.size();
    for (int iCurve = 0; iCurve != numCurves; ++iCurve)
    {
        ReportCurve const& curve = pItem->curves[iCurve];
        QTreeWidgetItem* pCurveItem = new QTreeWidgetItem;
        pCurveItem->setFont(0, font());

        // Construct the name
        QString name = curve.name;
        if (name.isEmpty())
            name = curve.points.size() == 1 ? curve.points.first().name() : tr("Curve %1").arg(1 + iCurve);
        pCurveItem->setText(0, name);
        pCurveItem->setData(0, skCurveRole, iCurve);
        pCurveItem->setData(0, skPointRole, -1);

        // Construct the icon
        if (pItem->subType != GraphReportItem::kMultiReal && pItem->subType != GraphReportItem::kMultiImag)
        {
            QPen pen(curve.lineColor, curve.lineWidth, curve.lineStyle);
            QCPScatterStyle style((QCPScatterStyle::ScatterShape) curve.markerShape, curve.markerSize);
            if (curve.markerFill)
                style.setBrush(curve.lineColor);
            style.setPen(pen);
            bool isLine = curve.lineStyle != Qt::NoPen;
            bool isMarker = curve.markerShape != ReportMarkerShape::kNone;
            QIcon icon = Utility::getIcon(style, mpCurveTree->iconSize(), isLine, isMarker);
            pCurveItem->setIcon(0, icon);
        }

        // Add the points
        int numPoints = curve.points.size();
        for (int iPoint = 0; iPoint != numPoints; ++iPoint)
        {
            QTreeWidgetItem* pPointItem = new QTreeWidgetItem;
            pPointItem->setText(0, curve.points[iPoint].name());
            pPointItem->setFont(0, font());
            pPointItem->setData(0, skCurveRole, iCurve);
            pPointItem->setData(0, skPointRole, iPoint);
            pCurveItem->addChild(pPointItem);
        }

        // Add the item to the tree
        mpCurveTree->addTopLevelItem(pCurveItem);
    }
    setTreeSelected(iCurve, iPoint);
}

//! Hide the curve editor
void GraphReportDataEditor::closeCurveEditor()
{
    if (!mpCurveEditor->isVisible())
        return;
    mpCurveEditor->setCurveGetter(ReportCurveGetter());
    mpCurveEditor->hide();
}

//! Add a new curve
void GraphReportDataEditor::addCurve()
{
    // Constants
    QList<ReportCurve> const kDefaultCurves = ReportDefaults::curves();

    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Helper function
    auto createCurve = [pItem, &kDefaultCurves](QList<ReportPoint> const& points)
    {
        int iDefaultCurve = Utility::getRepeatedIndex(pItem->curves.count(), kDefaultCurves.size());
        ReportCurve curve = kDefaultCurves[iDefaultCurve];
        if (points.size() == 1)
            curve.name = points.first().name();
        else
            curve.name = tr("Curve %1").arg(pItem->curves.size() + 1);
        curve.points = points;
        pItem->curves.push_back(curve);
    };

    // Add the curve
    QList<ReportPoint> selectedPoints = getSelectedPoints();
    if (!selectedPoints.isEmpty())
    {
        if (pItem->isMultiPointCurve())
        {
            createCurve(selectedPoints);
        }
        else
        {
            for (ReportPoint const& p : std::as_const(selectedPoints))
                createCurve({p});
        }
        qInfo() << tr("Curve consisted of %1 points is added").arg(selectedPoints.size());
    }
    else
    {
        qWarning() << tr("Cannot add a curve, since there are no selected points");
    }

    // Update the widgets content
    refresh();

    // Select the last curve
    if (!selectedPoints.isEmpty())
        setTreeSelected(pItem->curves.size() - 1);

    // Finish up the editing
    emit edited();
}

//! Edit the selected curve
void GraphReportDataEditor::editSelected()
{
    // Retrieve the selected curve
    auto [iCurve, iPoint] = getTreeSelected();
    if (iCurve < 0)
        return;

    // Show the editor
    ReportCurveGetter curveGetter = createCurveGetter(iCurve);
    ReportCurve* pCurve = curveGetter();
    if (iPoint < 0)
    {
        mpCurveEditor->setCurveGetter(curveGetter);
        mpCurveEditor->show();
    }
    else if (pCurve && iPoint < pCurve->points.size())
    {
        bool isOk;
        ReportPoint& point = pCurve->points[iPoint];
        QString name = QInputDialog::getText(this, tr("Change point"), tr("New point: "), QLineEdit::Normal, point.name(), &isOk);
        if (isOk)
        {
            point = ReportPoint(name);
            if (pCurve->points.size() == 1)
                pCurve->name = name;
            refreshTree();
            emit edited();
        }
    }
}

//! Replace the selected curve with the new one
void GraphReportDataEditor::replaceSelectedCurve()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Construct the dialog
    auto reply = QMessageBox::question(this, tr("Replace Curve"),
                                       tr("Are you sure that you want to replace all the curve points with the selectd ones?"),
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    // Retrieve the selected curve
    auto [iCurve, iPoint] = getTreeSelected();
    if (iCurve < 0)
        return;

    // Replace the current curve
    QList<ReportPoint> selectedPoints = getSelectedPoints();
    if (iCurve >= 0 && iCurve < pItem->curves.size())
    {
        if (!selectedPoints.isEmpty())
        {
            ReportCurve curve;
            if (pItem->isMultiPointCurve())
                curve = ReportCurve(selectedPoints);
            else
                curve = ReportCurve({selectedPoints.first()});
            pItem->curves[iCurve].points = curve.points;
            qInfo() << tr("Curve is replaced");
        }
        else
        {
            qWarning() << tr("Cannot replace the curve, since there are no selected points");
        }
    }

    // Update the widgets content
    refresh();

    // Finish up the editing
    emit edited();
}

//! Process removing the current curve
void GraphReportDataEditor::removeSelected()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Close the curve editor
    closeCurveEditor();

    // Remove the selected curve
    auto [iCurve, iPoint] = getTreeSelected();
    bool isCurve = iPoint < 0;
    if (iCurve >= 0 && iCurve < pItem->curves.size())
    {
        if (isCurve)
        {
            pItem->curves.remove(iCurve);
            qInfo() << tr("Curve is successfully removed");
        }
        else
        {
            pItem->curves[iCurve].points.remove(iPoint);
            qInfo() << tr("Point is successfully removed");
        }
    }

    // Update the widgets content
    refresh();

    // Select the next curve
    setTreeSelected(iCurve);

    // Finish up the editing
    emit edited();
}

//! Remove all the curves
void GraphReportDataEditor::removeAllCurves()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Show the dialog
    auto answer = QMessageBox::question(this, tr("Remove all curves"), tr("Are you sure you want to remove all the curves?"));
    if (answer != QMessageBox::Yes)
        return;

    // Remove all the curves
    pItem->curves.clear();

    // Update the widgets content
    refresh();

    // Finish up the editing
    emit edited();
}

//! Retrieve the selected points from the geometry view
QList<ReportPoint> GraphReportDataEditor::getSelectedPoints()
{
    auto selections = mpGeometryView->selectionPairs();
    int numSelections = selections.size();
    QList<ReportPoint> result(numSelections);
    for (int i = 0; i != numSelections; ++i)
    {
        auto selection = selections[i];
        result[i] = ReportPoint(selection.first, selection.second);
    }
    return result;
}

//! Process changing tree selection
void GraphReportDataEditor::processTreeSelected()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Update the header
    refreshHeader();

    // Highlight the selected curve
    auto [iCurve, iPoint] = getTreeSelected();
    if (iCurve >= 0 && iCurve < pItem->curves.size())
    {
        ReportCurve const& curve = pItem->curves[iCurve];
        if (curve.isEmpty())
            return;
        int numPoints = curve.points.size();
        mpGeometryView->clearSelection();
        bool isCurve = iPoint < 0;
        if (isCurve)
        {
            for (int i = 0; i != numPoints; ++i)
            {
                ReportPoint const& point = curve.points[i];
                mpGeometryView->addSelection(point.component, point.node);
            }
        }
        else
        {
            ReportPoint const& point = curve.points[iPoint];
            mpGeometryView->addSelection(point.component, point.node);
        }
        mpGeometryView->refresh();
    }
}

//! Get the graph item
GraphReportItem* GraphReportDataEditor::getItem()
{
    if (!mItemGetter)
        return nullptr;
    return (GraphReportItem*) mItemGetter();
}

//! Retrieve the selected tree entity: curve or point
PairInt GraphReportDataEditor::getTreeSelected()
{
    PairInt const null(-1, -1);
    QTreeWidgetItem* pCurrent = mpCurveTree->currentItem();
    if (!pCurrent)
        return null;
    int iPoint = pCurrent->data(0, skPointRole).toInt();
    int iCurve = pCurrent->data(0, skCurveRole).toInt();
    return {iCurve, iPoint};
}

//! Set the selected tree item
void GraphReportDataEditor::setTreeSelected(int iCurve, int iPoint)
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Select the curve item
    if (iCurve >= 0 && iCurve < pItem->curves.size())
    {
        QTreeWidgetItem* pCurveItem = mpCurveTree->topLevelItem(iCurve);
        if (pCurveItem && iPoint >= 0)
        {
            pCurveItem->setExpanded(true);
            mpCurveTree->setCurrentItem(pCurveItem->child(iPoint));
        }
        else
        {
            mpCurveTree->setCurrentItem(pCurveItem);
        }
    }
}

//! Process changing header of the item
void GraphReportDataEditor::processHeaderChanged()
{
    // Get the item
    GraphReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Set the item data
    pItem->subType = (GraphReportItem::SubType) mpSubTypeSelector->currentData().toInt();
    pItem->coordDir = (ReportDirection) mpCoordDirSelector->currentData().toInt();
    pItem->responseDir = (ReportDirection) mpResponseDirSelector->currentData().toInt();
    pItem->unit = mpUnitSelector->currentData().toString();
    pItem->link = mpLinkSelector->currentData().toUuid();

    // Update the content
    refreshTree();

    // Finish up the editing
    emit edited();
}

//! Process changing curve
void GraphReportDataEditor::processCurveEdited()
{
    refreshTree();
    emit edited();
}

//! Create the functor to obtain curves of the specified index
ReportCurveGetter GraphReportDataEditor::createCurveGetter(int iCurve)
{
    return [this, iCurve]()
    {
        GraphReportItem* pItem = getItem();
        if (pItem && iCurve >= 0 && iCurve < pItem->curves.size())
            return &pItem->curves[iCurve];
        return (ReportCurve*) nullptr;
    };
}

ModeReportDataEditor::ModeReportDataEditor(ReportPage const& page, QWidget* pParent)
    : ReportDataEditor(pParent)
    , mPage(page)
{
    setFont(Utility::getFont());
    createContent();
    createConnections();
}

//! Get the editor type
ReportItem::Type ModeReportDataEditor::type() const
{
    return ReportItem::kMode;
}

//! Update the widgets content
void ModeReportDataEditor::refresh()
{
    // Get the item
    ModeReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Set the unit
    QSignalBlocker blockerUnit(mpUnitSelector);
    refreshUnitSelector(mpUnitSelector, pItem->unit);

    // Set the view
    QSignalBlocker blockerView(mpViewSelector);
    Utility::setIndexByKey(mpViewSelector, (int) pItem->view);

    // Set the color map
    QSignalBlocker blockerColorMap(mpColorMapSelector);
    Utility::setIndexByKey(mpColorMapSelector, (int) pItem->colorMap);

    // Set the scale
    QSignalBlocker blockerScale(mpScaleEdit);
    mpScaleEdit->setValue(pItem->scale);

    // Set the amplitude
    QSignalBlocker blockerAmplitude(mpAmplitudeEdit);
    mpAmplitudeEdit->setValue(pItem->amplitude);

    // Set the phase
    QSignalBlocker blockerPhase(mpPhaseEdit);
    mpPhaseEdit->setValue(pItem->phase);

    // Set the link
    QSignalBlocker blockerLink(mpLinkSelector);
    refreshLinkSelector(mpLinkSelector, mPage, pItem);
}

//! Create all the widgets
void ModeReportDataEditor::createContent()
{
    // Create the widgets
    mpUnitSelector = createUnitSelector();
    mpViewSelector = createViewSelector();
    mpColorMapSelector = createColorMapSelector();
    mpScaleEdit = new Edit1d;
    mpAmplitudeEdit = new Edit1d;
    mpPhaseEdit = new Edit1d;
    mpLinkSelector = new QComboBox;

    // Initialize the widgets
    mpScaleEdit->setMinimum(0.0);

    // Combine the widgets
    QGridLayout* pLayout = new QGridLayout;
    pLayout->addWidget(new QLabel(tr("Unit: ")), 0, 0);
    pLayout->addWidget(mpUnitSelector, 0, 1);
    pLayout->addWidget(new QLabel(tr("View: ")), 1, 0);
    pLayout->addWidget(mpViewSelector, 1, 1);
    pLayout->addWidget(new QLabel(tr("Color map: ")), 2, 0);
    pLayout->addWidget(mpColorMapSelector, 2, 1);
    pLayout->addWidget(new QLabel(tr("Scale: ")), 3, 0);
    pLayout->addWidget(mpScaleEdit, 3, 1);
    pLayout->addWidget(new QLabel(tr("Amplitude: ")), 4, 0);
    pLayout->addWidget(mpAmplitudeEdit, 4, 1);
    pLayout->addWidget(new QLabel(tr("Phase: ")), 5, 0);
    pLayout->addWidget(mpPhaseEdit, 5, 1);
    pLayout->addWidget(new QLabel(tr("Link: ")), 6, 0);
    pLayout->addWidget(mpLinkSelector, 6, 1);
    pLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Preferred), 0, 2);
    pLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Preferred, QSizePolicy::Expanding), 7, 0);
    setLayout(pLayout);
}

//! Set the widget connections
void ModeReportDataEditor::createConnections()
{
    connect(mpUnitSelector, &QComboBox::currentIndexChanged, this, &ModeReportDataEditor::processChanged);
    connect(mpViewSelector, &QComboBox::currentIndexChanged, this, &ModeReportDataEditor::processChanged);
    connect(mpColorMapSelector, &QComboBox::currentIndexChanged, this, &ModeReportDataEditor::processChanged);
    connect(mpScaleEdit, &Edit1d::valueChanged, this, &ModeReportDataEditor::processChanged);
    connect(mpAmplitudeEdit, &Edit1d::valueChanged, this, &ModeReportDataEditor::processChanged);
    connect(mpPhaseEdit, &Edit1d::valueChanged, this, &ModeReportDataEditor::processChanged);
    connect(mpLinkSelector, &QComboBox::currentIndexChanged, this, &ModeReportDataEditor::processChanged);
}

//! Get the item of the requested type
ModeReportItem* ModeReportDataEditor::getItem()
{
    if (!mItemGetter)
        return nullptr;
    return (ModeReportItem*) mItemGetter();
}

//! Process item data changing
void ModeReportDataEditor::processChanged()
{
    // Get the item
    ModeReportItem* pItem = getItem();
    if (!pItem)
        return;

    // Set the item data
    pItem->unit = mpUnitSelector->currentData().toString();
    pItem->view = (ReportView) mpViewSelector->currentData().toInt();
    pItem->colorMap = (ReportColorMap) mpColorMapSelector->currentData().toInt();
    pItem->scale = mpScaleEdit->value();
    pItem->amplitude = mpAmplitudeEdit->value();
    pItem->phase = mpPhaseEdit->value();
    pItem->link = mpLinkSelector->currentData().toUuid();

    // Update the content
    refresh();

    // Finish up the editing
    emit edited();
}

ReportCurvePropertyEditor::ReportCurvePropertyEditor(QWidget* pParent)
{
    setWindowTitle(tr("Curve Editor"));
    setFont(Utility::getFont());
    createContent();
    createConnections();
}

//! Set the curve for editing
void ReportCurvePropertyEditor::setCurveGetter(ReportCurveGetter curveGetter)
{
    mCurveGetter = std::move(curveGetter);
    createProperties();
}

QSize ReportCurvePropertyEditor::sizeHint() const
{
    return QSize(400, 300);
}

//! Create all the widgets
void ReportCurvePropertyEditor::createContent()
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

//! Create the plottable properties
void ReportCurvePropertyEditor::createProperties()
{
    QStringList const kLineStyleNames = {QString(), tr("Solid"), tr("Dash"), tr("Dotted"), tr("Dash-dotted")};
    QStringList const kMarkerShapeNames = {QString(),
                                           tr("Dot"),
                                           tr("Cross"),
                                           tr("Plus"),
                                           tr("Circle"),
                                           tr("Disc"),
                                           tr("Square"),
                                           tr("Diamond"),
                                           tr("Star"),
                                           tr("Triangle"),
                                           tr("Inverted Triangle"),
                                           tr("Cross Square"),
                                           tr("Plus Square"),
                                           tr("Cross Circle"),
                                           tr("Plus Circle"),
                                           tr("Peace")};

    // Remove the previous properties
    QSignalBlocker blockerEditor(mpEditor);
    QSignalBlocker blockerManager(mpManager);
    mpEditor->clear();
    mpManager->clear();

    // Get the curve
    if (!mCurveGetter)
        return;
    ReportCurve* pCurve = mCurveGetter();
    if (!pCurve)
        return;

    // Create the properties
    QtVariantProperty* pNameProperty = mpManager->addProperty(kName, QMetaType::QString, tr("Name"));
    pNameProperty->setValue(pCurve->name);
    mpEditor->addProperty(pNameProperty);

    QtVariantProperty* pLineStyleProperty = mpManager->addProperty(kLineStyle, QtVariantPropertyManager::enumTypeId(), tr("Line style"));
    pLineStyleProperty->setAttribute("enumNames", kLineStyleNames);
    pLineStyleProperty->setValue((int) pCurve->lineStyle);
    mpEditor->addProperty(pLineStyleProperty);

    QtVariantProperty* pLineWidthProperty = mpManager->addProperty(kLineWidth, QMetaType::Double, tr("Line width"));
    pLineWidthProperty->setValue(pCurve->lineWidth);
    mpEditor->addProperty(pLineWidthProperty);

    QtVariantProperty* pLineColorProperty = mpManager->addProperty(kLineColor, QMetaType::QColor, tr("Line color"));
    pLineColorProperty->setValue(pCurve->lineColor);
    QtBrowserItem* pLineColorItem = mpEditor->addProperty(pLineColorProperty);
    mpEditor->setExpanded(pLineColorItem, false);

    QtVariantProperty* pMarkerShapeProperty = mpManager->addProperty(kMarkerShape, QtVariantPropertyManager::enumTypeId(), tr("Marker shape"));
    pMarkerShapeProperty->setAttribute("enumNames", kMarkerShapeNames);
    pMarkerShapeProperty->setValue((QCPScatterStyle::ScatterShape) pCurve->markerShape);
    mpEditor->addProperty(pMarkerShapeProperty);

    QtVariantProperty* pMarkerSizeProperty = mpManager->addProperty(kMarkerSize, QMetaType::Double, tr("Marker size"));
    pMarkerSizeProperty->setValue(pCurve->markerSize);
    mpEditor->addProperty(pMarkerSizeProperty);

    QtVariantProperty* pMarkerFillProperty = mpManager->addProperty(kMarkerFill, QMetaType::Bool, tr("Marker fill"));
    pMarkerFillProperty->setValue(pCurve->markerFill);
    mpEditor->addProperty(pMarkerFillProperty);

    QtVariantProperty* pMarkerSkipProperty = mpManager->addProperty(kMarkerSkip, QMetaType::Int, tr("Marker skip"));
    pMarkerSkipProperty->setValue(pCurve->markerSkip);
    mpEditor->addProperty(pMarkerSkipProperty);
}

//! Specify connections
void ReportCurvePropertyEditor::createConnections()
{
    connect(mpManager, &CustomVariantPropertyManager::valueChanged, this, &ReportCurvePropertyEditor::setValue);
}

//! Change the plottable property value
void ReportCurvePropertyEditor::setValue(QtProperty* pProperty, QVariant value)
{
    // Constants
    QSet<ReportMarkerShape> kPlainMarkers = {ReportMarkerShape::kDot,         ReportMarkerShape::kCross,       ReportMarkerShape::kPlus,
                                             ReportMarkerShape::kStar,        ReportMarkerShape::kCrossSquare, ReportMarkerShape::kPlusSquare,
                                             ReportMarkerShape::kCrossCircle, ReportMarkerShape::kPlusCircle};
    // Get the curve
    if (!mCurveGetter)
        return;
    ReportCurve* pCurve = mCurveGetter();
    if (!pCurve)
        return;

    // Get the property id
    if (!mpManager->contains(pProperty))
        return;
    int id = mpManager->id(pProperty);

    // Set property value
    switch (id)
    {
    case kName:
        pCurve->name = value.toString();
        break;
    case kLineStyle:
        pCurve->lineStyle = (Qt::PenStyle) value.toInt();
        break;
    case kLineWidth:
        pCurve->lineWidth = value.toDouble();
        break;
    case kLineColor:
        pCurve->lineColor = value.value<QColor>();
        break;
    case kMarkerShape:
        pCurve->markerShape = (ReportMarkerShape) value.toInt();
        if (kPlainMarkers.contains(pCurve->markerShape))
            pCurve->markerFill = false;
        break;
    case kMarkerSize:
        pCurve->markerSize = value.toDouble();
        break;
    case kMarkerFill:
        pCurve->markerFill = value.toBool();
        break;
    case kMarkerSkip:
        pCurve->markerSkip = value.toInt();
        break;
    }
    emit edited();
}

//! Helper function to create a combobox with predefined directions
QComboBox* createDirSelector()
{
    QComboBox* pResult = new QComboBox;
    pResult->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pResult->addItem(QString(), (int) ReportDirection::kNone);
    pResult->addItem("X", (int) ReportDirection::kX);
    pResult->addItem("Y", (int) ReportDirection::kY);
    pResult->addItem("Z", (int) ReportDirection::kZ);
    return pResult;
}

//! Helper function to create a combobox with predefined units
QComboBox* createUnitSelector()
{
    QComboBox* pResult = new QComboBox;
    pResult->addItem(QString());
    pResult->addItem(QObject::tr("m/s%1").arg(QChar(0x00B2)), Units::skM_S2);
    pResult->addItem(QObject::tr("(m/s%1)/N").arg(QChar(0x00B2)), Units::skM_S2_N);
    pResult->addItem(QObject::tr("m"), Units::skM);
    pResult->addItem(QObject::tr("mm/s%1").arg(QChar(0x00B2)), Units::skMM_S2);
    pResult->addItem(QObject::tr("(mm/s%1)/N").arg(QChar(0x00B2)), Units::skMM_S2_N);
    pResult->addItem(QObject::tr("mm"), Units::skMM);
    return pResult;
}

//! Helper function to create a combobox with predefined views
QComboBox* createViewSelector()
{
    QComboBox* pResult = new QComboBox;
    pResult->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pResult->addItem(QObject::tr("Front"), (int) ReportView::kFront);
    pResult->addItem(QObject::tr("Rear"), (int) ReportView::kRear);
    pResult->addItem(QObject::tr("Top"), (int) ReportView::kTop);
    pResult->addItem(QObject::tr("Bottom"), (int) ReportView::kBottom);
    pResult->addItem(QObject::tr("Left"), (int) ReportView::kLeft);
    pResult->addItem(QObject::tr("Right"), (int) ReportView::kRight);
    pResult->addItem(QObject::tr("Isometric"), (int) ReportView::kIsometric);
    return pResult;
}

//! Helper function to create a combobox with predefined color maps
QComboBox* createColorMapSelector()
{
    QComboBox* pResult = new QComboBox;
    pResult->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    pResult->addItem(QObject::tr("Cool-warm"), (int) ReportColorMap::kCoolToWarm);
    pResult->addItem(QObject::tr("Blue-red 1"), (int) ReportColorMap::kBlueToRed1);
    pResult->addItem(QObject::tr("Blue-red 2"), (int) ReportColorMap::kBlueToRed2);
    pResult->addItem(QObject::tr("Blue-red 3"), (int) ReportColorMap::kBlueToRed3);
    pResult->addItem(QObject::tr("Varadis"), (int) ReportColorMap::kVaradis);
    pResult->addItem(QObject::tr("Jet"), (int) ReportColorMap::kJet);
    pResult->addItem(QObject::tr("Plasma"), (int) ReportColorMap::kPlasma);
    return pResult;
}

//! Helper function to refresh unit selector
void refreshUnitSelector(QComboBox* pSelector, QString const& unit)
{
    int numUnits = pSelector->count();
    int iUnit = -1;
    for (int i = 0; i != numUnits; ++i)
    {
        if (pSelector->itemData(i).toString() == unit)
        {
            iUnit = i;
            break;
        }
    }
    pSelector->setCurrentIndex(iUnit);
}

//! Helper function to refresh link selector
void refreshLinkSelector(QComboBox* pSelector, ReportPage const& page, ReportItem* pItem)
{
    pSelector->clear();
    int numItems = page.count();
    pSelector->addItem(QString(), QUuid());
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem const* pAnotherItem = page.get(i);
        if (pItem->type() != pAnotherItem->type())
            continue;
        if (pItem->id == pAnotherItem->id)
            continue;
        pSelector->addItem(pAnotherItem->name, pAnotherItem->id);
        if (pItem->link == pAnotherItem->id)
            pSelector->setCurrentIndex(pSelector->count() - 1);
    }
}

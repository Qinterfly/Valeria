#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPrinter>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <vtkRenderWindow.h>

#include "constants.h"
#include "customtabwidget.h"
#include "diagramreportsceneitem.h"
#include "geometryview.h"
#include "graphreportsceneitem.h"
#include "modereportsceneitem.h"
#include "reportdataeditor.h"
#include "reportdesigner.h"
#include "reportpropertyeditor.h"
#include "reportsceneitem.h"
#include "sessioneditor.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Backend::Constants;
using namespace Backend::Core;
using namespace Frontend;

// Helper functions
QListWidgetItem* createListItem(ReportPage const& page, int index);

ReportDesignerOptions::ReportDesignerOptions()
{
    // Flags
    lockItems = true;
    isValid = true;
}

ReportDesigner::ReportDesigner(QSettings& settings, GeometryView* pGeometryView, ResponseEditor* pResponseEditor, ReportPage& page,
                               ReportTextEngine const& textEngine, ReportDesignerOptions const& options, QWidget* pParent)
    : QWidget(pParent)
    , mSettings(settings)
    , mpGeometryView(pGeometryView)
    , mpResponseEditor(pResponseEditor)
    , mPage(page)
    , mTextEngine(textEngine)
    , mOptions(options)
{
    setFont(Utility::getFont());
    createContent();
    createConnections();
    refresh();
}

ReportDesigner::~ReportDesigner()
{
    auto keys = mRenderWindows.keys();
    for (auto k : keys)
        mRenderWindows[k]->Delete();
    mRenderWindows.clear();
}

//! Get the scene
QGraphicsScene* ReportDesigner::scene()
{
    return mpScene;
}

//! Get the page
ReportPage& ReportDesigner::page()
{
    return mPage;
}

//! Get the options
ReportDesignerOptions const& ReportDesigner::options() const
{
    return mOptions;
}

//! Fit the content
void ReportDesigner::fit()
{
    mpSceneView->resetTransform();
    mpSceneView->fitToPage();
}

//! Render the page content
void ReportDesigner::refresh()
{
    drawAll();
    refreshList();
    refreshEditor();
}

//! Render the scene
void ReportDesigner::render(bool isPrint)
{
    mIsPrinting = isPrint;
    drawAll();
    mIsPrinting = false;
}

//! Print the page content to a pdf file
bool ReportDesigner::print(QPrinter& printer, QPainter& painter)
{
    // Save the painter state
    painter.save();

    // Set the view
    mpSceneView->fitToPage();

    // Change the orientation, if necessary
    if (mPage.layout.orientation() == QPageLayout::Landscape)
    {
        QRectF target = printer.pageRect(QPrinter::Millimeter);
        QRectF source = mpScene->sceneRect();

        // Rotate coordinate system
        painter.translate(0, target.height());
        painter.rotate(-90);

        // Scale to fit
        qreal scale = target.height() / source.height();
        painter.scale(scale, scale);

        // Center it
        painter.translate(-scale * target.height(), 0.0);
        painter.translate(-source.width() / 2.0, 0.0);
    }

    // Render to the painter
    mpScene->render(&painter);

    // Restore the painter state
    painter.restore();

    return true;
}

//! Select a report item by its index
void ReportDesigner::selectItem(int index)
{
    ReportItem* pItem = mScenePage.get(index);
    if (!pItem)
        return;
    mSelectedItemIDs = {pItem->id};
    refresh();
}

//! Update the text engine from the source
void ReportDesigner::setTextEngine(Backend::Core::ReportTextEngine const& textEngine)
{
    mTextEngine = textEngine;
    refresh();
}

//! Draw all the graphic objects
void ReportDesigner::drawAll()
{
    // Set the scene page
    mScenePage = mPage;

    // Set up the scene
    QSignalBlocker blockerScene(mpScene);
    QSignalBlocker blockerSceneView(mpSceneView);
    mpScene->clear();
    mpScene->setSceneRect(mScenePage.layout.paintRect());

    // Skip rendering the content if the page is not valid
    if (!mOptions.isValid)
    {
        drawCross();
        if (!mIsPrinting)
            drawBorder();
        return;
    }

    // Set the text engine
    updateTextEngine();

    // Resolve item links
    resolveItemLinks();

    // Draw all the objects
    drawItems();
    if (!mIsPrinting)
        drawBorder();
}

//! Draw the report items
void ReportDesigner::drawItems()
{
    int numItems = mScenePage.count();
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem* pReportItem = mScenePage.get(i);

        // Create the item
        ReportSceneItem* pSceneItem = nullptr;
        switch (pReportItem->type())
        {
        case ReportItem::kText:
            pSceneItem = new TextReportSceneItem((TextReportItem*) pReportItem, mTextEngine);
            break;
        case ReportItem::kGraph:
            pSceneItem = new GraphReportSceneItem((GraphReportItem*) pReportItem, mTextEngine, mpResponseEditor->collection(),
                                                  mpResponseEditor->iSelectedBundle(), mpGeometryView->getGeometry());
            break;
        case ReportItem::kPicture:
            pSceneItem = new PictureReportSceneItem((PictureReportItem*) pReportItem);
            break;
        case ReportItem::kTable:
            pSceneItem = new TableReportSceneItem((TableReportItem*) pReportItem, mTextEngine);
            break;
        case ReportItem::kMode:
            pSceneItem = new ModeReportSceneItem((ModeReportItem*) pReportItem, mTextEngine, mpResponseEditor->collection(),
                                                 mpResponseEditor->iSelectedBundle(), mpGeometryView->getGeometry(),
                                                 createRenderWindow(pReportItem->id));
            break;
        case ReportItem::kDiagram:
            pSceneItem = new DiagramReportSceneItem((DiagramReportItem*) pReportItem, mTextEngine, mpResponseEditor->collection(),
                                                    mpResponseEditor->iSelectedBundle(), mpGeometryView->getGeometry(),
                                                    createRenderWindow(pReportItem->id));
            break;
        default:
            break;
        }
        if (!pSceneItem)
            continue;

        // Set the item flags and connections
        if (mIsPrinting)
        {
            pSceneItem->setFlag(QGraphicsItem::ItemIsMovable, false);
            pSceneItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        }
        else
        {
            pSceneItem->setFlag(QGraphicsItem::ItemIsMovable, !mOptions.lockItems);
            pSceneItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
            connect(pSceneItem, &ReportSceneItem::changed, this, &ReportDesigner::processSceneItemChanged);
            connect(pSceneItem, &ReportSceneItem::requestEdit, this, [this, pSceneItem]() { processEditItemRequest(pSceneItem); });
            if (mSelectedItemIDs.contains(pSceneItem->item()->id))
                pSceneItem->setSelected(true);
        }

        // Add the item to the scene
        mpScene->addItem(pSceneItem);
    }
}

//! Draw the cross if the page is not valid
void ReportDesigner::drawCross()
{
    QRectF rect = mpScene->sceneRect();
    rect = rect.marginsRemoved(QMarginsF(5, 5, 5, 5));
    QPen pen(Qt::red, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    mpScene->addLine(rect.topLeft().x(), rect.topLeft().y(), rect.bottomRight().x(), rect.bottomRight().y(), pen);
    mpScene->addLine(rect.topRight().x(), rect.topRight().y(), rect.bottomLeft().x(), rect.bottomLeft().y(), pen);
}

//! Draw the border around the page
void ReportDesigner::drawBorder()
{
    QGraphicsRectItem* pBorder = mpScene->addRect(mpScene->sceneRect(), QPen(Qt::black, 0), QBrush(Qt::white));
    pBorder->setZValue(-1000);
    pBorder->setFlag(QGraphicsItem::ItemIsSelectable, false);
    pBorder->setFlag(QGraphicsItem::ItemIsMovable, false);
    mpSceneView->setBackgroundBrush(QColor(210, 210, 210));
}

//! Update the list of page items
void ReportDesigner::refreshList()
{
    QSignalBlocker blocker(mpItemList);
    mpItemList->clear();
    int numItems = mScenePage.count();
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem* pReportItem = mScenePage.get(i);
        QListWidgetItem* pListItem = createListItem(mScenePage, i);
        mpItemList->addItem(pListItem);
        if (mSelectedItemIDs.contains(pReportItem->id))
            pListItem->setSelected(true);
    }
}

//! Update the item editor
void ReportDesigner::refreshEditor()
{
    ReportItem* pItem = nullptr;
    if (mSelectedItemIDs.size() == 1)
        pItem = mScenePage.get(mSelectedItemIDs.first());
    QUuid itemID = pItem ? pItem->id : QUuid();
    mpPropertyEditor->setItemGetter(createItemGetter(itemID));
    setDataEditor(pItem);
}

//! Add a report item of the specified type
void ReportDesigner::addItem(ReportItem::Type type)
{
    // Create the item and initialize it
    ReportItem* pItem = nullptr;
    switch (type)
    {
    case ReportItem::kText:
        pItem = new TextReportItem;
        pItem->rect = QRect(80, 30, 50, 50);
        break;
    case ReportItem::kGraph:
        pItem = new GraphReportItem;
        pItem->rect = QRect(50, 50, 100, 100);
        break;
    case ReportItem::kPicture:
        pItem = new PictureReportItem;
        pItem->rect = QRect(50, 70, 100, 100);
        break;
    case ReportItem::kTable:
        pItem = new TableReportItem;
        pItem->rect = QRect(50, 90, 100, 100);
        break;
    case ReportItem::kMode:
        pItem = new ModeReportItem;
        pItem->rect = QRect(50, 110, 100, 100);
        break;
    case ReportItem::kDiagram:
        pItem = new DiagramReportItem;
        pItem->rect = QRect(50, 130, 100, 100);
        break;
    default:
        break;
    }
    if (!pItem)
        return;

    // Add it to the page
    mScenePage.add(pItem);

    // Select it
    mSelectedItemIDs = {pItem->id};

    // Set the change
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));

    // Update the designer
    refresh();
    qInfo() << tr("The item is added to the report");
    emit edited();
}

//! Duplicate the currently selected item
void ReportDesigner::duplicateSelectedItems()
{
    // Check if there are any selected items
    if (mSelectedItemIDs.isEmpty())
        return;

    // Duplicate the selected items
    int numSelected = mSelectedItemIDs.size();
    for (int i = 0; i != numSelected; ++i)
    {
        ReportItem* pSrcItem = mScenePage.get(mSelectedItemIDs[i]);
        ReportItem* pCopyItem = pSrcItem->clone();
        pCopyItem->name.append(tr(" (Copy)"));
        mScenePage.add(pCopyItem);
    }

    // Set the change
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));

    // Update the designer
    refresh();
    qInfo() << tr("Items are duplicated and added to the report");
    emit edited();
}

//! Remove the currently selected items
void ReportDesigner::removeSelectedItems()
{
    // Check if there are any selected items
    if (mSelectedItemIDs.isEmpty())
        return;

    // Show the dialog
    auto answer = QMessageBox::question(this, tr("Remove selected items"), tr("Are you sure you want to remove selected items?"));
    if (answer != QMessageBox::Yes)
        return;

    // Remove the selected items
    int numSelected = mSelectedItemIDs.size();
    for (int i = 0; i != numSelected; ++i)
        mScenePage.remove(mScenePage.get(mSelectedItemIDs[i]));
    mSelectedItemIDs.clear();

    // Set the change
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));

    // Update the designer
    refresh();
    qInfo() << tr("Items are removed from the report");
    emit edited();
}

//! Move selected items using the specified shift
void ReportDesigner::moveSelectedItems(int iShift)
{
    // Move the selected items
    QList<QListWidgetItem*> items = mpItemList->selectedItems();
    int numItems = items.size();
    for (int i = 0; i != numItems; ++i)
    {
        int iRow = mpItemList->row(items[i]);
        mScenePage.swap(iRow, iRow + iShift);
    }

    // Set the change
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));

    // Update the designer
    refresh();
    qInfo() << tr("The report items are shifted");
    emit edited();
}

//! Process selecting item through list
void ReportDesigner::selectByList()
{
    // Store the selected items
    QList<QListWidgetItem*> items = mpItemList->selectedItems();
    int numItems = items.size();
    mSelectedItemIDs.resize(numItems);
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem* pItem = mScenePage.get(mpItemList->row(items[i]));
        mSelectedItemIDs[i] = pItem->id;
    }

    // Update the scene
    drawAll();

    // Update the property editor
    refreshEditor();
}

//! Process selecting item through scene
void ReportDesigner::selectByScene()
{
    // Store the selected items
    QList<QGraphicsItem*> items = mpScene->selectedItems();
    int numItems = items.size();
    mSelectedItemIDs.resize(numItems);
    for (int i = 0; i != numItems; ++i)
    {
        ReportSceneItem* pSceneItem = (ReportSceneItem*) items[i];
        mSelectedItemIDs[i] = pSceneItem->item()->id;
    }

    // Update the item list
    refreshList();

    // Update the property editor
    refreshEditor();
}

//! Rename the item by its list counterpart
void ReportDesigner::changeItemByList(QListWidgetItem* pListItem)
{
    // Get the item
    int index = mpItemList->row(pListItem);
    ReportItem* pReportItem = mScenePage.get(index);
    QString oldName = pReportItem->name;

    // Set a new name
    pReportItem->name = pListItem->text();

    // Set the change
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));

    // Finish up the editing
    qInfo() << tr("Item is successfully renamed: %1 -> %2").arg(oldName, pReportItem->name);
    emit edited();
}

//! Select the scale from the list of predefined options
void ReportDesigner::setScaleBySelector()
{
    int percentScale = mpScaleSelector->currentData().toInt();

    // Fit the page
    mpSceneView->fitToPage();
    if (percentScale <= 0)
        return;

    // Apply the scale factor to the fitted view
    double scale = percentScale / 100.0;
    mpSceneView->scale(scale, scale);
}

//! Change page orientation to portrait (landscape)
void ReportDesigner::changePageOrientation()
{
    // Change the orientation
    if (mScenePage.layout.orientation() == QPageLayout::Portrait)
        mScenePage.layout.setOrientation(QPageLayout::Landscape);
    else
        mScenePage.layout.setOrientation(QPageLayout::Portrait);

    // Set the change
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));

    // Redraw the scene
    drawAll();

    // Finish up the editing
    emit edited();
}

//! Set the editor for item data
void ReportDesigner::setDataEditor(ReportItem* pItem)
{
    QLayout* pDataLayout = mpDataEditorContainer->layout();

    // Process the existing editor
    if (mpDataEditor)
    {
        // Set the new data if the editor has the same type
        if (pItem && mpDataEditor->type() == pItem->type())
        {
            mpDataEditor->setItemGetter(createItemGetter(pItem->id));
            return;
        }

        // Remove the current editor
        pDataLayout->removeWidget(mpDataEditor);
        delete mpDataEditor;
        mpDataEditor = nullptr;
    }

    // Create the new editor, if necessary
    if (pItem)
    {
        switch (pItem->type())
        {
        case ReportItem::kGraph:
            mpDataEditor = new GraphReportDataEditor(mpGeometryView, mScenePage);
            break;
        case ReportItem::kMode:
            mpDataEditor = new ModeReportDataEditor(mpGeometryView, mScenePage);
            break;
        case ReportItem::kDiagram:
            mpDataEditor = new DiagramReportDataEditor(mpGeometryView, mScenePage);
            break;
        default:
            break;
        }
        if (mpDataEditor)
        {
            connect(mpDataEditor, &ReportDataEditor::edited, this, &ReportDesigner::processItemEdited);
            mpDataEditor->setItemGetter(createItemGetter(pItem->id));
            pDataLayout->addWidget(mpDataEditor);
        }
    }
}

//! Process changing items via scene
void ReportDesigner::processSceneItemChanged()
{
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));
    refreshEditor();
    emit edited();
}

//! Process changing item via property or data editors
void ReportDesigner::processItemEdited()
{
    mpSceneUndoStack->push(new EditPage(mPage, mScenePage));
    drawAll();
    emit edited();
}

//! Process editing items on the scene
void ReportDesigner::processEditItemRequest(ReportSceneItem* pSceneItem)
{
    // Edit only the text items
    ReportItem* pReportItem = pSceneItem->item();

    // Get the item geometry
    QRectF sceneRect = pSceneItem->sceneBoundingRect();
    QPoint viewTopLeft = mpSceneView->mapFromScene(sceneRect.topLeft());
    QPoint viewBottomRight = mpSceneView->mapFromScene(sceneRect.bottomRight());
    QRect viewRect(viewTopLeft, viewBottomRight);

    // Show the editor
    switch (pReportItem->type())
    {
    case ReportItem::kText:
        mpTextEditor->startEditing(viewRect, (TextReportItem*) pReportItem);
        break;
    case ReportItem::kGraph:
        mpGraphEditor->startEditing(viewRect, (GraphReportSceneItem*) pSceneItem);
        break;
    case ReportItem::kPicture:
    {
        QString pathFile = QFileDialog::getOpenFileName(this, tr("Open Picture"), Utility::getLastDirectory(mSettings).path(),
                                                        tr("Picture file format (*.svg *.png *.jpg *.jpeg)"));
        if (!pathFile.isEmpty())
            static_cast<PictureReportItem*>(pReportItem)->load(pathFile);
        break;
    }
    case ReportItem::kTable:
        mpTableEditor->startEditing(viewRect, (TableReportItem*) pReportItem);
        break;
    default:
        break;
    }
}

//! Create all the widgets
void ReportDesigner::createContent()
{
    // Constants
    int const kHandleWidth = 5;

    // Create the control widgets
    QSplitter* pControlSplitter = new QSplitter(Qt::Vertical);
    pControlSplitter->setHandleWidth(kHandleWidth);
    pControlSplitter->addWidget(createListWidget());
    pControlSplitter->addWidget(createEditorWidget());
    pControlSplitter->setStretchFactor(1, 1);

    // Combine the widgets
    QSplitter* pMainSplitter = new QSplitter(Qt::Horizontal);
    pMainSplitter->addWidget(createSceneWidget());
    pMainSplitter->addWidget(pControlSplitter);
    pMainSplitter->setHandleWidth(kHandleWidth);
    pMainSplitter->setStretchFactor(0, 2);
    pMainSplitter->setStretchFactor(1, 1);

    // Create the main layout
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addWidget(pMainSplitter);
    setLayout(pMainLayout);
}

//! Set the connections between the widgets
void ReportDesigner::createConnections()
{
    // List
    connect(mpItemList, &QListWidget::itemSelectionChanged, this, &ReportDesigner::selectByList);
    connect(mpItemList, &QListWidget::itemChanged, this, &ReportDesigner::changeItemByList);

    // Scene
    connect(mpScaleSelector, &QComboBox::currentIndexChanged, this, &ReportDesigner::setScaleBySelector);
    connect(mpScene, &QGraphicsScene::selectionChanged, this, &ReportDesigner::selectByScene);

    // Editor
    connect(mpPropertyEditor, &ReportPropertyEditor::edited, this, &ReportDesigner::processItemEdited);
    connect(mpTextEditor, &ReportTextEditor::editingFinished, this, &ReportDesigner::processItemEdited);
    connect(mpGraphEditor, &ReportGraphEditor::editingFinished, this, &ReportDesigner::processItemEdited);
    connect(mpTableEditor, &ReportTableEditor::editingFinished, this, &ReportDesigner::processItemEdited);
}

//! Create the group of scene widgets
QWidget* ReportDesigner::createSceneWidget()
{
    // Constants
    QList<int> const kScales = {10, 25, 50, 75, 100, 125, 150, 200, 400, 800, 1600};

    // Initialize the printing state
    mIsPrinting = false;

    // Create the scene and the view
    mpScene = new QGraphicsScene;
    mpSceneView = new ReportSceneView;

    // Create the undo stack
    mpSceneUndoStack = new QUndoStack(this);

    // Create the scene editors
    mpTextEditor = new ReportTextEditor(mpSceneView);
    mpGraphEditor = new ReportGraphEditor(mpSceneView);
    mpTableEditor = new ReportTableEditor(mpSceneView);

    // Initialize the view
    mpSceneView->setScene(mpScene);
    mpSceneView->setRenderHint(QPainter::Antialiasing);
    mpSceneView->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    mpSceneView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    mpSceneView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    // Create the scale combobox
    mpScaleSelector = new QComboBox;
    mpScaleSelector->setFont(font());
    mpScaleSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    for (int s : kScales)
        mpScaleSelector->addItem(QString("%1 %").arg(QString::number(s)), s);
    mpScaleSelector->addItem(tr("Fit Page"), -1);
    mpScaleSelector->setCurrentIndex(mpScaleSelector->count() - 1);

    // Create the helper function to create actions
    auto createBoolAction = [this](QIcon const& icon, QString const& name, bool& option)
    {
        QAction* pAction = new QAction(icon, name, this);
        pAction->setCheckable(true);
        pAction->setChecked(option);
        connect(pAction, &QAction::triggered, this,
                [this, &option](bool flag)
                {
                    option = flag;
                    refresh();
                    emit edited();
                });
        return pAction;
    };

    // Create the actions
    QAction* pLockSceneAction = createBoolAction(QIcon(":/icons/lock.svg"), tr("Lock items"), mOptions.lockItems);
    QAction* pEnablePrintingAction = createBoolAction(QIcon(":/icons/page-valid.svg"), tr("Validate page"), mOptions.isValid);

    // Create the toolbar
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addWidget(new QLabel(tr("Scale: ")));
    pToolBar->addWidget(mpScaleSelector);
    pToolBar->addAction(QIcon(":/icons/page-fit.svg"), tr("Fit page"), Qt::CTRL | Qt::Key_0, mpSceneView, &ReportSceneView::fitToPage);
    pToolBar->addAction(QIcon(":/icons/page-zoom-in.svg"), tr("Zoom in"), QKeySequence::ZoomIn, mpSceneView, &ReportSceneView::zoomIn);
    pToolBar->addAction(QIcon(":/icons/page-zoom-out.svg"), tr("Zoom out"), QKeySequence::ZoomOut, mpSceneView, &ReportSceneView::zoomOut);

    pToolBar->addSeparator();
    QAction* pUndoAction = mpSceneUndoStack->createUndoAction(this, tr("&Undo"));
    pUndoAction->setIcon(QIcon(":/icons/edit-undo.svg"));
    pUndoAction->setShortcuts(QKeySequence::Undo);
    connect(pUndoAction, &QAction::triggered, this, &ReportDesigner::refresh);
    pToolBar->addAction(pUndoAction);
    QAction* pRedoAction = mpSceneUndoStack->createRedoAction(this, tr("&Redo"));
    pRedoAction->setIcon(QIcon(":/icons/edit-redo.svg"));
    pRedoAction->setShortcuts(QKeySequence::Redo);
    pToolBar->addAction(pRedoAction);
    connect(pRedoAction, &QAction::triggered, this, &ReportDesigner::refresh);

    pToolBar->addSeparator();
    pToolBar->addAction(pLockSceneAction);
    pToolBar->addAction(pEnablePrintingAction);
    pToolBar->addAction(QIcon(":/icons/page-orientation.svg"), tr("Change page orientation"), this, &ReportDesigner::changePageOrientation);
    pToolBar->addAction(QIcon(":/icons/page-print.svg"), tr("Print page"), Qt::CTRL | Qt::SHIFT | Qt::Key_P, this, &ReportDesigner::requestPrint);
    pToolBar->setIconSize(Constants::Size::skToolBarIcon);
    Utility::setShortcutHints(pToolBar);

    // Construct the group box
    QGroupBox* pGroupBox = new QGroupBox(tr("Layout"));
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(pToolBar);
    pLayout->addWidget(mpSceneView);
    pGroupBox->setLayout(pLayout);
    return pGroupBox;
}

//! Create the group of item widgets
QWidget* ReportDesigner::createListWidget()
{
    // Create the widgets
    mpItemList = new QListWidget;
    mpItemList->setFont(font());
    mpItemList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mpItemList->setResizeMode(QListWidget::Adjust);
    mpItemList->setSizeAdjustPolicy(QListWidget::AdjustToContents);

    // Create the toolbar
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addAction(QIcon(":/icons/item-text.svg"), tr("Add text"), this, [this]() { addItem(ReportItem::kText); });
    pToolBar->addAction(QIcon(":/icons/item-graph.svg"), tr("Add graph"), this, [this]() { addItem(ReportItem::kGraph); });
    pToolBar->addAction(QIcon(":/icons/item-picture.svg"), tr("Add picture"), this, [this]() { addItem(ReportItem::kPicture); });
    pToolBar->addAction(QIcon(":/icons/item-table.svg"), tr("Add table"), this, [this]() { addItem(ReportItem::kTable); });
    pToolBar->addAction(QIcon(":/icons/item-mode.svg"), tr("Add mode"), this, [this]() { addItem(ReportItem::kMode); });
    pToolBar->addAction(QIcon(":/icons/item-diagram.svg"), tr("Add diagram"), this, [this]() { addItem(ReportItem::kDiagram); });
    pToolBar->addSeparator();
    pToolBar->addAction(QIcon(":/icons/edit-copy.svg"), tr("Duplicate"), this, &ReportDesigner::duplicateSelectedItems);
    QAction* pRemoveAction = pToolBar->addAction(QIcon(":/icons/edit-remove.svg"), tr("Remove"), this, &ReportDesigner::removeSelectedItems);
    pToolBar->addSeparator();
    pToolBar->addAction(QIcon(":/icons/arrow-up.svg"), tr("Move up"), this, [this]() { moveSelectedItems(-1); });
    pToolBar->addAction(QIcon(":/icons/arrow-down.svg"), tr("Move down"), this, [this]() { moveSelectedItems(+1); });
    pToolBar->setIconSize(Constants::Size::skToolBarIcon);
    Utility::setShortcutHints(pToolBar);

    // Set the shortcuts
    pRemoveAction->setShortcut(Qt::Key_Delete);

    // Construct the group box
    QGroupBox* pGroupBox = new QGroupBox(tr("Items"));
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(pToolBar);
    pLayout->addWidget(mpItemList);
    pGroupBox->setLayout(pLayout);
    return pGroupBox;
}

//! Create the tab widget consisted of property and data editors
QWidget* ReportDesigner::createEditorWidget()
{
    // Create the widgets
    mpPropertyEditor = new ReportPropertyEditor;
    mpDataEditor = nullptr;
    mpDataEditorContainer = new QWidget;
    mpDataEditorContainer->setLayout(new QVBoxLayout);

    // Combine the widgets
    CustomTabWidget* pTabWidget = new CustomTabWidget;
    pTabWidget->setTabsRenamable(false);
    pTabWidget->setTabsClosable(false);
    pTabWidget->setTabPosition(CustomTabWidget::North);
    pTabWidget->addTab(mpPropertyEditor, tr("Properties"));
    pTabWidget->addTab(mpDataEditorContainer, tr("Data"));
    pTabWidget->setCurrentIndex(1);

    return pTabWidget;
}

//! Construct a functor to get the item of the specified identifier
ReportItemGetter ReportDesigner::createItemGetter(QUuid const& id)
{
    return [this, id]() { return mScenePage.get(id); };
}

//! Construct a functor to get the render window with the specified identifier
vtkRenderWindow* ReportDesigner::createRenderWindow(QUuid const& id)
{
    if (mRenderWindows.contains(id))
        return mRenderWindows[id];
    vtkRenderWindow* renderWindow = vtkRenderWindow::New();
    renderWindow->OffScreenRenderingOn();
    mRenderWindows[id] = renderWindow;
    return renderWindow;
}

//! Create the report text parser and initialize it
void ReportDesigner::updateTextEngine()
{
    // Set common values
    if (mpResponseEditor->iSelectedBundle() >= 0)
    {
        ResponseBundle const& bundle = mpResponseEditor->collection().get(mpResponseEditor->iSelectedBundle());
        mTextEngine.setVariable("freq", bundle.freq);
        mTextEngine.setVariable("force", bundle.force);
    }

    // Set common translations
    mTextEngine.setReplacement(Units::skM_S2, tr("m/s%1").arg(Constants::Symbol::skPow2));
    mTextEngine.setReplacement(Units::skM_S2_N, tr("(m/s%1)/N").arg(Constants::Symbol::skPow2));
    mTextEngine.setReplacement(Units::skM, tr("m"));
    mTextEngine.setReplacement(Units::skMM_S2, tr("mm/s%1").arg(Constants::Symbol::skPow2));
    mTextEngine.setReplacement(Units::skMM_S2_N, tr("(mm/s%1)/N").arg(Constants::Symbol::skPow2));
    mTextEngine.setReplacement(Units::skMM, tr("mm"));
    mTextEngine.setReplacement(Units::skN, tr("N"));
}

//! Resolve item dependencies
void ReportDesigner::resolveItemLinks()
{
    int numItems = mScenePage.count();
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem* pSlaveItem = mScenePage.get(i);
        if (!pSlaveItem->link.isNull())
        {
            ReportItem* pMasterItem = mScenePage.get(pSlaveItem->link);
            if (pMasterItem && pMasterItem->type() == pSlaveItem->type())
            {
                if (pMasterItem->type() == ReportItem::kGraph)
                {
                    GraphReportItem* pGraphSlaveItem = (GraphReportItem*) pSlaveItem;
                    GraphReportItem* pGraphMasterItem = (GraphReportItem*) pMasterItem;
                    pGraphSlaveItem->font = pGraphMasterItem->font;
                    pGraphSlaveItem->responseDir = pGraphMasterItem->responseDir;
                    pGraphSlaveItem->unit = pGraphMasterItem->unit;
                    pGraphSlaveItem->curves = pGraphMasterItem->curves;
                    pGraphSlaveItem->xRange = pGraphMasterItem->xRange;
                }
                else if (pMasterItem->type() == ReportItem::kMode)
                {
                    ModeReportItem* pModeSlaveItem = (ModeReportItem*) pSlaveItem;
                    ModeReportItem* pModeMasterItem = (ModeReportItem*) pMasterItem;
                    pModeSlaveItem->font = pModeMasterItem->font;
                    pModeSlaveItem->unit = pModeMasterItem->unit;
                    pModeSlaveItem->colorMap = pModeMasterItem->colorMap;
                    pModeSlaveItem->scale = pModeMasterItem->scale;
                    pModeSlaveItem->amplitude = pModeMasterItem->amplitude;
                    pModeSlaveItem->maskComponents = pModeMasterItem->maskComponents;
                    pModeSlaveItem->showUndeformed = pModeMasterItem->showUndeformed;
                }
                else if (pMasterItem->type() == ReportItem::kDiagram)
                {
                    DiagramReportItem* pModeSlaveItem = (DiagramReportItem*) pSlaveItem;
                    DiagramReportItem* pModeMasterItem = (DiagramReportItem*) pMasterItem;
                    pModeSlaveItem->font = pModeMasterItem->font;
                    pModeSlaveItem->unit = pModeMasterItem->unit;
                    pModeSlaveItem->colorMap = pModeMasterItem->colorMap;
                    pModeSlaveItem->scale = pModeMasterItem->scale;
                    pModeSlaveItem->amplitude = pModeMasterItem->amplitude;
                }
            }
        }
    }
}

ReportSceneView::ReportSceneView(QWidget* pParent)
    : QGraphicsView(pParent)
    , mZoomFactor(1.15)
{
}

//! Fit page to width/height
void ReportSceneView::fitToPage()
{
    if (!scene())
        return;
    resetTransform();
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
}

//! Increase the view scale
void ReportSceneView::zoomIn()
{
    scale(mZoomFactor, mZoomFactor);
}

//! Lower the view scale
void ReportSceneView::zoomOut()
{
    scale(1.0 / mZoomFactor, 1.0 / mZoomFactor);
}

//! Enable scrolling and zooming
void ReportSceneView::wheelEvent(QWheelEvent* pEvent)
{
    if (pEvent->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
}

ReportTextEditor::ReportTextEditor(QWidget* pParent)
    : QTextEdit(pParent)
    , mpItem(nullptr)
{
    hide();
}

//! Start editing of a text report item
void ReportTextEditor::startEditing(QRect const& rect, TextReportItem* pItem)
{
    // Store the item
    mpItem = pItem;

    // Set the geometry
    setGeometry(rect);

    // Set the text
    setFont(mpItem->font);
    setText(mpItem->text);

    // Set the alignment
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);
    QTextBlockFormat format;
    format.setAlignment(mpItem->align);
    cursor.mergeBlockFormat(format);

    // Show the editor
    show();
    setFocus();
}

//! Finish editing once the focus is lost
void ReportTextEditor::focusOutEvent(QFocusEvent* pEvent)
{
    QTextEdit::focusOutEvent(pEvent);
    mpItem->text = toPlainText();
    this->hide();
    mpItem = nullptr;
    emit editingFinished();
}

//! Process the keys to lose focus
void ReportTextEditor::keyPressEvent(QKeyEvent* pEvent)
{
    if (pEvent->key() == Qt::Key_Escape)
    {
        clearFocus();
        return;
    }
    QTextEdit::keyPressEvent(pEvent);
}

ReportGraphEditor::ReportGraphEditor(QWidget* pParent)
    : CustomPlot(pParent)
    , mpItem(nullptr)
{
    hide();
}

//! Start editing of a text report item
void ReportGraphEditor::startEditing(QRect const& rect, GraphReportSceneItem* pItem)
{
    // Store the item
    mpItem = pItem;
    GraphReportItem* pGraphItem = (GraphReportItem*) mpItem->item();

    // Set the geometry
    setGeometry(rect);

    // Clear the data
    clear();

    // Slice the plot data
    CustomPlot* pSrcPlot = mpItem->mpPlot;
    QCPAxis* pSrcXAxis = pSrcPlot->xAxis;
    QCPAxis* pSrcYAxis = pSrcPlot->yAxis;
    QCPLegend* pSrcLegend = pSrcPlot->legend;

    // Copy the visual settings
    setFont(pSrcPlot->font());

    // Get the destination axes
    QCPAxis* pXAxis = xAxis;
    QCPAxis* pYAxis = yAxis;
    if (pGraphItem->swapAxes)
        std::swap(pXAxis, pYAxis);

    // Copy the axes labels
    xAxis->setTickLabelFont(pSrcXAxis->tickLabelFont());
    yAxis->setTickLabelFont(pSrcYAxis->tickLabelFont());
    xAxis->setLabelFont(pSrcXAxis->labelFont());
    yAxis->setLabelFont(pSrcYAxis->labelFont());
    pXAxis->setLabel(pGraphItem->xLabel);
    pYAxis->setLabel(pGraphItem->yLabel);

    // Copy the grid
    xAxis->grid()->setPen(pSrcXAxis->grid()->pen());
    yAxis->grid()->setPen(pSrcYAxis->grid()->pen());
    xAxis->grid()->setZeroLinePen(pSrcXAxis->grid()->zeroLinePen());
    yAxis->grid()->setZeroLinePen(pSrcYAxis->grid()->zeroLinePen());

    // Copy the axis range
    rescaleAxes();
    xAxis->setRange(pSrcXAxis->range());
    yAxis->setRange(pSrcYAxis->range());
    xAxis->setTicker(pSrcXAxis->ticker());
    yAxis->setTicker(pSrcYAxis->ticker());

    // Copy the legend
    legend->setVisible(pSrcLegend->visible());
    legend->setFont(pSrcLegend->font());
    setLegendAlignment(pSrcPlot->legendAlignment());

    // Copy the plottables
    int numPlottables = pSrcPlot->plottableCount();
    for (int i = 0; i != numPlottables; ++i)
    {
        QCPCurve* pSrcCurve = qobject_cast<QCPCurve*>(pSrcPlot->plottable(i));
        if (!pSrcCurve)
            continue;
        QCPCurve* pDstCurve = new QCPCurve(pXAxis, pYAxis);
        pDstCurve->data()->set(*pSrcCurve->data());
        pDstCurve->setPen(pSrcCurve->pen());
        pDstCurve->setBrush(pSrcCurve->brush());
        pDstCurve->setName(pSrcCurve->name());
        pDstCurve->setLineStyle(pSrcCurve->lineStyle());
        pDstCurve->setScatterStyle(pSrcCurve->scatterStyle());
    }

    // Show the editor
    replot();
    show();
    setFocus();
}

//! Finish editing once the focus is lost
void ReportGraphEditor::focusOutEvent(QFocusEvent* pEvent)
{
    // Do not lose focus on editors
    QWidget* pFocusWidget = QApplication::focusWidget();
    if (pFocusWidget && pFocusWidget->parent() && pFocusWidget->parent()->parent() == this)
    {
        CustomPlot::focusOutEvent(pEvent);
        return;
    }

    // Get the destination axes
    GraphReportItem* pGraphItem = (GraphReportItem*) mpItem->item();
    QCPAxis* pXAxis = xAxis;
    QCPAxis* pYAxis = yAxis;
    if (pGraphItem->swapAxes)
        std::swap(pXAxis, pYAxis);

    // Copy the labels
    pGraphItem->xLabel = pXAxis->label();
    pGraphItem->yLabel = pYAxis->label();

    // Finish the editing
    this->hide();
    mpItem = nullptr;
    emit editingFinished();
    QWidget::focusOutEvent(pEvent);
}

//! Process the keys to lose focus
void ReportGraphEditor::keyPressEvent(QKeyEvent* pEvent)
{
    if (pEvent->key() == Qt::Key_Return || pEvent->key() == Qt::Key_Escape)
    {
        clearFocus();
        return;
    }
    QWidget::keyPressEvent(pEvent);
}

ReportTableEditor::ReportTableEditor(QWidget* pParent)
    : CustomTable(pParent)
    , mpItem(nullptr)
{
    hide();
}

//! Start editing of a text report item
void ReportTableEditor::startEditing(QRect const& rect, TableReportItem* pItem)
{
    // Store the item
    mpItem = pItem;

    // Set the geometr
    setGeometry(rect);

    // Set the properties
    setFont(mpItem->font);
    horizontalHeader()->setVisible(false);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    setWordWrap(true);

    // Set the data
    int numRows = mpItem->numRows();
    int numCols = mpItem->numCols();
    int iHeader = mpItem->showLabels;
    setRowCount(numRows + iHeader);
    setColumnCount(numCols + iHeader);
    for (int iRow = 0; iRow != numRows; ++iRow)
    {
        for (int iCol = 0; iCol != numCols; ++iCol)
            setItem(iHeader + iRow, iHeader + iCol, Utility::createTableItem(mpItem->data[iRow][iCol]));
    }

    // Set the header
    if (iHeader > 0)
    {
        // Middle
        setItem(0, 0, Utility::createTableItem(mpItem->midLabel));

        // Horizontal
        for (int iCol = 0; iCol != numCols; ++iCol)
            setItem(0, iHeader + iCol, Utility::createTableItem(mpItem->horLabels[iCol]));

        // Vertical
        for (int iRow = 0; iRow != numRows; ++iRow)
            setItem(iHeader + iRow, 0, Utility::createTableItem(mpItem->verLabels[iRow]));
    }

    // Show the editor
    show();
    setFocus();
}

//! Finish editing once the focus is lost
void ReportTableEditor::focusOutEvent(QFocusEvent* pEvent)
{
    // Do not lose focus on editors
    QWidget* pFocusWidget = QApplication::focusWidget();
    if (pFocusWidget && pFocusWidget->parent() && pFocusWidget->parent()->parent() == this)
    {
        QTableWidget::focusOutEvent(pEvent);
        return;
    }

    // Set the data
    int numRows = mpItem->numRows();
    int numCols = mpItem->numCols();
    int iHeader = mpItem->showLabels;
    for (int iRow = 0; iRow != numRows; ++iRow)
    {
        for (int iCol = 0; iCol != numCols; ++iCol)
            mpItem->data[iRow][iCol] = item(iHeader + iRow, iHeader + iCol)->text();
    }

    // Set the header
    if (iHeader > 0)
    {
        // Middle
        mpItem->midLabel = item(0, 0)->text();

        // Horizontal
        for (int iCol = 0; iCol != numCols; ++iCol)
            mpItem->horLabels[iCol] = item(0, iHeader + iCol)->text();

        // Vertical
        for (int iRow = 0; iRow != numRows; ++iRow)
            mpItem->verLabels[iRow] = item(iHeader + iRow, 0)->text();
    }

    // Finish the editing
    this->hide();
    mpItem = nullptr;
    emit editingFinished();
    CustomTable::focusOutEvent(pEvent);
}

//! Process the keys to lose focus
void ReportTableEditor::keyPressEvent(QKeyEvent* pEvent)
{
    if (pEvent->key() == Qt::Key_Return || pEvent->key() == Qt::Key_Escape)
    {
        clearFocus();
        return;
    }
    CustomTable::keyPressEvent(pEvent);
}

EditPage::EditPage(ReportPage& page, ReportPage const& value)
    : mPage(page)
    , mOldValue(page)
    , mNewValue(value)
{
}

//! Revert the change
void EditPage::undo()
{
    mPage = mOldValue;
}

//! Make the change
void EditPage::redo()
{
    mPage = mNewValue;
}

//! Helper function to create a list item
QListWidgetItem* createListItem(ReportPage const& page, int index)
{
    ReportItem const* pItem = page.get(index);

    // Get the visual attributes of the item
    QString prefix;
    QIcon icon;
    switch (pItem->type())
    {
    case ReportItem::kText:
        icon = QIcon(":/icons/item-text.svg");
        prefix = QObject::tr("Text");
        break;
    case ReportItem::kGraph:
        icon = QIcon(":/icons/item-graph.svg");
        prefix = QObject::tr("Graph");
        break;
    case ReportItem::kPicture:
        icon = QIcon(":/icons/item-picture.svg");
        prefix = QObject::tr("Picture");
        break;
    case ReportItem::kTable:
        icon = QIcon(":/icons/item-table.svg");
        prefix = QObject::tr("Table");
        break;
    case ReportItem::kMode:
        icon = QIcon(":/icons/item-mode.svg");
        prefix = QObject::tr("Mode");
        break;
    case ReportItem::kDiagram:
        icon = QIcon(":/icons/item-diagram.svg");
        prefix = QObject::tr("Diagram");
        break;
    default:
        break;
    }

    // Count the number of items of the same type
    int numItems = page.count();
    int numType = 0;
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem const* pCurrent = page.get(i);
        if (pCurrent == pItem)
            break;
        if (pCurrent->type() == pItem->type())
            ++numType;
    }

    // Define the name, if empty
    QString name = pItem->name;
    if (pItem->name.isEmpty())
        name = QObject::tr("%1 %2").arg(prefix).arg(1 + numType);

    // Create the list item
    QListWidgetItem* pResult = new QListWidgetItem(icon, name);
    pResult->setFlags(pResult->flags() | Qt::ItemIsEditable);
    return pResult;
}

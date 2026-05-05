#include <QColorDialog>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCamera.h>
#include <vtkCameraOrientationWidget.h>
#include <vtkCellArray.h>
#include <vtkCellPicker.h>
#include <vtkCubeSource.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkGlyph3DMapper.h>
#include <vtkInformation.h>
#include <vtkInformationStringKey.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolygon.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

#include "customtable.h"
#include "geometryview.h"
#include "mathutility.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Frontend;
using namespace Eigen;
using namespace Constants;

// Macros
vtkStandardNewMacro(GeometryInteractorStyle);

// Constants
vtkNew<vtkNamedColors> const vtkColors;
vtkInformationStringKey* vtkNameKey = vtkInformationStringKey::MakeKey("NameKey", "Name");

// Helper functions
void setActorScale(vtkActor* actor, double factor);

GeometryViewOptions::GeometryViewOptions()
{
    // Color scheme
    sceneColor = vtkColors->GetColor3d("aliceblue");
    sceneColor2 = vtkColors->GetColor3d("white");
    edgeColor = vtkColors->GetColor3d("gainsboro");
    componentColors = {vtkColors->GetColor3d("green"),  vtkColors->GetColor3d("orange"),    vtkColors->GetColor3d("lightseagreen"),
                       vtkColors->GetColor3d("purple"), vtkColors->GetColor3d("chocolate"), vtkColors->GetColor3d("slategray")};

    // Opacity
    edgeOpacity = 0.5;

    // Flags
    showEdges = false;
    showWireframe = false;
    showLabels = false;
    showVertices = true;
    showLines = true;
    showTrias = true;
    showQuads = true;

    // Dimensions
    sceneScale = {1.0, 1.0, 1.0};
    lineWidth = 2;
    pointScale = 0.0075;
    fontSize = 12;
    pickTolerance = 0.005;
    pickFactor = 2.0;
}

GeometryView::GeometryView(GeometryViewOptions const& options)
    : mOptions(options)
{
    createContent();
    initialize();
    createConnections();
}

GeometryView::~GeometryView()
{
    clear();
}

void GeometryView::clear()
{
    // Clean up the style
    mStyle->clear();

    // Remove the actors
    auto actors = mRenderer->GetActors();
    while (actors->GetLastActor())
        mRenderer->RemoveActor(actors->GetLastActor());

    // Remove the view properties
    auto props = mRenderer->GetViewProps();
    while (props->GetLastProp())
        mRenderer->RemoveViewProp(props->GetLastProp());
}

void GeometryView::plot()
{
    clear();
    drawGeometry();
    mRenderWindow->Render();
}

void GeometryView::refresh()
{
    mRenderWindow->Render();
}

int GeometryView::numSelected()
{
    return selections().size();
}

//! Retrieve the current selection
QList<GeometrySelection> GeometryView::selections() const
{
    return mStyle->selections();
}

//! Retrieve the current selection as the pairs of component and node names
QList<QPair<QString, QString>> GeometryView::selectionPairs() const
{
    QList<GeometrySelection> selections = mStyle->selections();
    int numSelections = selections.size();
    QList<QPair<QString, QString>> result(numSelections);
    for (int i = 0; i != numSelections; ++i)
    {
        GeometrySelection const& selection = selections[i];
        int iComponent = selection.iComponent;
        int iNode = selection.iNode;
        Testlab::Component const& component = mGeometry.components[iComponent];
        QString componentName = QString::fromStdWString(component.name);
        QString nodeName = QString::fromStdWString(component.nodes[iNode].name);
        result[i] = {componentName, nodeName};
    }
    return result;
}

Testlab::Geometry const& GeometryView::getGeometry() const
{
    return mGeometry;
}

GeometryDependencyEditor* GeometryView::dependencyEditor()
{
    return mpDependencyEditor;
}

void GeometryView::clearSelection()
{
    mStyle->deselectAll();
}

//! Add the node to the current selection set
bool GeometryView::addSelection(QString const& componentName, QString const& nodeName)
{
    int numComponents = mGeometry.components.size();
    for (int iComponent = 0; iComponent != numComponents; ++iComponent)
    {
        Testlab::Component const& component = mGeometry.components[iComponent];
        int numNodes = component.nodes.size();
        QString cName = QString::fromStdWString(component.name);
        for (int iNode = 0; iNode != numNodes; ++iNode)
        {
            Testlab::Node const& node = component.nodes[iNode];
            QString nName = QString::fromStdWString(node.name);
            if (cName == componentName && nName == nodeName)
            {
                GeometrySelection selection(iComponent, iNode);
                mStyle->select({selection});
                return true;
            }
        }
    }
    return false;
}

//! Replace the geometry
void GeometryView::setGeometry(Testlab::Geometry geometry)
{
    mGeometry = geometry;
    mOptions.maskComponents.clear();
    plot();
    Utility::setIsometricView(mRenderer);
    refresh();
}

//! Set the initial state of widgets
void GeometryView::initialize()
{
    int const kNumOrientationFrames = 10;

    // Specify the format for the VTK library
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    vtkObject::GlobalWarningDisplayOff();

    // Set up the scence
    mRenderer = vtkRenderer::New();
    mRenderer->SetBackground(mOptions.sceneColor.GetData());
    mRenderer->SetBackground2(mOptions.sceneColor2.GetData());
    mRenderer->GradientBackgroundOn();
    mRenderer->ResetCamera();

    // Create the window
    mRenderWindow = vtkGenericOpenGLRenderWindow::New();
    mRenderWindow->AddRenderer(mRenderer);
    mRenderWidget->setRenderWindow(mRenderWindow);

    // Create the orientation widget
    mOrientationWidget = vtkCameraOrientationWidget::New();
    mOrientationWidget->SetParentRenderer(mRenderer);
    mOrientationWidget->On();
    mOrientationWidget->SetAnimatorTotalFrames(kNumOrientationFrames);

    // Set the custom style to use for interaction
    auto interactor = mRenderWindow->GetInteractor();
    mStyle = GeometryInteractorStyle::New();
    mStyle->SetDefaultRenderer(mRenderer);
    mStyle->pickTolerance = mOptions.pickTolerance;
    mStyle->pickFactor = mOptions.pickFactor;
    interactor->SetInteractorStyle(mStyle);

    // Set the maximum dimension
    mMaximumDimension = 0.0;
}

//! Create all the widgets and corresponding actions
void GeometryView::createContent()
{
    QVBoxLayout* pLayout = new QVBoxLayout;

    // Create the editor
    mpViewEditor = new GeometryViewEditor(mGeometry, mOptions, this);
    mpDependencyEditor = new GeometryDependencyEditor(mGeometry, this);

    // Create the VTK widget
    mRenderWidget = new QVTKOpenGLNativeWidget(this);

    // Create auxiliary function
    auto createViewAction = [this](QIcon const& icon, QString const& name, int dir = -1, int sign = 1)
    {
        QAction* pAction = new QAction(icon, name, this);
        connect(pAction, &QAction::triggered, this,
                [this, dir, sign]()
                {
                    if (dir < 0)
                        Utility::setIsometricView(mRenderer);
                    else
                        Utility::setPlaneView(mRenderer, dir, sign);
                    refresh();
                });
        return pAction;
    };
    auto createShowAction = [this](QIcon const& icon, QString const& name, bool& option)
    {
        QAction* pAction = new QAction(icon, name, this);
        pAction->setCheckable(true);
        pAction->setChecked(option);
        connect(pAction, &QAction::triggered, this,
                [this, &option](bool flag)
                {
                    option = flag;
                    plot();
                });
        return pAction;
    };

    // Create the editor actions
    QAction* pViewEditorAction = new QAction(QIcon(":/icons/edit-view.png"), tr("Edit view options"), this);
    QAction* pDependencyEditorAction = new QAction(QIcon(":/icons/edit-dependency.svg"), tr("Edit dependencies"), this);

    // Create the view actions
    QAction* pIsometricViewAction = createViewAction(QIcon(":/icons/draw-isometric.svg"), tr("Show isometric view"));
    QAction* pFrontViewAction = createViewAction(QIcon(":/icons/draw-front.svg"), tr("Show front view"), 0, 1);
    QAction* pRearViewAction = createViewAction(QIcon(":/icons/draw-rear.svg"), tr("Show rear view"), 0, -1);
    QAction* pTopViewAction = createViewAction(QIcon(":/icons/draw-top.svg"), tr("Show top view"), 1, 1);
    QAction* pBottomViewAction = createViewAction(QIcon(":/icons/draw-bottom.svg"), tr("Show bottom view"), 1, -1);
    QAction* pLeftViewAction = createViewAction(QIcon(":/icons/draw-left.svg"), tr("Show left view"), 2, 1);
    QAction* pRightViewAction = createViewAction(QIcon(":/icons/draw-right.svg"), tr("Show right view"), 2, -1);

    // Create the show actions
    QAction* pLabelsAction = createShowAction(QIcon(":/icons/draw-label.svg"), tr("Show labels"), mOptions.showLabels);
    QAction* pVerticesAction = createShowAction(QIcon(":/icons/draw-vertex.svg"), tr("Show vertices"), mOptions.showVertices);
    QAction* pLinesAction = createShowAction(QIcon(":/icons/draw-line.svg"), tr("Show lines"), mOptions.showLines);
    QAction* pTriasAction = createShowAction(QIcon(":/icons/draw-tri.svg"), tr("Show triangles"), mOptions.showTrias);
    QAction* pQuadsAction = createShowAction(QIcon(":/icons/draw-quad.png"), tr("Show quadrangles"), mOptions.showQuads);
    QAction* pWireframeAction = createShowAction(QIcon(":/icons/draw-wireframe.svg"), tr("Show wireframe"), mOptions.showWireframe);

    // Create the connections
    connect(pViewEditorAction, &QAction::triggered, this,
            [this]()
            {
                mpViewEditor->refresh();
                showWidgetAtCenter(mpViewEditor);
            });
    connect(pDependencyEditorAction, &QAction::triggered, this,
            [this]()
            {
                mpDependencyEditor->refresh();
                showWidgetAtCenter(mpDependencyEditor);
            });

    // Create the toolbar
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addAction(pViewEditorAction);
    pToolBar->addAction(pDependencyEditorAction);
    pToolBar->addSeparator();
    pToolBar->addAction(pIsometricViewAction);
    pToolBar->addAction(pFrontViewAction);
    pToolBar->addAction(pRearViewAction);
    pToolBar->addAction(pTopViewAction);
    pToolBar->addAction(pBottomViewAction);
    pToolBar->addAction(pLeftViewAction);
    pToolBar->addAction(pRightViewAction);
    pToolBar->addSeparator();
    pToolBar->addAction(pLabelsAction);
    pToolBar->addAction(pVerticesAction);
    pToolBar->addAction(pLinesAction);
    pToolBar->addAction(pTriasAction);
    pToolBar->addAction(pQuadsAction);
    pToolBar->addAction(pWireframeAction);
    Utility::setShortcutHints(pToolBar);

    // Combine the widgets
    pLayout->addWidget(pToolBar);
    pLayout->addWidget(mRenderWidget);
    setLayout(pLayout);
}

//! Specify the connections between widgets
void GeometryView::createConnections()
{
    connect(mpViewEditor, &GeometryViewEditor::edited, this, &GeometryView::plot);
}

//! Represent geometry
void GeometryView::drawGeometry()
{
    // Estimate the maximum dimension
    mMaximumDimension = Backend::Utility::getMaximumDimension(mGeometry);
    if (mMaximumDimension < std::numeric_limits<double>::epsilon())
        mMaximumDimension = 1.0;

    // Update the mask of components
    int numComponents = mGeometry.components.size();
    if (mOptions.maskComponents.size() < numComponents)
        mOptions.maskComponents.resize(numComponents, true);

    // Draw all the components
    for (int iComponent = 0; iComponent != numComponents; ++iComponent)
    {
        if (mOptions.maskComponents[iComponent])
            drawComponent(iComponent);
    }
}

//! Represent the geometrical component
void GeometryView::drawComponent(int iComponent)
{
    Testlab::Component const& component = mGeometry.components[iComponent];

    // Construct the vertices out of nodes
    vtkSmartPointer<vtkPoints> points = createPoints(component.nodes);

    // Get the color
    int numColors = mOptions.componentColors.size();
    int iColor = Utility::getRepeatedIndex(iComponent, numColors);
    vtkColor3d color = mOptions.componentColors[iColor];

    // Draw the elements
    if (mOptions.showQuads)
        drawElements(points, component.quads, color);
    if (mOptions.showTrias)
        drawElements(points, component.trias, color);
    if (mOptions.showLines)
        drawElements(points, component.lines, color);

    // Draw the vertices
    if (mOptions.showVertices)
        drawVertices(points, iComponent, color);
}

//! Render vertices using one color
void GeometryView::drawVertices(vtkSmartPointer<vtkPoints> points, int iComponent, vtkColor3d color, double opacity)
{
    Testlab::Component const& component = mGeometry.components[iComponent];
    QString componentName = QString::fromStdWString(component.name);
    int numPoints = points->GetNumberOfPoints();
    double length = mOptions.pointScale * mMaximumDimension;
    for (int i = 0; i != numPoints; ++i)
    {
        // Create the point actor
        double data[3];
        points->GetPoint(i, data);
        Vector3d position = {data[0], data[1], data[2]};
        vtkSmartPointer<vtkActor> actor = Utility::createCubeActor(position, length);

        // Set the actor properties
        actor->GetProperty()->SetColor(color.GetData());
        actor->GetProperty()->SetEdgeColor(color.GetData());
        actor->GetProperty()->EdgeVisibilityOn();

        // Set the offset
        actor->GetMapper()->SetResolveCoincidentTopologyToPolygonOffset();
        actor->GetMapper()->SetResolveCoincidentTopologyPolygonOffsetParameters(1.0, -1.0);

        // Store the name
        vtkInformation* info = actor->GetProperty()->GetInformation();
        QString nodeName = QString::fromStdWString(component.nodes[i].name);
        QString fullName = QString("%1:%2").arg(componentName, nodeName);
        vtkNameKey->Set(info, fullName.toStdString());

        // Register the actor
        mStyle->registerActor(GeometrySelection(iComponent, i), actor);

        // Add the actor to the scene
        mRenderer->AddActor(actor);

        // Add the label, if necessary
        if (mOptions.showLabels)
            drawVertexLabel(actor);
    }
}

//! Render the vertex label
void GeometryView::drawVertexLabel(vtkSmartPointer<vtkActor> actor)
{
    // Get the position
    auto position = actor->GetCenter();
    double shift = mOptions.pointScale * mMaximumDimension;

    // Get the name
    vtkInformation* info = actor->GetProperty()->GetInformation();
    const char* text = vtkNameKey->Get(info);

    // Create the text actor
    vtkNew<vtkBillboardTextActor3D> textActor;
    textActor->SetInput(text);
    textActor->SetPosition(position[0], position[1] + shift, position[2]);

    // Set the text properties
    textActor->GetTextProperty()->SetFontSize(mOptions.fontSize);
    textActor->GetTextProperty()->SetColor(vtkColors->GetColor3d("Black").GetData());
    textActor->GetTextProperty()->SetJustificationToCentered();

    // Add the actor to the scene
    mRenderer->AddActor(textActor);
}

//! Render elements using one color
void GeometryView::drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color, double opacity)
{
    // Check if there are any elements to render
    if (indices.empty())
        return;

    // Create polygons
    vtkSmartPointer<vtkCellArray> polygons = createPolygons(indices);

    // Group polygons
    bool isPolys = indices.front().size() != 2;
    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    if (isPolys)
        polyData->SetPolys(polygons);
    else
        polyData->SetLines(polygons);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);

    // Set the offset
    mapper->SetResolveCoincidentTopologyToPolygonOffset();
    if (isPolys)
        mapper->SetResolveCoincidentTopologyPolygonOffsetParameters(1.0, 1.0);
    else
        mapper->SetResolveCoincidentTopologyLineOffsetParameters(1.0, -1.0);

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color.GetData());
    actor->GetProperty()->SetOpacity(opacity);
    actor->GetProperty()->SetLineWidth(mOptions.lineWidth);
    if (mOptions.showEdges)
    {
        actor->GetProperty()->SetEdgeColor(mOptions.edgeColor.GetData());
        actor->GetProperty()->SetEdgeOpacity(mOptions.edgeOpacity);
        actor->GetProperty()->EdgeVisibilityOn();
    }
    if (mOptions.showWireframe)
        actor->GetProperty()->SetRepresentationToWireframe();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Create points which are associated with the geometry
vtkSmartPointer<vtkPoints> GeometryView::createPoints(std::vector<Testlab::Node> const& nodes)
{
    vtkNew<vtkPoints> points;
    int numNodes = nodes.size();
    for (int i = 0; i != numNodes; ++i)
    {
        Vector3d position = Backend::Utility::convert3d(nodes[i].coordinates);
        position = position.cwiseProduct(mOptions.sceneScale);
        points->InsertPoint(i, position[0], position[1], position[2]);
    }
    return points;
}

//! Create polygons using given indices
vtkSmartPointer<vtkCellArray> GeometryView::createPolygons(std::vector<std::vector<int>> const& indices)
{
    vtkNew<vtkCellArray> polygons;
    int numElements = indices.size();
    for (int i = 0; i != numElements; ++i)
    {
        vtkNew<vtkPolygon> polygon;
        std::vector<int> const& elementIndices = indices[i];
        int numElementIndices = elementIndices.size();
        for (int j = 0; j != numElementIndices; ++j)
        {
            int iVertex = elementIndices[j];
            polygon->GetPointIds()->InsertNextId(iVertex);
        }
        polygons->InsertNextCell(polygon);
    }
    return polygons;
}

//! Show the widget at the center
void GeometryView::showWidgetAtCenter(QWidget* pWidget)
{
    // Show the widget
    pWidget->show();
    pWidget->raise();
    pWidget->activateWindow();

    // Position the widget on the screen
    QPoint center = mapToGlobal(rect().center());
    pWidget->move(center.x() - pWidget->width() / 2, center.y() - pWidget->height() / 2);
}

GeometryViewEditor::GeometryViewEditor(Testlab::Geometry const& geometry, GeometryViewOptions& options, QWidget* pParent)
    : QDialog(pParent)
    , mGeometry(geometry)
    , mOptions(options)
{
    setFont(Utility::getFont());
    setWindowTitle(tr("View Editor"));
    createContent();
    createConnections();
}

//! Update the widgets content
void GeometryViewEditor::refresh()
{
    // Update the list of components
    QSignalBlocker blockerComponentList(mpComponentList);
    mpComponentList->clear();
    int numComponents = mGeometry.components.size();
    int numMask = mOptions.maskComponents.size();
    int numColors = mOptions.componentColors.size();
    for (int i = 0; i != numComponents; ++i)
    {
        QString name = QString::fromStdWString(mGeometry.components[i].name);
        QListWidgetItem* pItem = new QListWidgetItem(name);
        int iColor = Utility::getRepeatedIndex(i, numColors);
        QColor color = Utility::getColor(mOptions.componentColors[iColor]);
        pItem->setData(Qt::DecorationRole, color);
        pItem->setData(Qt::UserRole, i);
        mpComponentList->addItem(pItem);
        if (i < numMask)
            pItem->setSelected(mOptions.maskComponents[i]);
    }
}

//! Create all the widgets
void GeometryViewEditor::createContent()
{
    // Create the component list
    mpComponentList = new QListWidget;
    mpComponentList->setFont(font());
    mpComponentList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mpComponentList->setResizeMode(QListWidget::Adjust);
    mpComponentList->setSizeAdjustPolicy(QListWidget::AdjustToContents);

    // Combine all the widgets
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->addWidget(new QLabel(tr("Components: ")));
    pLayout->addWidget(mpComponentList);

    // Set the layout
    setLayout(pLayout);
}

//! Specify the connections between widgets
void GeometryViewEditor::createConnections()
{
    connect(mpComponentList, &QListWidget::itemSelectionChanged, this, &GeometryViewEditor::processComponentSelectionChanged);
    connect(mpComponentList, &QListWidget::itemDoubleClicked, this, &GeometryViewEditor::processComponentDoubleClicked);
}

//! Process changing selection of components
void GeometryViewEditor::processComponentSelectionChanged()
{
    // Retrieve the selected items
    QList<QListWidgetItem*> selectedItems = mpComponentList->selectedItems();
    int numSelected = selectedItems.size();
    mOptions.maskComponents.fill(false);
    for (int i = 0; i != numSelected; ++i)
    {
        QListWidgetItem* pSelectedItem = selectedItems[i];
        int iComponent = pSelectedItem->data(Qt::UserRole).toInt();
        if (iComponent >= 0 && iComponent < mOptions.maskComponents.size())
            mOptions.maskComponents[iComponent] = true;
    }

    // Finish up the editing process
    emit edited();
}

//! Process changing color of components
void GeometryViewEditor::processComponentDoubleClicked(QListWidgetItem* pItem)
{
    // Block all the signals
    QSignalBlocker blocker(mpComponentList);

    // Retrieve the current data
    int iComponent = pItem->data(Qt::UserRole).toInt();
    QColor color = pItem->data(Qt::DecorationRole).value<QColor>();

    // Set the new color
    color = QColorDialog::getColor(color, this, tr("Set component color"));
    pItem->setData(Qt::DecorationRole, color);
    if (!mOptions.componentColors.isEmpty())
    {
        while (iComponent >= mOptions.componentColors.size())
            mOptions.componentColors.append(mOptions.componentColors);
        mOptions.componentColors[iComponent] = Utility::getColor(color);
    }

    // Finish up the editing process
    emit edited();
}

GeometryDependencyEditor::GeometryDependencyEditor(Testlab::Geometry& geometry, QWidget* pParent)
    : QDialog(pParent)
    , mGeometry(geometry)
{
    setFont(Utility::getFont());
    setWindowTitle(tr("Dependency Editor"));
    createContent();
    connect(mpTable, &CustomTable::pasted, this, &GeometryDependencyEditor::setData);
}

QSize GeometryDependencyEditor::sizeHint() const
{
    return QSize(600, 600);
}

//! Update the widgets content
void GeometryDependencyEditor::refresh()
{
    // Constants
    int const kNumCols = 8;
    QStringList const kLabels = {"Slave", "X", "Y", "Z", "Master 1", "Master 2", "Master 3", "Master 4"};

    // Set the table dimensions
    QSignalBlocker blocker(mpTable);
    mpTable->clear();
    std::vector<Testlab::Dependency> const& dependencies = mGeometry.dependencies;
    int numDependencies = dependencies.size();
    mpTable->setRowCount(numDependencies);
    mpTable->setColumnCount(kNumCols);
    mpTable->setHorizontalHeaderLabels(kLabels);

    // Set the dependencies
    for (int iDepend = 0; iDepend != numDependencies; ++iDepend)
    {
        Testlab::Dependency const& dependency = dependencies[iDepend];

        // Slave
        QTableWidgetItem* pSlaveItem = Utility::createTableItem(QString::fromStdWString(dependency.slave));
        mpTable->setItem(iDepend, 0, pSlaveItem);

        // Flags
        int numFlags = dependency.flags.size();
        for (int iFlag = 0; iFlag != numFlags; ++iFlag)
        {
            QTableWidgetItem* pFlagItem = Utility::createTableItem(getFlagLabel(dependency.flags[iFlag]));
            mpTable->setItem(iDepend, 1 + iFlag, pFlagItem);
        }

        // Masters
        int numMasters = dependency.masters.size();
        for (int iMaster = 0; iMaster != numMasters; ++iMaster)
        {
            QTableWidgetItem* pMasterItem = Utility::createTableItem(QString::fromStdWString(dependency.masters[iMaster]));
            mpTable->setItem(iDepend, 4 + iMaster, pMasterItem);
        }
    }
}

//! Remove all the dependencies
void GeometryDependencyEditor::clear()
{
    mGeometry.dependencies.clear();
    refresh();
    emit edited();
}

//! Create all the widgets
void GeometryDependencyEditor::createContent()
{
    // Create the actions
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addAction(QIcon(":/icons/edit-remove.svg"), tr("Clear"), this, &GeometryDependencyEditor::clear);
    pToolBar->setIconSize(Size::skToolBarIcon);

    // Create the widgets
    mpTable = new CustomTable;
    mpTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    mpTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Combine the widgets
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->addWidget(pToolBar);
    pLayout->addWidget(mpTable);
    setLayout(pLayout);
}

//! Set the dependencies from the editor
void GeometryDependencyEditor::setData()
{
    // Constants
    int const kNumCols = 8;
    int const kNumFlags = 3;
    int const kNumMasters = 4;

    // Get the tabular dimensions
    int numRows = mpTable->rowCount();
    int numCols = mpTable->columnCount();
    if (numCols != kNumCols)
    {
        qWarning() << tr("Dependency set is not valid");
        refresh();
        return;
    }

    // Set the dependencies
    std::vector<Testlab::Dependency>& dependencies = mGeometry.dependencies;
    dependencies.resize(numRows, Testlab::Dependency());
    for (int iDepend = 0; iDepend != numRows; ++iDepend)
    {
        Testlab::Dependency& dependency = dependencies[iDepend];

        // Slave
        QTableWidgetItem* pSlaveItem = mpTable->item(iDepend, 0);
        if (pSlaveItem)
            dependency.slave = pSlaveItem->text().toStdWString();

        // Flags
        dependency.flags.resize(kNumFlags);
        for (int iFlag = 0; iFlag != kNumFlags; ++iFlag)
        {
            QTableWidgetItem* pFlagItem = mpTable->item(iDepend, 1 + iFlag);
            if (pFlagItem)
                dependency.flags[iFlag] = getFlagValue(pFlagItem->text());
        }

        // Masters
        dependency.masters.resize(kNumMasters);
        for (int iMaster = 0; iMaster != kNumMasters; ++iMaster)
        {
            QTableWidgetItem* pMasterItem = mpTable->item(iDepend, 4 + iMaster);
            if (pMasterItem)
                dependency.masters[iMaster] = pMasterItem->text().toStdWString();
        }
    }

    // Finish up the editing
    qInfo() << tr("Geometry dependencies edited");
    emit edited();
}

//! Convert an integer to a Testlab text flag
QString GeometryDependencyEditor::getFlagLabel(int value)
{
    return value > 0 ? "TRUE" : "FALSE";
}

//! Convert a Testlab flag to an integer value
int GeometryDependencyEditor::getFlagValue(QString const& label)
{
    return label == "TRUE";
}

GeometrySelection::GeometrySelection()
    : iComponent(-1)
    , iNode(-1)
{
}

GeometrySelection::GeometrySelection(int uiComponent, int uiNode)
    : iComponent(uiComponent)
    , iNode(uiNode)
{
}

bool GeometrySelection::operator==(GeometrySelection const& another) const
{
    return std::tie(iComponent, iNode) == std::tie(another.iComponent, another.iNode);
}

bool GeometrySelection::operator!=(GeometrySelection const& another) const
{
    return !(*this == another);
}

bool GeometrySelection::operator<(GeometrySelection const& another) const
{
    return std::tie(iComponent, iNode) < std::tie(another.iComponent, another.iNode);
}

bool GeometrySelection::operator>(GeometrySelection const& another) const
{
    return !(*this < another);
}

bool GeometrySelection::operator<=(GeometrySelection const& another) const
{
    return *this < another || *this == another;
}

bool GeometrySelection::operator>=(GeometrySelection const& another) const
{
    return *this > another || *this == another;
}

bool GeometrySelection::isValid() const
{
    return iComponent >= 0 && iNode >= 0;
}

GeometryInteractorStyle::GeometryInteractorStyle()
    : pickTolerance(0.1)
    , pickFactor(2.0)
{
}

//! Process left button click
void GeometryInteractorStyle::OnLeftButtonDown()
{
    // Get the window interactor
    vtkRenderWindowInteractor* interactor = GetInteractor();

    // Get the location of the click (in window coordinates)
    int* position = interactor->GetEventPosition();

    // Construct the picker
    vtkNew<vtkCellPicker> picker;
    picker->SetTolerance(pickTolerance);
    picker->PickFromListOn();
    for (auto const [key, value] : mActors.asKeyValueRange())
        picker->AddPickList(value);

    // Pick from this location
    picker->Pick(position[0], position[1], 0.0, GetDefaultRenderer());

    // Select the last actor
    vtkActor* actor = picker->GetActor();
    if (actor)
    {
        // Display the information
        auto toString = [](double value) { return QString::number(value, 'f', 2); };
        auto center = actor->GetCenter();
        vtkInformation* info = actor->GetProperty()->GetInformation();
        const char* text = vtkNameKey->Get(info);
        QString name(text);
        QString label = QString("%1 = (%2, %3, %4)").arg(name, toString(center[0]), toString(center[1]), toString(center[2]));
        qInfo() << label;

        // Perform the selection
        select(actor);
    }

    // Forward events
    vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
}

//! Process key press events
void GeometryInteractorStyle::OnKeyPress()
{
    // Forward events
    vtkInteractorStyleTrackballCamera::OnKeyPress();

    // Retrieve the window interactor
    vtkRenderWindowInteractor* interactor = GetInteractor();

    // Get the pressed key
    std::string key = interactor->GetKeySym();

    // Process the press
    if (key == "Escape" || key == "BackSpace" || key == "Delete")
    {
        deselectAll();
        interactor->Render();
    }
}

//! Retrieve the current selection
QList<GeometrySelection> GeometryInteractorStyle::selections() const
{
    QList<GeometrySelection> result;
    int numSelected = mSelectedActors.size();
    result.reserve(numSelected);
    for (vtkActor* actor : mSelectedActors)
    {
        GeometrySelection key = find(actor);
        if (key.isValid())
            result.push_back(key);
    }
    return result;
}

//! Select the actors associated with the requested keys
void GeometryInteractorStyle::select(QList<GeometrySelection> const& keys)
{
    int numKeys = keys.size();
    for (int i = 0; i != numKeys; ++i)
    {
        GeometrySelection key = keys[i];
        if (mActors.contains(key))
            select(mActors[key]);
    }
}

//! Add the actor to the selection set
void GeometryInteractorStyle::select(vtkActor* actor)
{
    // Deselect the actor on the second click
    if (mSelectedActors.contains(actor))
    {
        deselect(actor);
        return;
    }

    // Change the visual representation of the actor
    vtkNew<vtkProperty> property;
    property->DeepCopy(actor->GetProperty());
    actor->GetProperty()->SetColor(vtkColors->GetColor3d("Red").GetData());
    actor->GetProperty()->SetDiffuse(1.0);
    actor->GetProperty()->SetSpecular(0.0);
    actor->GetProperty()->SetEdgeColor(actor->GetProperty()->GetColor());
    actor->GetProperty()->EdgeVisibilityOn();

    // Modify the actor scale
    setActorScale(actor, pickFactor);

    // Save the original property
    mSelectedActors.push_back(actor);
    mProperties[actor] = property;
}

//! Remove the actor from the selection set
void GeometryInteractorStyle::deselect(vtkActor* actor)
{
    // Check if there is such actor on the scene
    if (!mSelectedActors.contains(actor))
        return;

    // Set the original properties
    actor->GetProperty()->DeepCopy(mProperties[actor]);

    // Modify the actor scale
    setActorScale(actor, 1.0 / pickFactor);

    // Remove the actor from the selection
    mSelectedActors.remove(index(actor));
    mProperties.remove(actor);
}

//! Deselect all the actors associated with a model entity
void GeometryInteractorStyle::deselect(GeometrySelection key)
{
    if (mActors.contains(key))
        deselect(mActors[key]);
}

//! Remove all the actors from the selection set
void GeometryInteractorStyle::deselectAll()
{
    QList<GeometrySelection> const keys = mActors.keys();
    int numKeys = keys.size();
    for (int iKey = 0; iKey != numKeys; ++iKey)
    {
        vtkActor* value = mActors[keys[iKey]];
        deselect(value);
    }
}

//! Remove all the items created via interaction
void GeometryInteractorStyle::clear()
{
    mActors.clear();
}

//! Register the actor under specified selection
void GeometryInteractorStyle::registerActor(GeometrySelection const& key, vtkActor* value)
{
    mActors[key] = value;
}

//! Find a selection by actor
GeometrySelection GeometryInteractorStyle::find(vtkActor* actor) const
{
    for (auto const [key, value] : mActors.asKeyValueRange())
    {
        if (value == actor)
            return key;
    }
    return GeometrySelection();
}

//! Get the selected actor index
int GeometryInteractorStyle::index(vtkActor* actor)
{
    int numSelected = mSelectedActors.size();
    for (int i = 0; i != numSelected; ++i)
    {
        if (mSelectedActors[i] == actor)
            return i;
    }
    return -1;
}

//! Helper function to multiply actor scale
void setActorScale(vtkActor* actor, double factor)
{
    // Set the origin
    auto center = actor->GetCenter();
    actor->SetOrigin(center);

    // Multiply the scale
    double scale[3];
    actor->GetScale(scale);
    actor->SetScale(scale[0] * factor, scale[1] * factor, scale[2] * factor);
}

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

#include <Eigen/Geometry>

#include <vtkAxesActor.h>
#include <vtkLine.h>
#include <vtkLookupTable.h>
#include <vtkNamedColors.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolygon.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkTextActor.h>

#include "diagramreportsceneitem.h"
#include "mathutility.h"
#include "reportitem.h"
#include "reporttextengine.h"
#include "session.h"
#include "uiutility.h"

using namespace Backend::Core;
using namespace Frontend;
using namespace Eigen;

// Constants
vtkNew<vtkNamedColors> const vtkColors;
static double const skEps = std::numeric_limits<double>::epsilon();
static vtkColor3d const skTextColor = vtkColors->GetColor3d("Black");

// Helpers
Eigen::Vector3d getBinormalVector(ReportView view);

DiagramReportSceneItem::DiagramReportSceneItem(DiagramReportItem* pItem, ReportTextEngine& textEngine, ResponseCollection const& collection,
                                               int iSelectedBundle, Testlab::Geometry const& geometry, QGraphicsItem* pParent)
    : ReportSceneItem(pItem, pParent)
    , mTextEngine(textEngine)
    , mCollection(collection)
    , mISelectedBundle(iSelectedBundle)
    , mGeometry(geometry)
{
    initialize();
    setState();
    replot();
}

DiagramReportSceneItem::~DiagramReportSceneItem()
{
}

//! Set the item state
void DiagramReportSceneItem::setState()
{
    // Reset the state
    mState.clear();

    // Get the report item
    if (!mpItem)
        return;
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;

    // Get the currently active bundle of responses
    if (mCollection.isEmpty())
        return;
    ResponseBundle const& bundle = mISelectedBundle >= 0 ? mCollection.get(mISelectedBundle) : mCollection.get(0);
    if (bundle.freq < skEps)
    {
        qWarning() << tr("The bundle %1 has zero frequency. Skipping rendering the modeshape").arg(bundle.name);
        return;
    }

    // Update the parser
    mTextEngine.setVariable("bundle", bundle.name);
    mTextEngine.setVariable("unit", pItem->unit);

    // Set the vertex field
    mState = Backend::Utility::getGeometryState(pItem->unit, bundle, mGeometry);

    // Resolve the depenedencies
    Backend::Utility::resolveGeometryStateSlaves(mState, mGeometry);
}

//! Set the camera position as well as zoom
void DiagramReportSceneItem::setView()
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    Utility::setView(pItem->view, pItem->scale, mRenderer, mOverlayRenderer);
}

//! Represent geometry
void DiagramReportSceneItem::drawAll()
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Estimate the maximum dimension
    mMaximumDimension = Backend::Utility::getMaximumDimension(mGeometry);
    if (mMaximumDimension < skEps)
        mMaximumDimension = 1.0;

    // Draw the initial configuration
    drawUndeformedState();

    // Draw the deformed configuration
    drawDeformedState();

    // Render the title
    drawTitle();
}

//! Represent the initial configuration
void DiagramReportSceneItem::drawUndeformedState()
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Check if the state is valid to be rendered
    if (mState.isEmpty())
        return;

    // Loop through all the components
    int numComponents = mGeometry.components.size();
    vtkColor3d color = Utility::getColor(pItem->undeformedColor);
    for (int i = 0; i != numComponents; ++i)
    {
        Testlab::Component const& component = mGeometry.components[i];

        // Construct the vertices
        vtkSmartPointer<vtkPoints> points = createPoints(component);

        // Draw the elements
        drawElements(points, component.lines, color, false);
        drawElements(points, component.trias, color, false);
        drawElements(points, component.quads, color, false);
    }
}

//! Represent the vertex field
void DiagramReportSceneItem::drawDeformedState()
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Check if the state is valid to be rendered
    if (mState.isEmpty())
        return;
    PairDouble range = Backend::Utility::getMagnitudeRange(mState, mGeometry);
    if (std::abs(range.second - range.first) < skEps)
        return;

    // Create the lookup table
    double limit = std::max(std::abs(range.first), std::abs(range.second));
    vtkSmartPointer<vtkLookupTable> lookupTable = Utility::createLookupTable(pItem->colorMap, -limit, limit);

    // Set the mode parametsr
    double scale = pItem->amplitude * mMaximumDimension / limit;
    double phase = pItem->phase * M_PI / 180.0;

    // Loop through all the sections
    int numSections = pItem->sections.size();
    for (int i = 0; i != numSections; ++i)
        drawSection(pItem->sections[i], lookupTable, scale, phase);

    // Show the scalar bar
    drawScalarBar(lookupTable);
}

//! Render elements using one color
void DiagramReportSceneItem::drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color,
                                          bool isWireframe)
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Check if there are any elements to render
    if (indices.empty())
        return;

    // Create polygons
    vtkSmartPointer<vtkCellArray> polygons = Utility::createPolygons(indices);

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

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color.GetData());
    actor->GetProperty()->SetLineWidth(pItem->lineWidth);
    actor->GetProperty()->SetEdgeColor(color.GetData());
    actor->GetProperty()->EdgeVisibilityOn();
    if (isWireframe)
        actor->GetProperty()->SetRepresentationToWireframe();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Render the section
void DiagramReportSceneItem::drawSection(ReportSection const& section, vtkSmartPointer<vtkLookupTable> lookupTable, double scale, double phase)
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Check if the coordinate direction is specified
    if (section.coordDir == ReportDirection::kNone)
        return;
    int iCoordDir = (int) section.coordDir - 1;

    // Check if the response direction is specified
    if (section.responseDir == ReportDirection::kNone)
        return;
    int iResponseDir = (int) section.responseDir - 1;

    // Update the parser
    mTextEngine.setVariable("cdir", Backend::Utility::getDirLabel(section.coordDir));
    mTextEngine.setVariable("rdir", Backend::Utility::getDirLabel(section.responseDir));

    // Get the points
    ReportPoint firstPoint = section.firstPoint;
    if (firstPoint.isEmpty())
        return;
    ReportPoint secondPoint = section.secondPoint;
    if (secondPoint.isEmpty())
        secondPoint = firstPoint;

    // Get the coords
    Vector3d firstCoords = Backend::Utility::convert3d(Backend::Utility::getNodeCoords(mGeometry, firstPoint.component, firstPoint.node));
    Vector3d secondCoords = Backend::Utility::convert3d(Backend::Utility::getNodeCoords(mGeometry, secondPoint.component, secondPoint.node));

    // Compute the tangent vector
    Vector3d tangentVec = secondCoords - firstCoords;
    double distance = tangentVec.norm();
    bool isOnePoint = distance < skEps;

    // Compute the normal vector
    Vector3d normalVec = Vector3d::Zero();
    if (section.coordDir == ReportDirection::kN)
    {
        if (isOnePoint)
        {
            qWarning() << tr("Could not process the section consisted of %1:%2 in normal direction").arg(firstPoint.component, firstPoint.node);
            return;
        }
        else
        {
            Vector3d binormalVec = getBinormalVector(pItem->view);
            binormalVec /= binormalVec.norm();
            tangentVec /= distance;
            normalVec = tangentVec.cross(binormalVec);
        }
    }
    else
    {
        normalVec = Vector3d::Unit(iCoordDir);
    }
    normalVec *= section.sign;

    // Get the epure values
    double factor = scale * cos(phase);
    double firstValue = factor * Backend::Utility::getNodeValues(mState, firstPoint.component, firstPoint.node)[iResponseDir];
    double secondValue = factor * Backend::Utility::getNodeValues(mState, secondPoint.component, secondPoint.node)[iResponseDir];

    // Draw the epure
    drawZeroLine(firstCoords, secondCoords);
    drawEpure(firstCoords, secondCoords, firstValue, secondValue, normalVec, lookupTable);
}

//! Render the scalar bar
void DiagramReportSceneItem::drawScalarBar(vtkSmartPointer<vtkLookupTable> lookupTable)
{
    // Constants
    double const kRelMaxWidth = 1.0 / 5.0;

    // Get the report item
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Create the title actor
    QString title = mTextEngine.process(pItem->sLabel);
    vtkSmartPointer<vtkTextActor> titleActor = Utility::createScalarBarTitleActor(title, {0.96, 0.35}, {1.0, 0.55}, pItem->font.pointSize());

    // Create the scalar bar
    vtkSmartPointer<vtkScalarBarActor> scalarBar = Utility::createScalarBarActor(lookupTable, {0.9, 0.05}, {0.95, 0.6}, pItem->font.pointSize());
    int maxWidth = ceil(kRelMaxWidth * mRenderWindow->GetSize()[0]);
    scalarBar->SetMaximumWidthInPixels(maxWidth);

    // Add the actors to the scene
    mRenderer->AddActor(titleActor);
    mRenderer->AddViewProp(scalarBar);
}

//! Render the title
void DiagramReportSceneItem::drawTitle()
{
    // Get the report item
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Create the actor
    QString text = mTextEngine.process(pItem->title);
    vtkSmartPointer<vtkTextActor> actor = Utility::createTitleActor(text, {0.0, 0.0}, {0.5, 0.2}, pItem->font.pointSize());

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Create points which are associated with the geometry
vtkSmartPointer<vtkPoints> DiagramReportSceneItem::createPoints(Testlab::Component const& component)
{
    vtkNew<vtkPoints> points;
    QString componentName = QString::fromStdWString(component.name);
    int numNodes = component.nodes.size();
    for (int iNode = 0; iNode != numNodes; ++iNode)
    {
        Testlab::Node const& node = component.nodes[iNode];
        QString nodeName = QString::fromStdWString(node.name);

        // Get the nodal position
        std::vector<double> position = node.coordinates;

        // Add the point
        points->InsertPoint(iNode, position[0], position[1], position[2]);
    }
    return points;
}

//! Draw the line connecting two points
void DiagramReportSceneItem::drawZeroLine(Eigen::Vector3d const& firstCoords, Eigen::Vector3d const& secondCoords)
{
    // Create the points
    vtkNew<vtkPoints> points;
    points->InsertPoint(0, firstCoords[0], firstCoords[1], firstCoords[2]);
    points->InsertPoint(1, secondCoords[0], secondCoords[1], secondCoords[2]);

    // Create the line
    vtkNew<vtkLine> line;
    line->GetPointIds()->InsertNextId(0);
    line->GetPointIds()->InsertNextId(1);
    vtkNew<vtkCellArray> lines;
    lines->InsertNextCell(line);

    // Group the polygons
    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetLines(lines);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(vtkColors->GetColor3d("Black").GetData());
    actor->GetProperty()->SetLineWidth(4.0);

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Draw the epure based on two points
void DiagramReportSceneItem::drawEpure(Vector3d const& firstCoords, Vector3d const& secondCoords, double firstValue, double secondValue,
                                       Eigen::Vector3d const& normalVec, vtkSmartPointer<vtkLookupTable> lookupTable)
{
    // Compute the state vectors
    Vector3d firstState = firstValue * normalVec;
    Vector3d secondState = secondValue * normalVec;

    // Create the points
    vtkNew<vtkPoints> points;
    points->InsertPoint(0, firstCoords[0], firstCoords[1], firstCoords[2]);
    points->InsertPoint(1, secondCoords[0], secondCoords[1], secondCoords[2]);
    points->InsertPoint(2, firstCoords[0] + firstState[0], firstCoords[1] + firstState[1], firstCoords[2] + firstState[2]);
    points->InsertPoint(3, secondCoords[0] + secondState[0], secondCoords[1] + secondState[1], secondCoords[2] + secondState[2]);

    // Create the scalars
    vtkNew<vtkDoubleArray> scalars;
    scalars->SetNumberOfTuples(4);
    scalars->SetValue(0, firstValue);
    scalars->SetValue(1, secondValue);
    scalars->SetValue(2, firstValue);
    scalars->SetValue(3, secondValue);

    // Create the polygon
    vtkNew<vtkPolygon> polygon;
    polygon->GetPointIds()->InsertNextId(0);
    polygon->GetPointIds()->InsertNextId(1);
    polygon->GetPointIds()->InsertNextId(3);
    polygon->GetPointIds()->InsertNextId(2);
    vtkNew<vtkCellArray> polygons;
    polygons->InsertNextCell(polygon);

    // Group the polygons
    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetPolys(polygons);
    polyData->GetPointData()->SetScalars(scalars);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    mapper->UseLookupTableScalarRangeOn();
    mapper->SetLookupTable(lookupTable);

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    // actor->GetProperty()->SetEdgeColor(Utility::getColor(pItem->edgeColor).GetData());
    // actor->GetProperty()->SetEdgeOpacity(pItem->edgeOpacity);
    // actor->GetProperty()->EdgeVisibilityOn();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Clean up the scene
void DiagramReportSceneItem::clear()
{
    // Remove the actors
    auto actors = mRenderer->GetActors();
    while (actors->GetLastActor())
        mRenderer->RemoveActor(actors->GetLastActor());

    // Remove the view properties
    auto props = mRenderer->GetViewProps();
    while (props->GetLastProp())
        mRenderer->RemoveViewProp(props->GetLastProp());
}

//! Update the scene
void DiagramReportSceneItem::refresh()
{
    mRenderWindow->Render();
}

//! Replot the scene
void DiagramReportSceneItem::replot()
{
    // Constants
    qreal const kInchToMM = 25.4;

    // Check if the item is valid
    if (!mpItem)
        return;
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;

    // Check if there are any components to render
    if (mGeometry.components.size() == 0)
        return;

    // Set the window size
    QScreen* screen = QGuiApplication::primaryScreen();
    double dpi = screen->logicalDotsPerInch();
    auto mmToPx = [dpi, kInchToMM](double mm) { return mm * dpi / kInchToMM; };
    QSize pxSize(mmToPx(mpItem->rect.width()), mmToPx(mpItem->rect.height()));
    mRenderWindow->SetSize(pxSize.width(), pxSize.height());

    // Draw the content
    clear();
    drawAll();
    setView();
    mRenderWindow->Render();

    // Save as the image
    mImage = Utility::getImage(mRenderWindow, pItem->quality);
}

//! Process paint event
void DiagramReportSceneItem::paint(QPainter* pPainter, QStyleOptionGraphicsItem const* pOption, QWidget* pWidget)
{
    // Set the painter
    pPainter->save();
    pPainter->translate(mpItem->rect.center());
    pPainter->rotate(mpItem->angle);
    pPainter->translate(-mpItem->rect.center());

    // Draw the image
    pPainter->drawImage(mpItem->rect, mImage);

    // Restore the painter
    pPainter->restore();
    ReportSceneItem::paint(pPainter, pOption, pWidget);
}

//! Set the initial state of widgets
void DiagramReportSceneItem::initialize()
{
    // Specify the format for the VTK library
    vtkObject::GlobalWarningDisplayOff();

    // Create the main renderer
    mRenderer = vtkRenderer::New();
    mRenderer->GradientBackgroundOff();
    mRenderer->ResetCamera();
    mRenderer->SetBackgroundAlpha(0.0);
    mRenderer->SetLayer(0);

    // Create the overlay renderer
    mOverlayRenderer = vtkRenderer::New();
    mOverlayRenderer->GradientBackgroundOff();
    mOverlayRenderer->SetViewport(0.8, 0.6, 1.0, 1.0);
    mOverlayRenderer->SetBackgroundAlpha(0.0);
    mOverlayRenderer->SetLayer(1);

    // Add the axes
    mAxes = Utility::createAxesActor(mpItem->font.pointSize());
    mOverlayRenderer->AddActor(mAxes);
    mOverlayRenderer->ResetCamera();

    // Create the window
    mRenderWindow = vtkRenderWindow::New();
    mRenderWindow->OffScreenRenderingOn();
    mRenderWindow->SetNumberOfLayers(2);
    mRenderWindow->AddRenderer(mRenderer);
    mRenderWindow->AddRenderer(mOverlayRenderer);
}

//! Helper function to compute binormal vector for a given view
Vector3d getBinormalVector(ReportView view)
{
    switch (view)
    {
    case ReportView::kFront:
        return Vector3d::UnitX();
    case ReportView::kRear:
        return -Vector3d::UnitX();
    case ReportView::kTop:
        return Vector3d::UnitY();
    case ReportView::kBottom:
        return -Vector3d::UnitY();
    case ReportView::kLeft:
        return Vector3d::UnitX();
    case ReportView::kRight:
        return -Vector3d::UnitX();
    default:
        break;
    }
    return Vector3d::Zero();
}

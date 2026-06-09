#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

#include <vtkAxesActor.h>
#include <vtkCamera.h>
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

#include "mathutility.h"
#include "modereportsceneitem.h"
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

ModeReportSceneItem::ModeReportSceneItem(ModeReportItem* pItem, ReportTextEngine& textEngine, ResponseCollection const& collection,
                                         int iSelectedBundle, Testlab::Geometry const& geometry, vtkRenderWindow* renderWindow,
                                         QGraphicsItem* pParent)
    : ReportSceneItem(pItem, pParent)
    , mTextEngine(textEngine)
    , mCollection(collection)
    , mISelectedBundle(iSelectedBundle)
    , mGeometry(geometry)
    , mRenderWindow(renderWindow)
{
    initialize();
    setState();
    replot();
}

ModeReportSceneItem::~ModeReportSceneItem()
{
    // Remove the rendereres from the window
    mRenderWindow->RemoveRenderer(mRenderer);
    mRenderWindow->RemoveRenderer(mAxesRenderer);
    mRenderWindow->SetNumberOfLayers(0);

    // Free up the renderers
    mRenderer->Delete();
    mAxesRenderer->Delete();
}

//! Set the item state
void ModeReportSceneItem::setState()
{
    // Reset the state
    mState.clear();

    // Get the report item
    if (!mpItem)
        return;
    ModeReportItem* pItem = (ModeReportItem*) mpItem;

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

//! Clean up the scene
void ModeReportSceneItem::clear()
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
void ModeReportSceneItem::refresh()
{
    mRenderWindow->Render();
}

//! Replot the scene
void ModeReportSceneItem::replot()
{
    // Constants
    qreal const kInchToMM = 25.4;

    // Check if the item is valid
    if (!mpItem)
        return;
    ModeReportItem* pItem = (ModeReportItem*) mpItem;

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
void ModeReportSceneItem::paint(QPainter* pPainter, QStyleOptionGraphicsItem const* pOption, QWidget* pWidget)
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
void ModeReportSceneItem::initialize()
{
    // Specify the format for the VTK library
    vtkObject::GlobalWarningDisplayOff();

    // Initialize the lookup table
    mLookupTable = Utility::createCoolToWarmColorMap();

    // Create the main renderer
    mRenderer = vtkRenderer::New();
    mRenderer->GradientBackgroundOff();
    mRenderer->ResetCamera();
    mRenderer->SetBackgroundAlpha(0.0);
    mRenderer->SetLayer(0);

    // Create the axes renderer
    mAxesRenderer = vtkRenderer::New();
    mAxesRenderer->GradientBackgroundOff();
    mAxesRenderer->SetViewport(0.8, 0.6, 1.0, 1.0);
    mAxesRenderer->SetBackgroundAlpha(0.0);
    mAxesRenderer->SetLayer(1);

    // Add the axes actor
    mAxes = Utility::createAxesActor(mpItem->font.pointSize());
    mAxesRenderer->AddActor(mAxes);
    mAxesRenderer->ResetCamera();

    // Create the window
    mRenderWindow->SetNumberOfLayers(2);
    mRenderWindow->AddRenderer(mRenderer);
    mRenderWindow->AddRenderer(mAxesRenderer);
}

//! Set the camera position as well as zoom
void ModeReportSceneItem::setView()
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    Utility::setView(pItem->view, pItem->viewAngle, pItem->scale, mRenderer, mAxesRenderer);
}

//! Represent all the elements
void ModeReportSceneItem::drawAll()
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Estimate the maximum dimension
    mMaximumDimension = Backend::Utility::getMaximumDimension(mGeometry);
    if (mMaximumDimension < skEps)
        mMaximumDimension = 1.0;

    // Render the undeformed state
    if (pItem->showUndeformed)
        drawUndeformedState();

    // Render the deformed state
    drawDeformedState();

    // Render the title
    drawTitle();
}

//! Represent the initial configuration
void ModeReportSceneItem::drawUndeformedState()
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Check if the state is valid to be rendered
    if (mState.isEmpty())
        return;

    // Loop through all the components
    int numComponents = mGeometry.components.size();
    vtkColor3d color = Utility::getColor(pItem->undeformedColor);
    bool isMask = pItem->maskComponents.size() == numComponents;
    for (int i = 0; i != numComponents; ++i)
    {
        if (isMask && !pItem->maskComponents[i])
            continue;
        Testlab::Component const& component = mGeometry.components[i];

        // Construct the vertices
        vtkSmartPointer<vtkPoints> points = createPoints(component);

        // Draw the elements
        drawElements(points, component.lines, color, 1.0, true, true);
        drawElements(points, component.trias, color, 1.0, true, true);
        drawElements(points, component.quads, color, 1.0, true, true);
    }
}

//! Represent the vertex field
void ModeReportSceneItem::drawDeformedState()
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
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
    mLookupTable = Utility::createLookupTable(pItem->colorMap, -limit, limit);

    // Set the mode parametsr
    double scale = pItem->amplitude * mMaximumDimension / limit;

    // Loop through all the components
    int numComponents = mGeometry.components.size();
    bool isMask = pItem->maskComponents.size() == numComponents;
    for (int i = 0; i != numComponents; ++i)
    {
        if (isMask && !pItem->maskComponents[i])
            continue;
        Testlab::Component const& component = mGeometry.components[i];

        // Construct the vertices
        vtkSmartPointer<vtkPoints> points = createPoints(component, scale);

        // Compute the magnitudes
        vtkSmartPointer<vtkDoubleArray> magnitudes = getMagnitudes(component);

        // Draw the vertices
        drawVertices(points, magnitudes);

        // Draw the elements
        drawElements(points, component.lines, magnitudes);
        drawElements(points, component.trias, magnitudes);
        drawElements(points, component.quads, magnitudes);
    }

    // Show the scalar bar
    drawScalarBar();
}

//! Render color interpolated vertices
void ModeReportSceneItem::drawVertices(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkDoubleArray> scalars)
{
    // Get the report item
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Create the topology
    vtkNew<vtkCellArray> vertices;
    int numPoints = points->GetNumberOfPoints();
    for (int i = 0; i != numPoints; ++i)
    {
        vertices->InsertNextCell(1);
        vertices->InsertCellPoint(i);
    }

    // Build up the polygons
    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetVerts(vertices);
    polyData->GetPointData()->SetScalars(scalars);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    mapper->UseLookupTableScalarRangeOn();
    mapper->SetLookupTable(mLookupTable);

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetPointSize((float) pItem->vertexSize);

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Render elements using one color
void ModeReportSceneItem::drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color,
                                       double opacity, bool isEdgeVisible, bool isWireframe)
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
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
    actor->GetProperty()->SetLineWidth(pItem->lineWidth);
    if (isEdgeVisible)
    {
        actor->GetProperty()->SetEdgeColor(Utility::getColor(pItem->edgeColor).GetData());
        actor->GetProperty()->SetEdgeOpacity(pItem->edgeOpacity);
        actor->GetProperty()->EdgeVisibilityOn();
    }
    if (isWireframe)
        actor->GetProperty()->SetRepresentationToWireframe();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Render color interpolated elements
void ModeReportSceneItem::drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices,
                                       vtkSmartPointer<vtkDoubleArray> scalars, bool isWireframe)
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
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
    polyData->GetPointData()->SetScalars(scalars);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    mapper->UseLookupTableScalarRangeOn();
    mapper->SetLookupTable(mLookupTable);

    // Create the actor and add to the scene
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetLineWidth(pItem->lineWidth);
    actor->GetProperty()->SetEdgeColor(Utility::getColor(pItem->edgeColor).GetData());
    actor->GetProperty()->SetEdgeOpacity(pItem->edgeOpacity);
    actor->GetProperty()->EdgeVisibilityOn();
    if (isWireframe)
        actor->GetProperty()->SetRepresentationToWireframe();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Render the scalar bar
void ModeReportSceneItem::drawScalarBar()
{
    // Constants
    double const kRelMaxWidth = 1.0 / 5.0;

    // Get the report item
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Create the title actor
    QString title = mTextEngine.process(pItem->sLabel);
    vtkSmartPointer<vtkTextActor> titleActor = Utility::createScalarBarTitleActor(title, {0.98, 0.35}, {1.0, 0.55}, pItem->font.pointSize());

    // Create the scalar bar
    vtkSmartPointer<vtkScalarBarActor> scalarBar = Utility::createScalarBarActor(mLookupTable, {0.9, 0.05}, {0.95, 0.6}, pItem->font.pointSize());
    int maxWidth = ceil(kRelMaxWidth * mRenderWindow->GetSize()[0]);
    scalarBar->SetMaximumWidthInPixels(maxWidth);

    // Add the actors to the scene
    mRenderer->AddActor(titleActor);
    mRenderer->AddViewProp(scalarBar);
}

//! Render the title
void ModeReportSceneItem::drawTitle()
{
    // Get the report item
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Create the actor
    QString text = mTextEngine.process(pItem->title);
    vtkSmartPointer<vtkTextActor> actor = Utility::createTitleActor(text, {0.0, 0.0}, {0.5, 0.2}, pItem->font.pointSize());

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Create points which are associated with the geometry
vtkSmartPointer<vtkPoints> ModeReportSceneItem::createPoints(Testlab::Component const& component, double scale)
{
    vtkNew<vtkPoints> points;
    QString componentName = QString::fromStdWString(component.name);
    int numNodes = component.nodes.size();
    for (int iNode = 0; iNode != numNodes; ++iNode)
    {
        Testlab::Node const& node = component.nodes[iNode];
        QString nodeName = QString::fromStdWString(node.name);

        // Get the nodal position
        Vector3d position = Backend::Utility::convert3d(node.coordinates);

        // Apply the values
        Vector3d values = Backend::Utility::getNodeValues(mState, componentName, nodeName);
        position += values * scale;

        // Add the point
        points->InsertPoint(iNode, position[0], position[1], position[2]);
    }
    return points;
}

//! Get magnitudes at each node
vtkSmartPointer<vtkDoubleArray> ModeReportSceneItem::getMagnitudes(Testlab::Component const& component)
{
    // Allocate the result
    int numNodes = component.nodes.size();
    vtkNew<vtkDoubleArray> magnitudes;
    magnitudes->SetNumberOfTuples(numNodes);

    // Loop through all the nodes
    QString componentName = QString::fromStdWString(component.name);
    for (int iNode = 0; iNode != numNodes; ++iNode)
    {
        Testlab::Node const& node = component.nodes[iNode];
        QString nodeName = QString::fromStdWString(node.name);

        // Find the maximum absolute node value
        Vector3d values = Backend::Utility::getNodeValues(mState, componentName, nodeName);
        double magnitude = Backend::Utility::getSignedAbsMax(values);

        // Set the magnitude
        magnitudes->SetValue(iNode, magnitude);
    }
    return magnitudes;
}


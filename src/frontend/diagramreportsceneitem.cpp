#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

#include <Eigen/Geometry>

#include <vtkAxesActor.h>
#include <vtkCamera.h>
#include <vtkCoordinate.h>
#include <vtkLine.h>
#include <vtkLookupTable.h>
#include <vtkMatrix4x4.h>
#include <vtkNamedColors.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkPolygon.h>
#include <vtkProperty.h>
#include <vtkProperty2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
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
Vector2d worldToViewport(vtkRenderer* renderer, Vector3d const& world);

DiagramReportSceneItem::DiagramReportSceneItem(DiagramReportItem* pItem, ReportTextEngine& textEngine, ResponseCollection const& collection,
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

DiagramReportSceneItem::~DiagramReportSceneItem()
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
    Utility::setView(pItem->view, pItem->viewAngle, pItem->scale, mRenderer, mAxesRenderer);
}

//! Represent geometry
void DiagramReportSceneItem::drawAll()
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Set the rendering parameters
    mMaximumDimension = Backend::Utility::getMaximumDimension(mGeometry);
    if (mMaximumDimension < skEps)
        mMaximumDimension = 1.0;
    mAmplitudeScale = 0.0;

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
    double opacity = pItem->undeformedOpacity;
    bool isMask = pItem->maskComponents.size() == numComponents;
    for (int i = 0; i != numComponents; ++i)
    {
        if (isMask && !pItem->maskComponents[i])
            continue;
        Testlab::Component const& component = mGeometry.components[i];

        // Construct the vertices
        vtkSmartPointer<vtkPoints> points = createPoints(component);

        // Draw the elements
        drawElements(points, component.lines, color, opacity, false);
        drawElements(points, component.trias, color, opacity, false);
        drawElements(points, component.quads, color, opacity, false);
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
    auto [minLimit, maxLimit] = Backend::Utility::getColorMagnitudeRange(mState, mGeometry, ReportColorTransform::kMax);
    if (std::abs(maxLimit - minLimit) < skEps)
        return;

    // Create the lookup table
    mLookupTable = Utility::createLookupTable(pItem->colorMap, minLimit, maxLimit);

    // Set the mode parametsr
    mAmplitudeScale = pItem->amplitude * mMaximumDimension / maxLimit;

    // Loop through all the sections
    int numSections = pItem->sections.size();
    for (int i = 0; i != numSections; ++i)
        drawSection(pItem->sections[i]);

    // Show the scalar bar
    if (pItem->showScalarBar)
        drawScalarBar();
}

//! Render elements using one color
void DiagramReportSceneItem::drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color,
                                          double opacity, bool isWireframe)
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
    actor->GetProperty()->SetOpacity(opacity);
    actor->GetProperty()->SetLineWidth(pItem->lineWidth);
    actor->GetProperty()->LightingOff();
    if (isWireframe)
        actor->GetProperty()->SetRepresentationToWireframe();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Render the section
void DiagramReportSceneItem::drawSection(ReportSection const& section)
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

    // Get the binormal vector
    Vector3d binormalVec = getBinormalVector(pItem->view);
    binormalVec /= binormalVec.norm();

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
            tangentVec /= distance;
            normalVec = tangentVec.cross(binormalVec);
        }
    }
    else
    {
        normalVec = Vector3d::Unit(iCoordDir);
    }

    // Check if the normal vector is valid
    if (normalVec.norm() < skEps)
    {
        qWarning() << tr("The normal for %1 → %2 is singular. The Y direction is chosen automatically. Change the direction manually")
                          .arg(section.firstPoint.name(), section.secondPoint.name());
        normalVec = Vector3d::Unit(1);
    }
    normalVec *= section.sign;

    // Process the degenerate section
    if (isOnePoint)
    {
        tangentVec = normalVec.cross(binormalVec);
        tangentVec /= tangentVec.norm();
        double shift = pItem->barWidth * mMaximumDimension;
        firstCoords -= shift * tangentVec;
        secondCoords += shift * tangentVec;
    }

    // Get the epure values
    double firstValue = mAmplitudeScale * Backend::Utility::getNodeValues(mState, firstPoint.component, firstPoint.node)[iResponseDir];
    double secondValue = mAmplitudeScale * Backend::Utility::getNodeValues(mState, secondPoint.component, secondPoint.node)[iResponseDir];

    // Draw the epure
    drawZeroLine(firstCoords, secondCoords);
    if (firstValue * secondValue < 0.0)
        drawTriEpure(firstCoords, secondCoords, firstValue, secondValue, normalVec);
    else
        drawQuadEpure(firstCoords, secondCoords, firstValue, secondValue, normalVec);
}

//! Render the scalar bar
void DiagramReportSceneItem::drawScalarBar()
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
    vtkSmartPointer<vtkScalarBarActor> scalarBar = Utility::createScalarBarActor(mLookupTable, {0.9, 0.05}, {0.95, 0.6}, pItem->font.pointSize());
    int maxWidth = ceil(kRelMaxWidth * mRenderWindow->GetSize()[0]);
    scalarBar->SetMaximumWidthInPixels(maxWidth);

    // Add the actors to the scene
    mRenderer->AddActor(titleActor);
    mRenderer->AddViewProp(scalarBar);
}

//! Render the scale ruler
//! * Must be rendered after all the procedures once the transformation matrices are ready
void DiagramReportSceneItem::drawRuler()
{
    // Constants
    double const kX = 0.05;
    double const kY = 0.9;
    double const kT = 0.1;

    // Check if the deformed state is present
    if (std::abs(mAmplitudeScale) < skEps)
        return;

    // Compute the ruler length in the viewport coordinates
    double* range = mLookupTable->GetRange();
    double maxAbsRange = std::max(std::abs(range[0]), std::abs(range[1]));
    double limit = mAmplitudeScale * maxAbsRange;
    Vector2d normStart = worldToViewport(mRenderer, {0, 0, 0});
    Vector2d normEnd = worldToViewport(mRenderer, {limit, limit, limit});
    double a = (normEnd - normStart).norm() / std::sqrt(3.0);
    double t = a * kT;

    // Create the points
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->InsertPoint(0, kX, kY, 0.0);
    points->InsertPoint(1, kX + a, kY, 0.0);
    points->InsertPoint(2, kX, kY - a, 0.0);
    points->InsertPoint(3, kX + a, kY + t, 0.0);
    points->InsertPoint(4, kX + a, kY - t, 0.0);
    points->InsertPoint(5, kX + t, kY - a, 0.0);
    points->InsertPoint(6, kX - t, kY - a, 0.0);

    // Create the main lines
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    vtkNew<vtkLine> hMain;
    vtkNew<vtkLine> vMain;
    hMain->GetPointIds()->SetId(0, 0);
    hMain->GetPointIds()->SetId(1, 1);
    vMain->GetPointIds()->SetId(0, 0);
    vMain->GetPointIds()->SetId(1, 2);
    lines->InsertNextCell(hMain);
    lines->InsertNextCell(vMain);

    // Create the ticks
    vtkNew<vtkLine> hTick;
    vtkNew<vtkLine> vTick;
    hTick->GetPointIds()->SetId(0, 3);
    hTick->GetPointIds()->SetId(1, 4);
    vTick->GetPointIds()->SetId(0, 5);
    vTick->GetPointIds()->SetId(1, 6);
    lines->InsertNextCell(hTick);
    lines->InsertNextCell(vTick);

    // Set the polygon data
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);

    // Create the transformation
    vtkSmartPointer<vtkCoordinate> coord = vtkSmartPointer<vtkCoordinate>::New();
    coord->SetCoordinateSystemToNormalizedViewport();

    // Map the polygons
    vtkSmartPointer<vtkPolyDataMapper2D> mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
    mapper->SetInputData(polyData);
    mapper->SetTransformCoordinate(coord);

    // Create the ruler actor
    vtkSmartPointer<vtkActor2D> actor = vtkSmartPointer<vtkActor2D>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(vtkColors->GetColor3d("Black").GetData());
    actor->GetProperty()->SetLineWidth(3.0);

    // Create the labels
    int fontSize = mpItem->font.pointSize();
    QString rangeText = QString::number(maxAbsRange, 'g', 1);
    QString unitText = mTextEngine.getValue("unit");
    vtkSmartPointer<vtkTextActor> zLabel = Utility::createLabelActor("0", {kX - t, kY + t}, fontSize, VTK_TEXT_LEFT);
    vtkSmartPointer<vtkTextActor> hLabel = Utility::createLabelActor(rangeText, {kX + a, kY + t}, fontSize, VTK_TEXT_CENTERED);
    vtkSmartPointer<vtkTextActor> vLabel = Utility::createLabelActor(rangeText, {kX - 2 * t, kY - a - 3 * t}, fontSize, VTK_TEXT_RIGHT);
    vtkSmartPointer<vtkTextActor> uLabel = Utility::createLabelActor(unitText, {kX + 2 * t, kY - a}, fontSize, VTK_TEXT_LEFT);

    // Add the actors to the scene
    mRenderer->AddActor(zLabel);
    mRenderer->AddActor(hLabel);
    mRenderer->AddActor(vLabel);
    mRenderer->AddActor(uLabel);
    mRenderer->AddViewProp(actor);
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
    actor->GetProperty()->SetLineWidth(2.0);
    actor->GetProperty()->LightingOff();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Draw the epure based on two points with zero intersection
void DiagramReportSceneItem::drawTriEpure(Vector3d const& firstCoords, Vector3d const& secondCoords, double firstValue, double secondValue,
                                          Eigen::Vector3d const& normalVec)
{
    // Compute the directional vectors
    Vector3d e1 = (secondCoords - firstCoords).normalized();
    Vector3d e2 = normalVec;

    // Build the projection functions
    Vector3d a1 = firstCoords;
    Vector3d a2 = secondCoords;
    auto to2D = [&](Vector3d const& p)
    {
        Vector3d v = p - a1;
        return Vector2d(v.dot(e1), v.dot(e2));
    };
    auto to3D = [&](Vector2d const& p2) { return a1 + p2.x() * e1 + p2.y() * e2; };

    // Compute the state vectors
    Vector3d firstState = firstValue * normalVec;
    Vector3d secondState = secondValue * normalVec;
    Vector3d b1 = firstCoords + firstState;
    Vector3d b2 = secondCoords + secondState;

    // Find the intersection
    Vector2d xp;
    if (!Backend::Utility::findLineIntersect(to2D(a1), to2D(a2), to2D(b1), to2D(b2), xp))
    {
        drawQuadEpure(firstCoords, secondCoords, firstValue, secondValue, normalVec);
        return;
    }
    Vector3d x = to3D(xp);

    // Create the points
    vtkNew<vtkPoints> points;
    points->InsertPoint(0, a1[0], a1[1], a1[2]);
    points->InsertPoint(1, a2[0], a2[1], a2[2]);
    points->InsertPoint(2, b1[0], b1[1], b1[2]);
    points->InsertPoint(3, b2[0], b2[1], b2[2]);
    points->InsertPoint(4, x[0], x[1], x[2]);
    points->InsertPoint(5, x[0], x[1], x[2]);

    // Create the scalars
    vtkNew<vtkDoubleArray> scalars;
    scalars->SetNumberOfTuples(6);
    scalars->SetValue(0, firstValue);
    scalars->SetValue(1, secondValue);
    scalars->SetValue(2, firstValue);
    scalars->SetValue(3, secondValue);
    scalars->SetValue(4, firstValue);
    scalars->SetValue(5, secondValue);

    // Create the polygon
    vtkNew<vtkPolygon> firstPoly;
    firstPoly->GetPointIds()->InsertNextId(0);
    firstPoly->GetPointIds()->InsertNextId(2);
    firstPoly->GetPointIds()->InsertNextId(4);
    vtkNew<vtkPolygon> secondPoly;
    secondPoly->GetPointIds()->InsertNextId(5);
    secondPoly->GetPointIds()->InsertNextId(1);
    secondPoly->GetPointIds()->InsertNextId(3);
    vtkNew<vtkCellArray> polygons;
    polygons->InsertNextCell(firstPoly);
    polygons->InsertNextCell(secondPoly);

    // Group the polygons
    vtkNew<vtkPolyData> polyData;
    polyData->SetPoints(points);
    polyData->SetPolys(polygons);
    polyData->GetPointData()->SetScalars(scalars);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    mapper->UseLookupTableScalarRangeOn();
    mapper->SetLookupTable(mLookupTable);

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->LightingOff();

    // Add the actor to the scene
    mRenderer->AddActor(actor);
}

//! Draw the epure based on two points without zero intersection
void DiagramReportSceneItem::drawQuadEpure(Vector3d const& firstCoords, Vector3d const& secondCoords, double firstValue, double secondValue,
                                           Eigen::Vector3d const& normalVec)
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
    mapper->SetLookupTable(mLookupTable);

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->LightingOff();

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

    // Draw the ruler
    if (pItem->showRuler)
        drawRuler();

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
        return Vector3d::UnitZ();
    case ReportView::kRight:
        return -Vector3d::UnitZ();
    case ReportView::kIsometric:
        return -Vector3d::UnitX();
    default:
        break;
    }
    return Vector3d::Zero();
}

//! Helper function to convert world coordinated to the viewport ones
Vector2d worldToViewport(vtkRenderer* renderer, Vector3d const& world)
{
    Vector2d result;

    // Get the transformation matrix
    vtkCamera* cam = renderer->GetActiveCamera();
    vtkMatrix4x4* M = cam->GetCompositeProjectionTransformMatrix(renderer->GetTiledAspectRatio(), -1, 1);

    // Clip to the world space
    double clip[4];
    double p[4] = {world[0], world[1], world[2], 1.0};
    M->MultiplyPoint(p, clip);

    // Convert to normalized view coordinates
    double ndc[3];
    ndc[0] = clip[0] / clip[3];
    ndc[1] = clip[1] / clip[3];
    result[0] = (ndc[0] + 1.0) * 0.5;
    result[1] = (ndc[1] + 1.0) * 0.5;

    return result;
}

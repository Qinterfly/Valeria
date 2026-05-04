#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTemporaryFile>

#include <vtkAxesActor.h>
#include <vtkCamera.h>
#include <vtkCaptionActor2D.h>
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
#include <vtkTextProperty.h>
#include <vtkTransform.h>

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

// Constants
static double const skEps = std::numeric_limits<double>::epsilon();
static vtkColor3d const skTextColor = vtkColors->GetColor3d("Black");

// Helper functions
PairString getKey(std::wstring const& name);

ModeReportSceneItem::ModeReportSceneItem(ModeReportItem* pItem, ReportTextEngine& textEngine, ResponseCollection const& collection,
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

ModeReportSceneItem::~ModeReportSceneItem()
{
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
    int numResponses = bundle.responses.size();
    int iFound = -1;
    for (int i = 0; i != numResponses; ++i)
    {
        // Retrieve the acceleration which has the requested units
        Testlab::Response const& response = bundle.responses[i];
        if (response.header.type != Testlab::ResponseType::kAccel)
            continue;
        Testlab::Response accel = Backend::Utility::convertAcceleration(bundle, response, pItem->unit);
        int numKeys = accel.keys.size();
        if (numKeys == 0)
            continue;

        // Find the closest frequency to the resonance one
        if (iFound < 0 || iFound > numKeys)
            iFound = Backend::Utility::findClosestKey(accel, bundle.freq);
        if (iFound < 0)
            continue;

        // Set the field value
        QString componentName = QString::fromStdWString(accel.header.point.component);
        QString nodeName = QString::fromStdWString(accel.header.point.node);
        Vector3d value = Backend::Utility::projectResponse(accel, mGeometry, iFound).imag();
        PairString key(componentName, nodeName);
        if (!mState.contains(key))
            mState[key] = Vector3d::Zero();
        mState[key] += value;
    }

    // Resolve the depenedencies
    resolveStateSlaves();
}

//! Resolve dependencies between state values
void ModeReportSceneItem::resolveStateSlaves()
{
    int numSlaves = mGeometry.dependencies.size();
    for (int iSlave = 0; iSlave != numSlaves; ++iSlave)
    {
        Testlab::Dependency const& dependency = mGeometry.dependencies[iSlave];

        // Get the slave
        PairString slaveKey = getKey(dependency.slave);
        if (!mState.contains(slaveKey))
            continue;
        Vector3d slaveCoords = Utility::convert3d(Backend::Utility::getNodeCoords(mGeometry, slaveKey.first, slaveKey.second));
        Vector3d slaveValues = mState[slaveKey];

        // Count the valid master nodes
        int numMasters = dependency.masters.size();
        int numValidMasters = 0;
        for (int iMaster = 0; iMaster != numMasters; ++iMaster)
        {
            PairString masterKey = getKey(dependency.masters[iMaster]);
            if (!mState.contains(masterKey))
                continue;
            ++numValidMasters;
        }
        if (numValidMasters == 0)
            continue;

        // Get the data of master nodes
        int numDirs = slaveValues.size();
        MatrixXd masterCoords(numValidMasters, numDirs);
        MatrixXd masterValues(numValidMasters, numDirs);
        for (int iMaster = 0; iMaster != numMasters; ++iMaster)
        {
            PairString masterKey = getKey(dependency.masters[iMaster]);
            if (!mState.contains(masterKey))
                continue;
            masterCoords.row(iMaster) = Utility::convert3d(Backend::Utility::getNodeCoords(mGeometry, masterKey.first, masterKey.second));
            masterValues.row(iMaster) = mState[masterKey];
        }

        // Interpolate the master values
        int numFlags = dependency.flags.size();
        VectorXd interpValues = Backend::Utility::interpolateIDW(slaveCoords, masterCoords, masterValues);
        for (int iFlag = 0; iFlag != numFlags; ++iFlag)
        {
            if (dependency.flags[iFlag] > 0)
                slaveValues[iFlag] = interpValues[iFlag];
        }

        // Store the result
        mState[slaveKey] = slaveValues;
    }
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
    drawGeometry();
    drawTitle();
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
    drawAxes();
    mOverlayRenderer->ResetCamera();

    // Create the window
    mRenderWindow = vtkRenderWindow::New();
    mRenderWindow->OffScreenRenderingOn();
    mRenderWindow->SetNumberOfLayers(2);
    mRenderWindow->AddRenderer(mRenderer);
    mRenderWindow->AddRenderer(mOverlayRenderer);

    // Initialize the font file
    QTemporaryFile* pFile = QTemporaryFile::createNativeFile(":/fonts/Roboto.ttf");
    mPathFontFile = pFile->fileName();
}

//! Set the camera position as well as zoom
void ModeReportSceneItem::setView()
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;

    // Set the camera position
    switch (pItem->view)
    {
    case ReportView::kFront:
        Utility::setPlaneView(mRenderer, 0, 1);
        break;
    case ReportView::kRear:
        Utility::setPlaneView(mRenderer, 0, -1);
        break;
    case ReportView::kTop:
        Utility::setPlaneView(mRenderer, 1, 1);
        break;
    case ReportView::kBottom:
        Utility::setPlaneView(mRenderer, 1, -1);
        break;
    case ReportView::kLeft:
        Utility::setPlaneView(mRenderer, 2, 1);
        break;
    case ReportView::kRight:
        Utility::setPlaneView(mRenderer, 2, -1);
        break;
    case ReportView::kIsometric:
        Utility::setIsometricView(mRenderer);
        break;
    default:
        break;
    }

    // Copy the view to the overlay
    double position[3];
    double viewUp[3];
    mRenderer->GetActiveCamera()->GetPosition(position);
    mRenderer->GetActiveCamera()->GetViewUp(viewUp);
    mOverlayRenderer->GetActiveCamera()->ParallelProjectionOn();
    mOverlayRenderer->GetActiveCamera()->SetPosition(position[0], position[1], position[2]);
    mOverlayRenderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
    mOverlayRenderer->GetActiveCamera()->SetViewUp(viewUp);
    mOverlayRenderer->ResetCamera();

    // Fit the camera view
    mOverlayRenderer->GetActiveCamera()->Zoom(1.5);

    // Set the zoom
    mRenderer->GetActiveCamera()->Zoom(pItem->scale);
}

//! Represent geometry
void ModeReportSceneItem::drawGeometry()
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
    for (int i = 0; i != numComponents; ++i)
    {
        Testlab::Component const& component = mGeometry.components[i];

        // Construct the vertices
        vtkSmartPointer<vtkPoints> points = createPoints(component);

        // Draw the elements
        if (pItem->showLines)
            drawElements(points, component.lines, color, 1.0, true, true);
        if (pItem->showTrias)
            drawElements(points, component.trias, color, 1.0, true, true);
        if (pItem->showQuads)
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
    PairDouble range = getMagnitudeRange();
    if (std::abs(range.second - range.first) < skEps)
        return;

    // Create the lookup table
    double limit = std::max(std::abs(range.first), std::abs(range.second));
    vtkSmartPointer<vtkLookupTable> lookupTable = Utility::createLookupTable(pItem->colorMap, -limit, limit);

    // Set the mode parametsr
    double scale = pItem->amplitude * mMaximumDimension / limit;
    double phase = pItem->phase * M_PI / 180.0;

    // Loop through all the components
    int numComponents = mGeometry.components.size();
    for (int i = 0; i != numComponents; ++i)
    {
        Testlab::Component const& component = mGeometry.components[i];

        // Construct the vertices
        vtkSmartPointer<vtkPoints> points = createPoints(component, scale, phase);

        // Compute the magnitudes
        vtkSmartPointer<vtkDoubleArray> magnitudes = getMagnitudes(component);

        // Draw the vertices
        if (pItem->showVertices)
            drawVertices(points, magnitudes, lookupTable);

        // Draw the elements
        if (pItem->showLines)
            drawElements(points, component.lines, magnitudes, lookupTable);
        if (pItem->showTrias)
            drawElements(points, component.trias, magnitudes, lookupTable);
        if (pItem->showQuads)
            drawElements(points, component.quads, magnitudes, lookupTable);
    }

    // Show the scalar bar
    drawScalarBar(lookupTable);
}

//! Render color interpolated vertices
void ModeReportSceneItem::drawVertices(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkDoubleArray> scalars,
                                       vtkSmartPointer<vtkLookupTable> lookupTable)
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
    mapper->SetLookupTable(lookupTable);

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
                                       vtkSmartPointer<vtkDoubleArray> scalars, vtkSmartPointer<vtkLookupTable> lookupTable, bool isWireframe)
{
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

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
    polyData->GetPointData()->SetScalars(scalars);

    // Build the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(polyData);
    mapper->UseLookupTableScalarRangeOn();
    mapper->SetLookupTable(lookupTable);

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
void ModeReportSceneItem::drawScalarBar(vtkSmartPointer<vtkLookupTable> lookupTable)
{
    // Constants
    double const kRelMaxWidth = 1.0 / 5.0;

    // Get the report item
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Create the scalar bar
    vtkNew<vtkScalarBarActor> scalarBar;

    // Set the title
    QString title = mTextEngine.process(pItem->sLabel);
    vtkNew<vtkTextActor> titleActor;
    titleActor->SetInput(title.toStdString().c_str());
    vtkTextProperty* titleProp = titleActor->GetTextProperty();
    titleProp->SetFontFamily(VTK_FONT_FILE);
    titleProp->SetFontFile(mPathFontFile.toStdString().data());
    titleProp->SetColor(skTextColor.GetData());
    titleProp->SetOrientation(90);
    titleProp->SetFontSize(pItem->font.pointSize());
    titleProp->SetJustificationToCentered();
    titleActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
    titleActor->GetPosition2Coordinate()->SetCoordinateSystemToNormalizedViewport();
    titleActor->SetPosition(0.98, 0.35);
    titleActor->SetPosition2(1.0, 0.55);
    mRenderer->AddActor(titleActor);

    // Set the labels
    scalarBar->SetLabelFormat("%5.2f");
    scalarBar->SetNumberOfLabels(2);
    vtkTextProperty* labelProp = scalarBar->GetLabelTextProperty();
    labelProp->ShadowOff();
    labelProp->BoldOff();
    labelProp->SetColor(skTextColor.GetData());
    labelProp->SetFontSize(pItem->font.pointSize());
    scalarBar->UnconstrainedFontSizeOn();

    // Set the geometry
    int maxWidth = ceil(kRelMaxWidth * mRenderWindow->GetSize()[0]);
    scalarBar->SetLookupTable(lookupTable);
    scalarBar->SetMaximumWidthInPixels(maxWidth);
    scalarBar->SetPosition(0.9, 0.05);
    scalarBar->SetPosition2(0.95, 0.6);

    // Add to the scene
    mRenderer->AddViewProp(scalarBar);
}

//! Render the axes
void ModeReportSceneItem::drawAxes()
{
    mAxes = vtkAxesActor::New();

    // Set the text properties
    vtkTextProperty* xTextProp = mAxes->GetXAxisCaptionActor2D()->GetCaptionTextProperty();
    vtkTextProperty* yTextProp = mAxes->GetYAxisCaptionActor2D()->GetCaptionTextProperty();
    vtkTextProperty* zTextProp = mAxes->GetZAxisCaptionActor2D()->GetCaptionTextProperty();
    xTextProp->SetColor(vtkColors->GetColor3d("Red").GetData());
    yTextProp->SetColor(vtkColors->GetColor3d("Green").GetData());
    zTextProp->SetColor(vtkColors->GetColor3d("Blue").GetData());
    xTextProp->ShadowOff();
    yTextProp->ShadowOff();
    zTextProp->ShadowOff();
    xTextProp->ItalicOff();
    yTextProp->ItalicOff();
    zTextProp->ItalicOff();

    // Add them to the scene
    mOverlayRenderer->AddActor(mAxes);
}

//! Render the title
void ModeReportSceneItem::drawTitle()
{
    // Get the report item
    ModeReportItem* pItem = (ModeReportItem*) mpItem;
    if (!pItem)
        return;

    // Set the text
    QString text = mTextEngine.process(pItem->title);
    vtkNew<vtkTextActor> actor;
    actor->SetInput(text.toStdString().c_str());
    vtkTextProperty* prop = actor->GetTextProperty();
    prop->SetFontFamily(VTK_FONT_FILE);
    prop->SetFontFile(mPathFontFile.toStdString().data());
    prop->SetColor(skTextColor.GetData());
    prop->SetFontSize(pItem->font.pointSize());
    prop->SetJustificationToLeft();
    actor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
    actor->GetPosition2Coordinate()->SetCoordinateSystemToNormalizedViewport();
    actor->SetPosition(0.0, 0.0);
    actor->SetPosition2(0.5, 0.2);
    mRenderer->AddActor(actor);
}

//! Create points which are associated with the geometry
vtkSmartPointer<vtkPoints> ModeReportSceneItem::createPoints(Testlab::Component const& component, double scale, double phase)
{
    vtkNew<vtkPoints> points;
    QString componentName = QString::fromStdWString(component.name);
    int numNodes = component.nodes.size();
    for (int iNode = 0; iNode != numNodes; ++iNode)
    {
        Testlab::Node const& node = component.nodes[iNode];
        QString nodeName = QString::fromStdWString(node.name);

        // Get the nodal position
        Vector3d position = Utility::convert3d(node.coordinates);

        // Apply the values
        Vector3d values = getNodeValues(componentName, nodeName);
        position += values * scale * cos(phase);

        // Add the point
        points->InsertPoint(iNode, position[0], position[1], position[2]);
    }
    return points;
}

//! Create polygons using given indices
vtkSmartPointer<vtkCellArray> ModeReportSceneItem::createPolygons(std::vector<std::vector<int>> const& indices)
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
        Vector3d values = getNodeValues(componentName, nodeName);
        int numValues = values.size();
        double magnitude = 0.0;
        for (int iValue = 0; iValue != numValues; ++iValue)
        {
            if (std::abs(values[iValue]) > std::abs(magnitude))
                magnitude = values[iValue];
        }

        // Set the magnitude
        magnitudes->SetValue(iNode, magnitude);
    }
    return magnitudes;
}

//! Get the range of magnitudes
PairDouble ModeReportSceneItem::getMagnitudeRange()
{
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    int numComponents = mGeometry.components.size();
    for (int iComponent = 0; iComponent != numComponents; ++iComponent)
    {
        Testlab::Component const& component = mGeometry.components[iComponent];
        QString componentName = QString::fromStdWString(component.name);
        int numNodes = component.nodes.size();
        for (int iNode = 0; iNode != numNodes; ++iNode)
        {
            Testlab::Node const& node = component.nodes[iNode];
            QString nodeName = QString::fromStdWString(node.name);
            Vector3d values = getNodeValues(componentName, nodeName);
            min = std::min(min, values.minCoeff());
            max = std::max(max, values.maxCoeff());
        }
    }
    return {min, max};
}

//! Get the vertex field value related to the node
Eigen::Vector3d ModeReportSceneItem::getNodeValues(QString const& componentName, QString const& nodeName)
{
    Vector3d result = Vector3d::Zero();
    PairString key(componentName, nodeName);
    if (mState.contains(key))
        result = mState[key];
    return result;
}

//! Helper functin to get key out of full node name
PairString getKey(std::wstring const& name)
{
    PairString result;
    QString text = QString::fromStdWString(name);
    QStringList tokens = text.split(':');
    if (tokens.size() == 2)
        return {tokens[0], tokens[1]};
    return {QString(), text};
}

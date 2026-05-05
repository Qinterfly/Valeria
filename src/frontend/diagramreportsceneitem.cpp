#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTemporaryFile>

#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>
#include <vtkNamedColors.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>

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
void DiagramReportSceneItem::drawGeometry()
{
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
    if (!pItem)
        return;

    // Estimate the maximum dimension
    mMaximumDimension = Backend::Utility::getMaximumDimension(mGeometry);
    if (mMaximumDimension < skEps)
        mMaximumDimension = 1.0;

    // TODO
}

//! Render the axes
void DiagramReportSceneItem::drawAxes()
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
void DiagramReportSceneItem::drawTitle()
{
    // Get the report item
    DiagramReportItem* pItem = (DiagramReportItem*) mpItem;
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
    drawGeometry();
    drawTitle();
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

#include <QComboBox>
#include <QSettings>
#include <QTableWidgetItem>
#include <QToolBar>

#include <vtkCamera.h>
#include <vtkColor.h>
#include <vtkColorTransferFunction.h>
#include <vtkCubeSource.h>
#include <vtkLookupTable.h>
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>
#include <vtkWindowToImageFilter.h>

#include "customplot.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Backend::Core;
using namespace Eigen;

// Constants
vtkNew<vtkNamedColors> const vtkColors;

namespace Frontend::Utility
{

//! Get a default font
QFont getFont()
{
    QString fontName = "Roboto";
    uint fontSize = 12;
#ifdef Q_OS_WIN
    fontName = "Cambria";
    fontSize = 12;
#endif
    return QFont(fontName, fontSize);
}

//! Get a default monospace font
QFont getMonospaceFont()
{
    QString fontName = "RobotoMono";
    uint fontSize = 12;
#ifdef Q_OS_WIN
    fontName = "monofur";
    fontSize = 14;
#endif
    return QFont(fontName, fontSize);
}

//! Convert a VTK color to Qt one
QColor getColor(vtkColor3d color)
{
    return QColor::fromRgbF(color[0], color[1], color[2]);
}

//! Convert a Qt color to Vtk one
vtkColor3d getColor(QColor color)
{
    return vtkColor3d(color.redF(), color.greenF(), color.blueF());
}

//! Add shortcurt hints to all items contained in a tool bar
void setShortcutHints(QToolBar* pToolBar)
{
    QList<QAction*> actions = pToolBar->actions();
    int numActions = actions.size();
    for (int i = 0; i != numActions; ++i)
    {
        QAction* pAction = actions[i];
        QKeySequence shortcut = pAction->shortcut();
        if (shortcut.isEmpty())
            continue;
        pAction->setToolTip(QString("%1 (%2)").arg(pAction->toolTip(), shortcut.toString()));
    }
}

//! Get circular index
int getRepeatedIndex(int index, int size)
{
    if (index >= size)
        return index % size;
    return index;
}

//! Show save dialog when closing a widget and process its output
int showSaveDialog(QWidget* pWidget, QString const& title, QString const& message)
{
    QMessageBox* pMessageBox = new QMessageBox(QMessageBox::Question, title, message,
                                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    pMessageBox->setFont(pWidget->font());
    auto result = pMessageBox->exec();
    switch (result)
    {
    case QMessageBox::Save:
        return 1;
    case QMessageBox::Discard:
        return 0;
    default:
        return -1;
    }
}

//! Substitute a file suffix to the expected one, if necessary
void modifyFileSuffix(QString& pathFile, QString const& expectedSuffix)
{
    QFileInfo info(pathFile);
    QString currentSuffix = info.suffix();
    if (currentSuffix.isEmpty())
        pathFile.append(QString(".%1").arg(expectedSuffix));
    else if (currentSuffix != expectedSuffix)
        pathFile.replace(currentSuffix, expectedSuffix);
}

//! Retrieve last used directory
QDir getLastDirectory(QSettings const& settings)
{
    return QFileInfo(getLastPathFile(settings)).dir();
}

//! Retrieve last used path file
QString getLastPathFile(QSettings const& settings)
{
    return settings.value(Constants::Settings::skLastPathFile, QString()).toString();
}

//! Set last used path file
void setLastPathFile(QSettings& settings, QString const& pathFile)
{
    settings.setValue(Constants::Settings::skLastPathFile, pathFile);
}

//! Set combobox current index by item key
void setIndexByKey(QComboBox* pComboBox, int key)
{
    int numItems = pComboBox->count();
    pComboBox->setCurrentIndex(-1);
    for (int i = 0; i != numItems; ++i)
    {
        if (pComboBox->itemData(i).toInt() == key)
        {
            pComboBox->setCurrentIndex(i);
            break;
        }
    }
}

//! Create table widget item associated with a double value
QTableWidgetItem* createTableItem(double value, Qt::AlignmentFlag alignment)
{
    QString text = QString::number(value);
    QTableWidgetItem* pItem = new QTableWidgetItem(text);
    pItem->setTextAlignment(alignment);
    return pItem;
};

//! Create table widget item associated with double values
QTableWidgetItem* createTableItem(std::vector<double> const& values, Qt::AlignmentFlag alignment)
{
    QString text;
    QTextStream stream(&text);
    int numValues = values.size();
    for (int i = 0; i != numValues; ++i)
    {
        if (i > 0)
            stream << " ";
        stream << values[i];
    }
    return createTableItem(text, alignment);
}

//! Create table widget item associated with a string value
QTableWidgetItem* createTableItem(QString const& text, Qt::AlignmentFlag alignment)
{
    QTableWidgetItem* pItem = new QTableWidgetItem(text);
    pItem->setTextAlignment(alignment);
    return pItem;
}

//! Show widget as a dialog window
QDialog* showAsDialog(QWidget* pWidget, QString const& title, QWidget* pParent, bool isModal)
{
    QDialog* pDialog = new QDialog(pParent);
    pDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    pDialog->setWindowTitle(title);
    pDialog->setModal(isModal);
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->addWidget(pWidget);
    pDialog->setLayout(pLayout);
    pDialog->show();
    return pDialog;
}

//! Create enum associated lookup table within the specified range
vtkSmartPointer<vtkLookupTable> createLookupTable(ReportColorMap colorMap, double lower, double upper)
{
    vtkSmartPointer<vtkLookupTable> result;
    switch (colorMap)
    {
    case ReportColorMap::kCoolToWarm:
        result = Utility::createCoolToWarmColorMap();
        break;
    case ReportColorMap::kBlueToRed:
        result = Utility::createBlueToRedColorMap();
        break;
    case ReportColorMap::kVaradis:
        result = Utility::createVaradisColorMap();
        break;
    case ReportColorMap::kJet:
        result = Utility::createJetColorMap();
        break;
    case ReportColorMap::kPlasma:
        result = Utility::createPlasmaColorMap();
        break;
    default:
        result = Utility::createCoolToWarmColorMap();
        break;
    }
    result->SetRange(lower, upper);
    result->Build();
    return result;
}

//! Build the lookup table out of the transfer function
vtkSmartPointer<vtkLookupTable> buildLookupTable(vtkSmartPointer<vtkColorTransferFunction> ctf)
{
    // Constants
    int const kTableSize = 256;

    // Create the lookup table
    vtkNew<vtkLookupTable> lut;
    lut->SetNumberOfTableValues(kTableSize);
    lut->Build();

    // Set the table values
    int numColors = lut->GetNumberOfColors();
    for (auto i = 0; i != numColors; ++i)
    {
        std::array<double, 3> rgb;
        ctf->GetColor(static_cast<double>(i) / lut->GetNumberOfColors(), rgb.data());
        std::array<double, 4> rgba{0.0, 0.0, 0.0, 1.0};
        std::copy(std::begin(rgb), std::end(rgb), std::begin(rgba));
        lut->SetTableValue(static_cast<vtkIdType>(i), rgba.data());
    }

    return lut;
}

//! Create the diverging color map from cool to warm colors
vtkSmartPointer<vtkLookupTable> createCoolToWarmColorMap()
{
    vtkNew<vtkColorTransferFunction> ctf;
    ctf->SetColorSpaceToDiverging();
    ctf->AddRGBPoint(0.0, 0.230, 0.299, 0.754);
    ctf->AddRGBPoint(0.5, 0.865, 0.865, 0.865);
    ctf->AddRGBPoint(1.0, 0.706, 0.016, 0.150);
    return buildLookupTable(ctf);
}

//! Create the diverging color map from blue to red colors
vtkSmartPointer<vtkLookupTable> createBlueToRedColorMap()
{
    vtkNew<vtkColorTransferFunction> ctf;
    ctf->SetColorSpaceToRGB();
    ctf->AddRGBPoint(0.0, 0.0, 0.0, 1.0);  // Blue
    ctf->AddRGBPoint(0.25, 0.0, 1.0, 1.0); // Cyan
    ctf->AddRGBPoint(0.5, 0.0, 1.0, 0.0);  // Green
    ctf->AddRGBPoint(0.75, 1.0, 1.0, 0.0); // Yellow
    ctf->AddRGBPoint(1.0, 1.0, 0.0, 0.0);  // Red
    return buildLookupTable(ctf);
}

//! Create the Varadis color map
vtkSmartPointer<vtkLookupTable> createVaradisColorMap()
{
    vtkNew<vtkColorTransferFunction> ctf;
    ctf->SetColorSpaceToRGB();
    ctf->AddRGBPoint(0.0, 0.267, 0.005, 0.329); // Dark purple
    ctf->AddRGBPoint(0.25, 0.229, 0.322, 0.545);
    ctf->AddRGBPoint(0.5, 0.127, 0.566, 0.550);
    ctf->AddRGBPoint(0.75, 0.369, 0.788, 0.382);
    ctf->AddRGBPoint(1.0, 0.993, 0.906, 0.144); // Yellow
    return buildLookupTable(ctf);
}

//! Create the Jet color map
vtkSmartPointer<vtkLookupTable> createJetColorMap()
{
    vtkNew<vtkColorTransferFunction> ctf;
    ctf->SetColorSpaceToRGB();
    ctf->AddRGBPoint(0.0, 0.0, 0.0, 1.0);  // Blue
    ctf->AddRGBPoint(0.33, 0.0, 1.0, 1.0); // Cyan
    ctf->AddRGBPoint(0.66, 1.0, 1.0, 0.0); // Yellow
    ctf->AddRGBPoint(1.0, 1.0, 0.0, 0.0);  // Red
    return buildLookupTable(ctf);
}

//! Create the Plasma color map
vtkSmartPointer<vtkLookupTable> createPlasmaColorMap()
{
    vtkNew<vtkColorTransferFunction> ctf;
    ctf->SetColorSpaceToRGB();
    ctf->AddRGBPoint(0.0, 0.05, 0.03, 0.53); // Dark purple
    ctf->AddRGBPoint(0.25, 0.49, 0.01, 0.66);
    ctf->AddRGBPoint(0.5, 0.76, 0.22, 0.45); // Pink
    ctf->AddRGBPoint(0.75, 0.93, 0.57, 0.14);
    ctf->AddRGBPoint(1.0, 0.99, 0.94, 0.20); // Yellow
    return buildLookupTable(ctf);
}

//! Set the camera position as well as zoom
void setView(ReportView view, double scale, vtkSmartPointer<vtkRenderer> renderer, vtkSmartPointer<vtkRenderer> overlayRenderer)
{
    // Set the camera position
    switch (view)
    {
    case ReportView::kFront:
        Utility::setPlaneView(renderer, 0, 1);
        break;
    case ReportView::kRear:
        Utility::setPlaneView(renderer, 0, -1);
        break;
    case ReportView::kTop:
        Utility::setPlaneView(renderer, 1, 1);
        break;
    case ReportView::kBottom:
        Utility::setPlaneView(renderer, 1, -1);
        break;
    case ReportView::kLeft:
        Utility::setPlaneView(renderer, 2, 1);
        break;
    case ReportView::kRight:
        Utility::setPlaneView(renderer, 2, -1);
        break;
    case ReportView::kIsometric:
        Utility::setIsometricView(renderer);
        break;
    default:
        break;
    }

    // Copy the view to the overlay
    double position[3];
    double viewUp[3];
    renderer->GetActiveCamera()->GetPosition(position);
    renderer->GetActiveCamera()->GetViewUp(viewUp);
    overlayRenderer->GetActiveCamera()->ParallelProjectionOn();
    overlayRenderer->GetActiveCamera()->SetPosition(position[0], position[1], position[2]);
    overlayRenderer->GetActiveCamera()->SetFocalPoint(0, 0, 0);
    overlayRenderer->GetActiveCamera()->SetViewUp(viewUp);
    overlayRenderer->ResetCamera();

    // Fit the camera view
    overlayRenderer->GetActiveCamera()->Zoom(1.5);

    // Set the zoom
    renderer->GetActiveCamera()->Zoom(scale);
}

//! Set the isometric view
void setIsometricView(vtkSmartPointer<vtkRenderer> renderer)
{
    vtkCamera* camera = renderer->GetActiveCamera();
    camera->SetPosition(1, 1, -1);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    renderer->ResetCamera();
}

//! Set view perpendicular to one of the planes
void setPlaneView(vtkSmartPointer<vtkRenderer> renderer, int dir, int sign)
{
    int const kNumDirections = 3;
    vtkSmartPointer<vtkCamera> camera = renderer->GetActiveCamera();
    double position[kNumDirections];
    for (int i = 0; i != kNumDirections; ++i)
        position[i] = 0.0;
    position[dir] = 1.0 * sign;
    camera->SetPosition(position);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    renderer->ResetCamera();
}

//! Render window to an image
QImage getImage(vtkSmartPointer<vtkRenderWindow> renderWindow, double quality)
{
    // Set up the filter
    vtkNew<vtkWindowToImageFilter> filter;
    filter->SetInput(renderWindow);
    filter->SetInputBufferTypeToRGBA();
    filter->SetScale(quality);
    filter->ReadFrontBufferOff();
    filter->Update();

    // Create the image
    vtkImageData* data = filter->GetOutput();
    int* dims = data->GetDimensions();
    unsigned char* ptr = static_cast<unsigned char*>(data->GetScalarPointer());
    return QImage(ptr, dims[0], dims[1], QImage::Format_RGBA8888).mirrored();
}

//! Construct a cube of specified dimension
vtkSmartPointer<vtkActor> createCubeActor(Eigen::Vector3d const& position, double length)
{
    // Construct the source to be rendered at each location
    vtkNew<vtkCubeSource> source;
    source->SetCenter(position[0], position[1], position[2]);
    source->SetXLength(length);
    source->SetYLength(length);
    source->SetZLength(length);

    // Build up the mapper
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(source->GetOutputPort());
    mapper->SetResolveCoincidentTopologyToPolygonOffset();

    // Create the actor
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);

    return actor;
}

//! Get an icon for a legend
QIcon getIcon(QCPScatterStyle const& style, QSize const& size, bool isLine, bool isMarker)
{
    // Construct the pixmap to be drawn at
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    // Create the painter
    QCPPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    style.applyTo(&painter, style.pen());

    // Draw the line style
    if (isLine)
    {
        QLineF line(0, size.height() / 2.0, size.width(), size.height() / 2);
        painter.drawLine(line);
    }

    // Draw the scatter style
    if (isMarker)
    {
        QRectF targetRect(0, 0, size.width(), size.height());
        style.drawShape(&painter, targetRect.center());
    }

    return QIcon(pixmap);
}
}

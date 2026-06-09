
#ifndef UIUTILITY_H
#define UIUTILITY_H

#include <QDir>
#include <QFont>

#include <Eigen/Core>

#include <vtkNew.h>

#include "reportcommon.h"

QT_FORWARD_DECLARE_CLASS(QTableWidgetItem);
QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QToolBar)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)

class QCPScatterStyle;

class vtkColor3d;
class vtkRenderer;
class vtkLookupTable;
class vtkActor;
class vtkAxesActor;
class vtkCellArray;
class vtkTextActor;
class vtkScalarBarActor;
class vtkRenderWindow;
class vtkColorTransferFunction;

namespace Frontend::Utility
{

// Text
QFont getFont();
QFont getMonospaceFont();

// Ui
QColor getColor(vtkColor3d color);
vtkColor3d getColor(QColor color);
void setShortcutHints(QToolBar* pToolBar);
int getRepeatedIndex(int index, int size);
int showSaveDialog(QWidget* pWidget, QString const& title, QString const& message);

// File
void modifyFileSuffix(QString& pathFile, QString const& expectedSuffix);
QDir getLastDirectory(QSettings const& settings);
QString getLastPathFile(QSettings const& settings);
void setLastPathFile(QSettings& settings, QString const& pathFile);

// Widgets
void setIndexByKey(QComboBox* pComboBox, int key);
QTableWidgetItem* createTableItem(double value, Qt::AlignmentFlag alignment = Qt::AlignCenter);
QTableWidgetItem* createTableItem(std::vector<double> const& values, Qt::AlignmentFlag alignment = Qt::AlignCenter);
QTableWidgetItem* createTableItem(QString const& text, Qt::AlignmentFlag alignment = Qt::AlignCenter);
QDialog* showAsDialog(QWidget* pWidget, QString const& title = QString(), QWidget* pParent = nullptr, bool isModal = false);

// Render: color scheme
vtkSmartPointer<vtkLookupTable> createLookupTable(Backend::Core::ReportColorMap colorMap, double lower, double upper);
vtkSmartPointer<vtkLookupTable> buildLookupTable(vtkSmartPointer<vtkColorTransferFunction> ctf);
vtkSmartPointer<vtkLookupTable> createCoolToWarmColorMap();
vtkSmartPointer<vtkLookupTable> createBlueToRedColorMap1();
vtkSmartPointer<vtkLookupTable> createBlueToRedColorMap2();
vtkSmartPointer<vtkLookupTable> createBlueToRedColorMap3();
vtkSmartPointer<vtkLookupTable> createVaradisColorMap();
vtkSmartPointer<vtkLookupTable> createJetColorMap();
vtkSmartPointer<vtkLookupTable> createPlasmaColorMap();

// Render: view
void setView(Backend::Core::ReportView view, double angle, double scale, vtkSmartPointer<vtkRenderer> renderer,
             vtkSmartPointer<vtkRenderer> axesRenderer);
void setIsometricView(vtkSmartPointer<vtkRenderer> renderer, double angle = 0.0);
void setPlaneView(vtkSmartPointer<vtkRenderer> renderer, int dir, int sign, double angle = 0.0);
QImage getImage(vtkSmartPointer<vtkRenderWindow> renderWindow, double quality = 2.0);

// Render: actors
vtkSmartPointer<vtkCellArray> createPolygons(std::vector<std::vector<int>> const& indices);
vtkSmartPointer<vtkActor> createCubeActor(Eigen::Vector3d const& position, double length);
vtkSmartPointer<vtkAxesActor> createAxesActor(int fontSize);
vtkSmartPointer<vtkTextActor> createLabelActor(QString const& text, Eigen::Vector2d const& pos, int fontSize, int justification);
vtkSmartPointer<vtkTextActor> createTitleActor(QString const& text, Eigen::Vector2d const& pos1, Eigen::Vector2d const& pos2, int fontSize);
vtkSmartPointer<vtkTextActor> createScalarBarTitleActor(QString const& title, Eigen::Vector2d const& pos1, Eigen::Vector2d const& pos2,
                                                        int fontSize);
vtkSmartPointer<vtkScalarBarActor> createScalarBarActor(vtkSmartPointer<vtkLookupTable> lookupTable, Eigen::Vector2d const& pos1,
                                                        Eigen::Vector2d const& pos2, int fontSize);

// Icons
QIcon getIcon(QCPScatterStyle const& style, QSize const& size, bool isLine, bool isMarker);
}

#endif // UIUTILITY_H

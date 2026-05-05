#ifndef MODEREPORTSCENEITEM_H
#define MODEREPORTSCENEITEM_H

#include <Eigen/Core>

#include <testlab/common.h>

#include <vtkColor.h>
#include <vtkPolyDataMapper.h>

#include "mathutility.h"
#include "reportinterface.h"
#include "reportsceneitem.h"

class vtkPoints;
class vtkCellArray;
class vtkLookupTable;
class vtkDoubleArray;
class vtkAxesActor;

namespace Backend::Core
{
class ResponseCollection;
class ModeReportItem;
class ReportPoint;
}

namespace Frontend
{

//! Class to render report mode items
class ModeReportSceneItem : public ReportSceneItem
{
    Q_OBJECT

public:
    ModeReportSceneItem(Backend::Core::ModeReportItem* pItem, Backend::Core::ReportTextEngine& textEngine,
                        Backend::Core::ResponseCollection const& collection, int iSelectedBundle, Testlab::Geometry const& geometry,
                        QGraphicsItem* pParent = nullptr);
    virtual ~ModeReportSceneItem();

    void clear();
    void refresh();
    void replot();

protected:
    void paint(QPainter* pPainter, QStyleOptionGraphicsItem const* pOption, QWidget* pWidget) override;

private:
    void initialize();

    // State
    void setState();

    // Rendering
    void setView();
    void drawGeometry();
    void drawUndeformedState();
    void drawDeformedState();
    void drawVertices(vtkSmartPointer<vtkPoints> points, vtkSmartPointer<vtkDoubleArray> scalars, vtkSmartPointer<vtkLookupTable> lookupTable);
    void drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color, double opacity = 1.0,
                      bool isEdgeVisible = true, bool isWireframe = false);
    void drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkSmartPointer<vtkDoubleArray> scalars,
                      vtkSmartPointer<vtkLookupTable> lookupTable, bool isWireframe = false);
    void drawScalarBar(vtkSmartPointer<vtkLookupTable> lookupTable);
    void drawAxes();
    void drawTitle();

    // Helper functions
    vtkSmartPointer<vtkPoints> createPoints(Testlab::Component const& component, double scale = 0.0, double phase = 0.0);
    vtkSmartPointer<vtkCellArray> createPolygons(std::vector<std::vector<int>> const& indices);
    vtkSmartPointer<vtkDoubleArray> getMagnitudes(Testlab::Component const& component);
    PairDouble getMagnitudeRange();
    Eigen::Vector3d getNodeValues(QString const& componentName, QString const& nodeName);

private:
    Backend::Core::ReportTextEngine& mTextEngine;
    Backend::Core::ResponseCollection const& mCollection;
    int const mISelectedBundle;
    Testlab::Geometry const& mGeometry;

    // Data
    Backend::Core::GeometryState mState;
    double mMaximumDimension;

    // VTK
    vtkSmartPointer<vtkRenderWindow> mRenderWindow;
    vtkSmartPointer<vtkRenderer> mRenderer;
    vtkSmartPointer<vtkRenderer> mOverlayRenderer;
    vtkSmartPointer<vtkAxesActor> mAxes;
    QImage mImage;
    QString mPathFontFile;
};

}

#endif // MODEREPORTSCENEITEM_H

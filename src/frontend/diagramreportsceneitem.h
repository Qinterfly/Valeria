#ifndef DIAGRAMREPORTSCENEITEM_H
#define DIAGRAMREPORTSCENEITEM_H

#include <Eigen/Core>

#include <testlab/common.h>

#include <vtkColor.h>
#include <vtkPolyDataMapper.h>

#include "mathutility.h"
#include "reportsceneitem.h"

class vtkPoints;
class vtkCellArray;
class vtkLookupTable;
class vtkDoubleArray;
class vtkAxesActor;

namespace Backend::Core
{
class ResponseCollection;
class DiagramReportItem;
class ReportPoint;
}

namespace Frontend
{

//! Class to render report diagram items
class DiagramReportSceneItem : public ReportSceneItem
{
public:
    DiagramReportSceneItem(Backend::Core::DiagramReportItem* pItem, Backend::Core::ReportTextEngine& textEngine,
                           Backend::Core::ResponseCollection const& collection, int iSelectedBundle, Testlab::Geometry const& geometry,
                           QGraphicsItem* pParent = nullptr);
    virtual ~DiagramReportSceneItem();

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
    void drawAll();
    void drawUndeformedState();
    void drawDeformedState();
    void drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color, bool isWireframe);
    void drawSection(Backend::Core::ReportSection const& section, vtkSmartPointer<vtkLookupTable> lookupTable, double scale, double phase);
    void drawScalarBar(vtkSmartPointer<vtkLookupTable> lookupTable);
    void drawTitle();

    // Helper functions
    vtkSmartPointer<vtkPoints> createPoints(Testlab::Component const& component);

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
};

}

#endif // DIAGRAMREPORTSCENEITEM_H

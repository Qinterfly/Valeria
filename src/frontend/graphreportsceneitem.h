#ifndef GRAPHREPORTSCENEITEM_H
#define GRAPHREPORTSCENEITEM_H

#include "reportsceneitem.h"

QT_FORWARD_DECLARE_CLASS(QBuffer)

namespace Testlab
{
class Geometry;
}

namespace Backend::Core
{
class ReportItem;
class GraphReportItem;
class ResponseCollection;
class ResponseBundle;
class ReportCurve;
}

class QCPAxis;
class QCPCurve;

namespace Frontend
{

class CustomPlot;

//! Class to render graphs
class GraphReportSceneItem : public ReportSceneItem
{
    Q_OBJECT

public:
    friend class ReportGraphEditor;

    GraphReportSceneItem(Backend::Core::GraphReportItem* pItem, Backend::Core::ReportTextEngine& textEngine,
                         Backend::Core::ResponseCollection const& collection, int iSelectedBundle, Testlab::Geometry const& geometry,
                         QGraphicsItem* pParent = nullptr);
    virtual ~GraphReportSceneItem();

    QPair<double, double> yRange();
    void setYRange(double lower, double upper);

protected:
    void paint(QPainter* pPainter, QStyleOptionGraphicsItem const* pOption, QWidget* pWidget) override;

private:
    void setState();
    void processReIm(Backend::Core::ResponseBundle const& bundle);
    void processMultiReIm();
    void processFreqAmp();
    void processModeshape(Backend::Core::ResponseBundle const& bundle);
    QCPCurve* addPlottable(QList<double> const& xData, QList<double> const& yData, Backend::Core::ReportCurve const& curve,
                           QString const& name = QString());
    QPair<QCPAxis*, QCPAxis*> axes();

    // Rendering
    void drawPlot(QPainter* pPainter);
    void drawAsImage(QPainter* pPainter, QSize const& size);
    void renderToSvg(QString const& pathFile, QSize const& size);
    void renderToBuffer(QBuffer& buffer, QSize const& size);

private:
    Backend::Core::ReportTextEngine& mTextEngine;
    Backend::Core::ResponseCollection const& mCollection;
    int const mISelectedBundle;
    Testlab::Geometry const& mGeometry;
    CustomPlot* mpPlot;
};

}

#endif // GRAPHREPORTSCENEITEM_H

#include <testlab/common.h>
#include <QSvgGenerator>
#include <QSvgRenderer>

#include "customplot.h"
#include "graphreportsceneitem.h"
#include "mathutility.h"
#include "reportdefaults.h"
#include "reportitem.h"
#include "reporttextengine.h"
#include "session.h"
#include "uiutility.h"

using namespace Backend::Core;
using namespace Frontend;

// Constants
static double const skEps = std::numeric_limits<double>::epsilon();
static double const skInf = std::numeric_limits<double>::infinity();

// Helper function
PairDouble getValueRange(CustomPlot* pPlot, double keyLower = -skInf, double keyUpper = skInf);

GraphReportSceneItem::GraphReportSceneItem(GraphReportItem* pItem, ReportTextEngine& textEngine, ResponseCollection const& collection,
                                           int iSelectedBundle, Testlab::Geometry const& geometry, QGraphicsItem* pParent)
    : ReportSceneItem(pItem, pParent)
    , mTextEngine(textEngine)
    , mCollection(collection)
    , mISelectedBundle(iSelectedBundle)
    , mGeometry(geometry)
    , mpPlot(new CustomPlot)
{
    setState();
}

GraphReportSceneItem::~GraphReportSceneItem()
{
    delete mpPlot;
}

//! Get Y-axis range
QPair<double, double> GraphReportSceneItem::yRange()
{
    auto [pXAxis, pYAxis] = axes();
    QCPRange range = pYAxis->range();
    return {range.lower, range.upper};
}

//! Set Y-axis range
void GraphReportSceneItem::setYRange(double lower, double upper)
{
    auto [pXAxis, pYAxis] = axes();
    pYAxis->setRangeLower(lower);
    pYAxis->setRangeUpper(upper);
    mpPlot->replot();
    update();
}

//! Set the item state
void GraphReportSceneItem::setState()
{
    // Constants
    QColor const kGridColor(200, 200, 200);
    double const kThreshold = 1e-9;

    // Retrieve the item
    GraphReportItem* pItem = (GraphReportItem*) mpItem;

    // Clear the plot
    mpPlot->clear();

    // Add the plottables
    if (!mCollection.isEmpty())
    {
        ResponseBundle const& bundle = mISelectedBundle >= 0 ? mCollection.get(mISelectedBundle) : mCollection.get(0);
        switch (pItem->subType)
        {
        case GraphReportItem::kReal:
        case GraphReportItem::kImag:
            processReIm(bundle);
            break;
        case GraphReportItem::kMultiReal:
        case GraphReportItem::kMultiImag:
            processMultiReIm();
            break;
        case GraphReportItem::kFreqAmp:
            processFreqAmp();
            break;
        case GraphReportItem::kModeshape:
            processModeshape(bundle);
            break;
        default:
            break;
        }
    }

    // Update the parser
    mTextEngine.setVariable("unit", pItem->unit);
    mTextEngine.setVariable("cdir", Backend::Utility::getDirLabel(pItem->coordDir));
    mTextEngine.setVariable("rdir", Backend::Utility::getDirLabel(pItem->responseDir));

    // Set the legend
    int numPlottables = mpPlot->plottableCount();
    bool isPlottables = numPlottables > 0;
    mpPlot->legend->setVisible(pItem->showLegend && isPlottables);
    mpPlot->legend->setFont(pItem->font);
    mpPlot->setLegendAlignment(pItem->legendAlign);

    // Get the axes
    auto [pXAxis, pYAxis] = axes();

    // Set the axes labels
    pXAxis->setTickLabelFont(pItem->font);
    pYAxis->setTickLabelFont(pItem->font);
    pXAxis->setLabelFont(pItem->font);
    pYAxis->setLabelFont(pItem->font);
    pXAxis->setLabel(mTextEngine.process(pItem->xLabel));
    pYAxis->setLabel(mTextEngine.process(pItem->yLabel));

    // Set the grid options
    QPen gridPen = QPen(kGridColor, pItem->gridWidth, Qt::DotLine);
    QPen gridZeroPen = QPen(kGridColor, pItem->gridZeroWidth, Qt::DotLine);
    pXAxis->grid()->setPen(gridPen);
    pYAxis->grid()->setPen(gridPen);
    pXAxis->grid()->setZeroLinePen(gridZeroPen);
    pYAxis->grid()->setZeroLinePen(gridZeroPen);

    // Set the default axes range
    mpPlot->rescaleAxes();
    if (pItem->subType == GraphReportItem::kModeshape)
    {
        auto [min, max] = getValueRange(mpPlot);
        bool isZero = std::abs(min) < kThreshold && std::abs(max) < kThreshold;
        if (isZero || !isPlottables)
            pYAxis->setRange(-kThreshold, kThreshold);
    }

    // Set the axes range manually
    bool isManualX = std::abs(pItem->xRange.second - pItem->xRange.first) > skEps;
    bool isManualY = std::abs(pItem->yRange.second - pItem->yRange.first) > skEps;
    if (isManualX)
    {
        pXAxis->setRange(pItem->xRange.first, pItem->xRange.second);
        if (!isManualY)
        {
            auto [min, max] = getValueRange(mpPlot, pXAxis->range().lower, pXAxis->range().upper);
            pYAxis->setRange(min, max);
        }
    }
    if (isManualY)
        pYAxis->setRange(pItem->yRange.first, pItem->yRange.second);

    // Scale the axes range
    if (isPlottables)
    {
        pXAxis->scaleRange(pItem->scaleRange);
        pYAxis->scaleRange(pItem->scaleRange);
    }

    // Set the axes ticks
    if (pItem->numTicks > 0)
    {
        pXAxis->ticker()->setTickCount(pItem->numTicks);
        pYAxis->ticker()->setTickCount(pItem->numTicks);
        pXAxis->ticker()->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
        pYAxis->ticker()->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
    }
    else
    {
        pXAxis->ticker()->setTickStepStrategy(QCPAxisTicker::tssReadability);
        pYAxis->ticker()->setTickStepStrategy(QCPAxisTicker::tssReadability);
    }

    // Reverse the axes, if necessary
    pXAxis->setRangeReversed(pItem->reverseX);
    pYAxis->setRangeReversed(pItem->reverseY);

    // Show the auxiliary axes
    mpPlot->axisRect()->setupFullAxesBox(false);

    // Render the plot
    mpPlot->replot();
}

//! Process the item of the real (imag) subtype
void GraphReportSceneItem::processReIm(ResponseBundle const& bundle)
{
    GraphReportItem* pItem = (GraphReportItem*) mpItem;

    // Loop through all the curves
    int numCurves = pItem->curves.size();
    for (int iCurve = 0; iCurve != numCurves; ++iCurve)
    {
        ReportCurve const& curve = pItem->curves[iCurve];

        // Loop through all the points belonged to the curve
        int numPoints = curve.points.size();
        for (int iPoint = 0; iPoint != numPoints; ++iPoint)
        {
            // Get the response which has the requested unit and direction
            ReportPoint const& point = curve.points[iPoint];
            Testlab::Response response = Backend::Utility::getAcceleration(bundle, point, pItem->responseDir, pItem->unit);
            if (response.keys.size() == 0)
            {
                qWarning() << tr("Could not find the response for point %1 which has %2 units").arg(point.name(), pItem->unit);
                continue;
            }

            // Set the data
            QList<double> xData = Backend::Utility::convert(response.keys);
            QList<double> yData = pItem->subType == GraphReportItem::kReal ? Backend::Utility::convert(response.realValues)
                                                                           : Backend::Utility::convert(response.imagValues);
            if (xData.isEmpty() || xData.size() != yData.size())
                continue;
            if (pItem->swapAxes)
                std::swap(xData, yData);

            // Add the plottable
            QString name = tr("p. %1").arg(point.node);
            addPlottable(xData, yData, curve, name);
        }
    }

    // Display the bundle frequency
    if (pItem->showBundleFreq)
    {
        QCPItemStraightLine* pLine = new QCPItemStraightLine(mpPlot);
        pLine->setPen(QPen(Qt::black, 1.0, Qt::DashLine));
        pLine->point1->setCoords(bundle.freq, 0);
        pLine->point2->setCoords(bundle.freq, 1);
    }
}

//! Process the item of the multi real (imag) subtype
void GraphReportSceneItem::processMultiReIm()
{
    QList<ReportCurve> const kDefaultCurves = ReportDefaults::curves();

    GraphReportItem* pItem = (GraphReportItem*) mpItem;

    // Process only the first curve
    if (pItem->curves.isEmpty())
        return;
    ReportCurve const& baseCurve = pItem->curves.first();

    // Process only the first curve point
    if (baseCurve.points.isEmpty())
        return;

    // Set the variable
    ReportPoint const& point = baseCurve.points.first();
    mTextEngine.setVariable("point", point.name());
    mTextEngine.setVariable("component", point.component);
    mTextEngine.setVariable("node", point.node);

    // Loop through all the bundles
    int numBundles = mCollection.count();
    for (int iBundle = 0; iBundle != numBundles; ++iBundle)
    {
        ResponseBundle const& bundle = mCollection.get(iBundle);

        // Get the response which has the requested unit and direction
        Testlab::Response response = Backend::Utility::getAcceleration(bundle, point, pItem->responseDir, pItem->unit);
        if (response.keys.size() == 0)
        {
            qWarning() << tr("Could not find the response for point %1 which has %2 units").arg(point.name(), pItem->unit);
            continue;
        }

        // Set the data
        QList<double> xData = Backend::Utility::convert(response.keys);
        QList<double> yData = pItem->subType == GraphReportItem::kMultiReal ? Backend::Utility::convert(response.realValues)
                                                                            : Backend::Utility::convert(response.imagValues);
        if (xData.isEmpty() || xData.size() != yData.size())
            continue;
        if (pItem->swapAxes)
            std::swap(xData, yData);

        // Set the curve for plotting
        int iDefaultCurve = Utility::getRepeatedIndex(iBundle, kDefaultCurves.size());
        ReportCurve currentCurve = kDefaultCurves[iDefaultCurve];
        currentCurve.points = {point};
        currentCurve.lineStyle = baseCurve.lineStyle;
        currentCurve.lineWidth = baseCurve.lineWidth;
        currentCurve.markerSize = baseCurve.markerSize;
        currentCurve.markerSkip = baseCurve.markerSkip;

        // Add the plottable
        QString name = tr("F = %1 N").arg(QString::number(bundle.force));
        addPlottable(xData, yData, currentCurve, name);
    }
}

//! Process the item of the freq amplitude subtype
void GraphReportSceneItem::processFreqAmp()
{
    GraphReportItem* pItem = (GraphReportItem*) mpItem;

    // Loop through all the curves
    int numCurves = pItem->curves.size();
    int numBundles = mCollection.count();
    for (int iCurve = 0; iCurve != numCurves; ++iCurve)
    {
        ReportCurve const& curve = pItem->curves[iCurve];

        // Process only the first point
        if (curve.isEmpty())
            continue;
        ReportPoint const& point = curve.points.first();

        // Loop through all the bundles
        QList<double> xData(numBundles, 0.0);
        QList<double> yData(numBundles, 0.0);
        for (int iBundle = 0; iBundle != numBundles; ++iBundle)
        {
            ResponseBundle const& bundle = mCollection.get(iBundle);
            if (bundle.freq < skEps)
            {
                qWarning() << tr("The bundle frequncy is not specified for %1. Could not process the Freq-Amp graph").arg(bundle.name);
                continue;
            }

            // Get the response which has the requested unit and direction
            Testlab::Response response = Backend::Utility::getAcceleration(bundle, point, pItem->responseDir, pItem->unit);
            if (response.keys.size() == 0)
            {
                qWarning() << tr("Could not find the response for point %1 which has %2 units").arg(point.name(), pItem->unit);
                continue;
            }

            // Find all the roots
            QList<double> xReal = Backend::Utility::convert(response.keys);
            QList<double> yReal = Backend::Utility::convert(response.realValues);
            auto roots = Backend::Utility::findRoots(xReal, yReal);
            if (roots.empty())
            {
                qWarning() << tr("Could not find any roots for the Freq-Amp graph");
                return;
            }

            // Get the closest root
            double freq = bundle.freq;
            double minDist = skInf;
            for (auto const& root : roots)
            {
                double dist = std::abs(root.key - bundle.freq);
                if (dist < minDist)
                {
                    freq = root.key;
                    minDist = dist;
                }
            }

            // Find the closest frequency to the resonance one
            int iFound = Backend::Utility::findClosestKey(response, freq);
            if (iFound < 0)
                continue;

            // Store the resonance frequency and response value
            double re = response.realValues[iFound];
            double im = response.imagValues[iFound];
            xData[iBundle] = response.keys[iFound];
            yData[iBundle] = std::sqrt(std::pow(re, 2.0) + std::pow(im, 2.0));

            // Add the variable
            QString varName = QString("%1:f").arg(point.name());
            double varValue = freq;
            mTextEngine.setVariable(varName, varValue);
        }

        // Add the curve
        if (pItem->swapAxes)
            std::swap(xData, yData);
        QString name = tr("p. %1").arg(point.node);
        addPlottable(xData, yData, curve, name);
    }
}

//! Process the item of the modeshape subtype
void GraphReportSceneItem::processModeshape(ResponseBundle const& bundle)
{
    GraphReportItem* pItem = (GraphReportItem*) mpItem;

    // Check if the resonance frequency specified for the bundle
    if (bundle.freq < skEps)
    {
        qWarning() << tr("Resonance frequency for bundle %1 is zero").arg(bundle.name);
        return;
    }

    // Check if the coordinates direction is specified
    if (pItem->coordDir == ReportDirection::kNone)
    {
        qWarning() << tr("Coordinate direction is not specified for a modeshape");
        return;
    }
    int iCoordDir = (int) pItem->coordDir - 1;

    // Check if the response direction is specified
    if (pItem->responseDir == ReportDirection::kNone)
    {
        qWarning() << tr("Response direction is not specified for a modeshape");
        return;
    }
    int iRespDir = (int) pItem->responseDir - 1;

    // Loop through all the curves
    int numCurves = pItem->curves.size();
    for (int iCurve = 0; iCurve != numCurves; ++iCurve)
    {
        ReportCurve const& curve = pItem->curves[iCurve];
        if (curve.isEmpty())
            continue;
        int numPoints = curve.points.size();

        // Loop through all the points
        QList<double> xData(numPoints, 0.0);
        QList<double> yData(numPoints, 0.0);
        for (int iPoint = 0; iPoint != numPoints; ++iPoint)
        {
            ReportPoint const& point = curve.points[iPoint];

            // Get the response which has the requested unit and direction
            Testlab::Response response = Backend::Utility::getAcceleration(bundle, point, pItem->responseDir, pItem->unit);
            if (response.keys.size() == 0)
            {
                qWarning() << tr("Could not find the response for point %1 which has %2 units").arg(point.name(), pItem->unit);
                continue;
            }

            // Get the point coordinates
            std::vector<double> coords = Backend::Utility::getNodeCoords(mGeometry, point.component, point.node);
            if (coords.empty())
                continue;

            // Find the closest frequency to the resonance one
            int iFound = Backend::Utility::findClosestKey(response, bundle.freq);
            if (iFound < 0)
                continue;

            // Project the response
            Eigen::Vector3cd projResponse = Backend::Utility::projectResponse(response, mGeometry, iFound);

            // Set the data
            xData[iPoint] = coords[iCoordDir];
            yData[iPoint] = projResponse[iRespDir].imag();

            // Add the variable
            QString varName = QString("%1:%2").arg(point.name(), Backend::Utility::getDirLabel(pItem->responseDir));
            QString varValue = QString::number(yData[iPoint], 'f', 3).replace('.', ',');
            mTextEngine.setVariable(varName, varValue);
        }

        // Add the curve
        if (pItem->swapAxes)
            std::swap(xData, yData);
        addPlottable(xData, yData, curve, curve.name);
    }
}

//! Add the plottable to the plot
void GraphReportSceneItem::addPlottable(QList<double> const& xData, QList<double> const& yData, ReportCurve const& curve, QString const& name)
{
    // Define the style
    QPen pen(curve.lineColor, curve.lineWidth, curve.lineStyle);
    QCPScatterStyle scatterStyle((QCPScatterStyle::ScatterShape) curve.markerShape, curve.markerSize);
    if (curve.markerFill)
        scatterStyle.setBrush(curve.lineColor);
    QString label = name.isEmpty() ? curve.name : name;

    // Modify the style, so that the markers are visible when the curve is not
    auto lineStyle = QCPCurve::lsLine;
    if (pen.style() == Qt::NoPen)
    {
        lineStyle = QCPCurve::lsNone;
        pen.setStyle(Qt::SolidLine);
    }

    // Create the plottable
    QCPCurve* pPlottable = new QCPCurve(mpPlot->xAxis, mpPlot->yAxis);
    pPlottable->setData(xData, yData);
    pPlottable->setLineStyle(lineStyle);
    pPlottable->setPen(pen);
    pPlottable->setName(label);
    pPlottable->setScatterStyle(scatterStyle);
    pPlottable->setScatterSkip(curve.markerSkip);
}

//! Retrieve the axes, taking into account the swap flag
QPair<QCPAxis*, QCPAxis*> GraphReportSceneItem::axes()
{
    GraphReportItem* pItem = (GraphReportItem*) mpItem;
    QCPAxis* pXAxis = mpPlot->xAxis;
    QCPAxis* pYAxis = mpPlot->yAxis;
    if (pItem->swapAxes)
        std::swap(pXAxis, pYAxis);
    return {pXAxis, pYAxis};
}

//! Process paint event
void GraphReportSceneItem::paint(QPainter* pPainter, QStyleOptionGraphicsItem const* pOption, QWidget* pWidget)
{
    drawPlot(pPainter);
    ReportSceneItem::paint(pPainter, pOption, pWidget);
}

//! Draw the plot to the vector picture
void GraphReportSceneItem::drawPlot(QPainter* pPainter)
{
    // Constants
    qreal const kInchToMM = 25.4;

    // Get the dimensions
    double dpi = pPainter->device()->logicalDpiX();
    auto mmToPx = [dpi, kInchToMM](double mm) { return mm * dpi / kInchToMM; };
    QSize pxSize(mmToPx(mpItem->rect.width()), mmToPx(mpItem->rect.height()));

    // Create the picture
    QPicture pic;
    QCPPainter qcpPainter;
    qcpPainter.begin(&pic);
    mpPlot->toPainter(&qcpPainter, pxSize.width(), pxSize.height());
    qcpPainter.end();

    // Compute the transformation parameters
    QSize finalSize = pic.boundingRect().size();
    finalSize.scale(QSize(mpItem->rect.width(), mpItem->rect.height()), Qt::KeepAspectRatio);
    double scaleFactor = finalSize.width() / (double) pic.boundingRect().size().width();
    QPoint pos(mmToPx(mpItem->rect.x()), mmToPx(mpItem->rect.y()));
    pos -= QPoint(mmToPx(mpItem->rect.center().x()), mmToPx(mpItem->rect.center().y()));

    // Render the picture
    pPainter->save();
    pPainter->translate(mpItem->rect.center());
    pPainter->rotate(mpItem->angle);
    pPainter->scale(scaleFactor, scaleFactor);
    pPainter->drawPicture(pos, pic);
    pPainter->restore();
}

//! Draw the plot as the svg image
void GraphReportSceneItem::drawAsImage(QPainter* pPainter, QSize const& size)
{
    // Create the buffer
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);

    // Render to the buffer
    renderToBuffer(buffer, size);
    QSvgRenderer renderer(byteArray);

    // Copy the result to the painter
    pPainter->save();
    renderer.render(pPainter, mpItem->rect);
    pPainter->restore();
}

//! Render the plot to a svg file
void GraphReportSceneItem::renderToSvg(QString const& pathFile, QSize const& size)
{
    // Set up the generator
    QSvgGenerator generator;
    generator.setFileName(pathFile);
    generator.setSize(QSize(size.width(), size.height()));
    generator.setViewBox(QRect(0, 0, size.width(), size.height()));

    // Draw the content
    QCPPainter painter;
    painter.begin(&generator);
    mpPlot->toPainter(&painter, size.width(), size.height());
    painter.end();
}

//! Render the  plot to a buffer
void GraphReportSceneItem::renderToBuffer(QBuffer& buffer, QSize const& size)
{
    // Set up the generator
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(QSize(size.width(), size.height()));
    generator.setViewBox(QRect(0, 0, size.width(), size.height()));

    // Draw the content
    QCPPainter painter;
    painter.begin(&generator);
    mpPlot->toPainter(&painter, size.width(), size.height());
    painter.end();
}

//! Helper function to get value range on the specified range of keys
PairDouble getValueRange(CustomPlot* pPlot, double keyLower, double keyUpper)
{
    double min = skInf;
    double max = -skInf;
    int numPlottables = pPlot->plottableCount();
    for (int iPlottable = 0; iPlottable != numPlottables; ++iPlottable)
    {
        QCPCurve* pCurve = qobject_cast<QCPCurve*>(pPlot->plottable(iPlottable));
        if (!pCurve)
            continue;
        int numData = pCurve->dataCount();
        for (int iData = 0; iData != numData; ++iData)
        {
            double key = pCurve->dataMainKey(iData);
            double value = pCurve->dataMainValue(iData);
            if (key >= keyLower && key <= keyUpper)
            {
                min = std::min(min, value);
                max = std::max(max, value);
            }
        }
    }
    return {min, max};
}

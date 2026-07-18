#include <QObject>

#include <testlab/common.h>

#include "reportdefaults.h"
#include "reportitem.h"

#include "constants.h"
#include "reportdocument.h"

using namespace Backend::Constants;
using namespace Backend::Core;

//! Create curves
QList<ReportCurve> ReportDefaults::curves()
{
    QList<ReportCurve> result;
    result.emplaceBack("red", ReportMarkerShape::kCircle, true);
    result.emplaceBack("green", ReportMarkerShape::kCircle, false);
    result.emplaceBack("blue", ReportMarkerShape::kSquare, true);
    result.emplaceBack("orange", ReportMarkerShape::kSquare, false);
    result.emplaceBack("cyan", ReportMarkerShape::kTriangle, true);
    result.emplaceBack("magenta", ReportMarkerShape::kTriangle, false);
    result.emplaceBack("gray", ReportMarkerShape::kTriangleInverted, true);
    result.emplaceBack("purple", ReportMarkerShape::kTriangleInverted, false);
    result.emplaceBack("brown", ReportMarkerShape::kCross);
    result.emplaceBack("chocolate", ReportMarkerShape::kPlus);
    result.emplaceBack("olive", ReportMarkerShape::kStar);
    result.emplaceBack("steelblue", ReportMarkerShape::kCrossSquare);
    result.emplaceBack("firebrick", ReportMarkerShape::kPlusSquare);
    return result;
}

//! Create a text engine
ReportTextEngine ReportDefaults::textEngine()
{
    ReportTextEngine result;
    result.setVariable("mode", QObject::tr("Mode"));
    result.setVariable("excite", QObject::tr("Excitation"));
    return result;
}

//! Create a document with page preset
ReportDocument ReportDefaults::document()
{
    ReportDocument result;
    result.textEngine = ReportDefaults::textEngine();
    result.add(ReportDefaults::imRePage());
    result.add(ReportDefaults::multiImRePage());
    result.add(ReportDefaults::freqAmpPage());
    result.add(ReportDefaults::projModeYPage());
    result.add(ReportDefaults::mode3DPage());
    result.add(ReportDefaults::diagramPage());
    return result;
}

//! Create a page with imaginary and real parts of a spectrum
ReportPage ReportDefaults::imRePage()
{
    ReportPage page(QObject::tr("Im-Re"));

    // Create an imaginary graph
    GraphReportItem* pImag = new GraphReportItem;
    pImag->name = QObject::tr("Imaginary");
    pImag->rect = QRect(30, 35, 155, 110);
    pImag->subType = GraphReportItem::kImag;
    pImag->responseDir = ReportDirection::kY;
    pImag->unit = Units::skM_S2;
    pImag->xLabel = QObject::tr("f, Hz");
    pImag->yLabel = QObject::tr("Im a, ${UNIT}");
    pImag->showBundleFreq = true;

    // Create a real graph
    GraphReportItem* pReal = new GraphReportItem;
    pReal->name = QObject::tr("Real");
    pReal->rect = QRect(30, 150, 155, 110);
    pReal->subType = GraphReportItem::kReal;
    pReal->responseDir = ReportDirection::kY;
    pReal->unit = Units::skM_S2;
    pReal->link = pImag->id;
    pReal->xLabel = QObject::tr("f, Hz");
    pReal->yLabel = QObject::tr("Re a, ${UNIT}");
    pReal->showBundleFreq = true;

    // Create title
    TextReportItem* pTitle = new TextReportItem;
    pTitle->name = QObject::tr("Title");
    pTitle->rect = QRect(30, 10, 155, 20);
    pTitle->text = QObject::tr("${MODE}, fr = ${FREQ} Hz.\n${EXCITE}.");

    // Create the caption
    TextReportItem* pCaption = new TextReportItem;
    pCaption->name = QObject::tr("Caption");
    pCaption->rect = QRect(30, 265, 155, 10);
    pCaption->text = QObject::tr("Figure X.YY");

    // Combine
    page.add(pImag);
    page.add(pReal);
    page.add(pTitle);
    page.add(pCaption);

    return page;
}

//! Create a page with multiple imaginary and real parts of a spectrum
ReportPage ReportDefaults::multiImRePage()
{
    ReportPage page(QObject::tr("Multi Im-Re"));

    // Create an imaginary graph
    GraphReportItem* pImag = new GraphReportItem;
    pImag->name = QObject::tr("Imaginary");
    pImag->rect = QRect(30, 35, 155, 110);
    pImag->subType = GraphReportItem::kMultiImag;
    pImag->responseDir = ReportDirection::kY;
    pImag->unit = Units::skM_S2;
    pImag->xLabel = QObject::tr("f, Hz");
    pImag->yLabel = QObject::tr("Im a, ${UNIT}");

    // Create a real graph
    GraphReportItem* pReal = new GraphReportItem;
    pReal->name = QObject::tr("Real");
    pReal->rect = QRect(30, 150, 155, 110);
    pReal->subType = GraphReportItem::kMultiReal;
    pReal->responseDir = ReportDirection::kY;
    pReal->unit = Units::skM_S2;
    pReal->link = pImag->id;
    pReal->xLabel = QObject::tr("f, Hz");
    pReal->yLabel = QObject::tr("Re a, ${UNIT}");

    // Create title
    TextReportItem* pTitle = new TextReportItem;
    pTitle->name = QObject::tr("Title");
    pTitle->rect = QRect(30, 10, 155, 20);
    pTitle->text = QObject::tr("${MODE}.\n${EXCITE}.\nPoint ${NODE}.");

    // Create the caption
    TextReportItem* pCaption = new TextReportItem;
    pCaption->name = QObject::tr("Caption");
    pCaption->rect = QRect(30, 265, 155, 10);
    pCaption->text = QObject::tr("Figure X.YY");

    // Combine
    page.add(pImag);
    page.add(pReal);
    page.add(pTitle);
    page.add(pCaption);

    return page;
}

//! Create a page with freq imaginary and real parts of a spectrum
ReportPage ReportDefaults::freqAmpPage()
{
    ReportPage page(QObject::tr("Freq Amp"));

    // Create an imaginary graph
    GraphReportItem* pAmp = new GraphReportItem;
    pAmp->name = QObject::tr("Amplitude");
    pAmp->rect = QRect(30, 35, 155, 110);
    pAmp->subType = GraphReportItem::kFreqAmp;
    pAmp->responseDir = ReportDirection::kY;
    pAmp->unit = Units::skM;
    pAmp->xLabel = QObject::tr("f, Hz");
    pAmp->yLabel = QObject::tr("A, ${UNIT}");
    pAmp->swapAxes = true;

    // Create a picture
    PictureReportItem* pPic = new PictureReportItem;
    pPic->name = QObject::tr("Picture");
    pPic->rect = QRect(30, 150, 155, 110);

    // Create title
    TextReportItem* pTitle = new TextReportItem;
    pTitle->name = QObject::tr("Title");
    pTitle->rect = QRect(30, 10, 155, 20);
    pTitle->text = QObject::tr("${MODE}.\n${EXCITE}.");

    // Create the caption
    TextReportItem* pCaption = new TextReportItem;
    pCaption->name = QObject::tr("Caption");
    pCaption->rect = QRect(30, 265, 155, 10);
    pCaption->text = QObject::tr("Figure X.YY");

    // Combine
    page.add(pAmp);
    page.add(pPic);
    page.add(pTitle);
    page.add(pCaption);

    return page;
}

//! Create a page with a modeshape projection along Y axis
ReportPage ReportDefaults::projModeYPage()
{
    ReportPage page(QObject::tr("Modeshape Y"));
    page.layout.setOrientation(QPageLayout::Landscape);

    // Create an fuselage graph
    GraphReportItem* pFus = new GraphReportItem;
    pFus->name = QObject::tr("Fuselage");
    pFus->rect = QRect(22, 37, 170, 50);
    pFus->subType = GraphReportItem::kModeshape;
    pFus->coordDir = ReportDirection::kX;
    pFus->responseDir = ReportDirection::kY;
    pFus->unit = Units::skM_S2;
    pFus->xLabel = QObject::tr("${CDIR}, m");
    pFus->yLabel = QObject::tr("${RDIR}; Im a, ${UNIT}");
    pFus->reverseX = true;
    pFus->showLegend = false;

    // Create a wing graph
    GraphReportItem* pWing = new GraphReportItem;
    pWing->name = QObject::tr("Wing");
    pWing->rect = QRect(22, 86, 170, 50);
    pWing->subType = GraphReportItem::kModeshape;
    pWing->coordDir = ReportDirection::kZ;
    pWing->responseDir = ReportDirection::kY;
    pWing->unit = Units::skM_S2;
    pWing->xLabel = QObject::tr("${CDIR}, m");
    pWing->yLabel = QObject::tr("${RDIR}; Im a, ${UNIT}");
    pWing->showLegend = false;

    // Create a horizontal stabilizer graph
    GraphReportItem* pHStab = new GraphReportItem;
    pHStab->name = QObject::tr("Hor. stab.");
    pHStab->rect = QRect(46, 138, 120, 50);
    pHStab->subType = GraphReportItem::kModeshape;
    pHStab->coordDir = ReportDirection::kZ;
    pHStab->responseDir = ReportDirection::kY;
    pHStab->unit = Units::skM_S2;
    pHStab->xLabel = QObject::tr("${CDIR}, m");
    pHStab->yLabel = QObject::tr("${RDIR}; Im a, ${UNIT}");
    pHStab->showLegend = false;

    // Create a vertical stabilizer graph
    GraphReportItem* pVStab = new GraphReportItem;
    pVStab->name = QObject::tr("Vert. stab.");
    pVStab->rect = QRect(197, 85, 70, 103);
    pVStab->subType = GraphReportItem::kModeshape;
    pVStab->coordDir = ReportDirection::kY;
    pVStab->responseDir = ReportDirection::kZ;
    pVStab->unit = Units::skM_S2;
    pVStab->xLabel = QObject::tr("${CDIR}, m");
    pVStab->yLabel = QObject::tr("${RDIR}; Im a, ${UNIT}");
    pVStab->swapAxes = true;
    pVStab->showLegend = false;

    // Create table
    TableReportItem* pTable = new TableReportItem;
    pTable->name = QObject::tr("Table");
    pTable->rect = QRect(197, 40, 72, 45);
    pTable->resize(3, 2);
    pTable->midLabel = "Y; Im a,\n${UNIT}";
    pTable->horLabels = {QObject::tr("Rod"), QObject::tr("Case")};
    pTable->verLabels = {QObject::tr("FG"), QObject::tr("LG"), QObject::tr("RG")};

    // Create title
    TextReportItem* pTitle = new TextReportItem;
    pTitle->name = QObject::tr("Title");
    pTitle->rect = QRect(20, 22, 257, 20);
    pTitle->text = QObject::tr("${MODE}, fr = ${FREQ} Hz.\n${EXCITE}.");

    // Create the caption
    TextReportItem* pCaption = new TextReportItem;
    pCaption->name = QObject::tr("Caption");
    pCaption->rect = QRect(20, 185, 255, 10);
    pCaption->text = QObject::tr("Figure X.YY");

    // Combine
    page.add(pFus);
    page.add(pWing);
    page.add(pHStab);
    page.add(pVStab);
    page.add(pTable);
    page.add(pTitle);
    page.add(pCaption);

    return page;
}

//! Create a page with 3D modeshapes
ReportPage ReportDefaults::mode3DPage()
{
    ReportPage page(QObject::tr("Modeshape 3D"));

    // Create isometric mode
    ModeReportItem* pIso = new ModeReportItem;
    pIso->name = QObject::tr("Isometric");
    pIso->rect = QRect(30, 25, 155, 80);
    pIso->unit = Units::skM_S2;
    pIso->view = ReportView::kIsometric;
    pIso->title = QObject::tr("${BUNDLE}");
    pIso->sLabel = QObject::tr("Im a, ${UNIT}");

    // Create front mode
    ModeReportItem* pFront = new ModeReportItem;
    pFront->name = QObject::tr("Front");
    pFront->rect = QRect(30, 105, 155, 80);
    pFront->unit = Units::skM_S2;
    pFront->view = ReportView::kFront;
    pFront->link = pIso->id;
    pFront->sLabel = QObject::tr("Im a, ${UNIT}");

    // Create top mode
    ModeReportItem* pTop = new ModeReportItem;
    pTop->name = QObject::tr("Top");
    pTop->rect = QRect(30, 185, 155, 80);
    pTop->unit = Units::skM_S2;
    pTop->view = ReportView::kTop;
    pTop->link = pIso->id;
    pTop->sLabel = QObject::tr("Im a, ${UNIT}");

    // Create title
    TextReportItem* pTitle = new TextReportItem;
    pTitle->name = QObject::tr("Title");
    pTitle->rect = QRect(30, 10, 155, 20);
    pTitle->text = QObject::tr("${MODE}, fr = ${FREQ} Hz.\n${EXCITE}.");

    // Create the caption
    TextReportItem* pCaption = new TextReportItem;
    pCaption->name = QObject::tr("Caption");
    pCaption->rect = QRect(30, 265, 155, 10);
    pCaption->text = QObject::tr("Figure X.YY");

    // Combine
    page.add(pIso);
    page.add(pFront);
    page.add(pTop);
    page.add(pTitle);
    page.add(pCaption);

    return page;
}

//! Create a page with a diagram
ReportPage ReportDefaults::diagramPage()
{
    ReportPage page(QObject::tr("Diagram"));
    page.layout.setOrientation(QPageLayout::Landscape);

    // Create the diagram
    DiagramReportItem* pDiag = new DiagramReportItem;
    pDiag->name = QObject::tr("Diagram");
    pDiag->rect = QRect(30, 40, 240, 140);
    pDiag->unit = Units::skM_S2;
    pDiag->view = ReportView::kIsometric;
    pDiag->title = QObject::tr("${BUNDLE}");
    pDiag->sLabel = QObject::tr("${RDIR}; Im a, ${UNIT}");

    // Create title
    TextReportItem* pTitle = new TextReportItem;
    pTitle->name = QObject::tr("Title");
    pTitle->rect = QRect(20, 22, 257, 20);
    pTitle->text = QObject::tr("${MODE}, fr = ${FREQ} Hz.\n${EXCITE}.");

    // Create the caption
    TextReportItem* pCaption = new TextReportItem;
    pCaption->name = QObject::tr("Caption");
    pCaption->rect = QRect(20, 185, 255, 10);
    pCaption->text = QObject::tr("Figure X.YY");

    // Combine
    page.add(pDiag);
    page.add(pTitle);
    page.add(pCaption);

    return page;
}

//! Create a default response
Testlab::Response ReportDefaults::response()
{
    Testlab::Response response;
    Testlab::ResponseHeader& header = response.header;
    Testlab::ResponsePoint& point = header.point;

    // General info
    header.type = Testlab::ResponseType::kAccel;
    header.name = QObject::tr("Sine").toStdWString();

    // Point
    point.name = L"F:1:+Y";
    point.component = L"F";
    point.node = L"1";
    point.direction = Testlab::Direction::kY;
    point.sign = 1;

    // Data: sine wave
    response.keys = {0, 0.6981, 1.3963, 2.0944, 2.7925, 3.4907, 4.1888, 4.8869, 5.5851, 6.2832};
    response.realValues = {0, 0.6428, 0.9848, 0.8660, 0.3420, -0.3420, -0.8660, -0.9848, -0.6428, -0.0000};
    response.imagValues = {1.0000, 0.7660, 0.1736, -0.5000, -0.9397, -0.9397, -0.5000, 0.1736, 0.7660, 1.0000};

    return response;
}

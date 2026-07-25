#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>

#include "customlineedit.h"
#include "customplot.h"
#include "customtabwidget.h"
#include "geometryview.h"
#include "mathutility.h"
#include "reportdefaults.h"
#include "sessioneditor.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Frontend;
using namespace Backend::Core;

SessionEditor::SessionEditor(QSettings& settings, QWidget* pParent)
    : QWidget(pParent)
    , mSettings(settings)
{
    setFont(Utility::getFont());
    createContent();
}

QSize SessionEditor::sizeHint() const
{
    return QSize(500, 1000);
}

GeometryView* SessionEditor::geometryView()
{
    return mpGeometryView;
}

ResponseEditor* SessionEditor::responseEditor()
{
    return mpResponseEditor;
}

//! Open the project located at the specfied path
bool SessionEditor::openProject(QString const& pathFile)
{
    if (!mSession.openProject(pathFile))
        return false;
    bool isValid = mSession.isProjectValid();
    if (isValid)
    {
        mpGeometryView->setGeometry(mSession.getGeometry());
        mpResponseEditor->removeAllBundles();
        mpProjectPath->setText(pathFile);
        Utility::setLastPathFile(mSettings, pathFile);
        qInfo() << tr("Testlab project is successfully opened");
    }
    else
    {
        mpGeometryView->setGeometry(Testlab::Geometry());
        mSession.closeProject();
        qWarning() << tr("Could not connect to a Testlab project. Make sure that the license server is running");
    }
    return isValid;
}

//! Create all the widgets
void SessionEditor::createContent()
{
    // Create the toolbar
    mpProjectPath = new Edit1s;
    mpProjectPath->setReadOnly(true);
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addWidget(new QLabel(tr("Testlab project: ")));
    pToolBar->addWidget(mpProjectPath);
    pToolBar->addAction(QIcon(":/icons/document-open.svg"), tr("Open project"), this, &SessionEditor::openProjectDialog);
    pToolBar->setIconSize(Constants::Size::skToolBarIcon);
    Utility::setShortcutHints(pToolBar);

    // Create the tab widget
    mpGeometryView = new GeometryView;
    mpResponseEditor = new ResponseEditor(mSettings, mSession);
    CustomTabWidget* pTabWidget = new CustomTabWidget;
    pTabWidget->setTabsRenamable(false);
    pTabWidget->setTabsClosable(false);
    pTabWidget->addTab(mpGeometryView, tr("Geometry"));
    pTabWidget->addTab(mpResponseEditor, tr("Responses"));
    pTabWidget->setCurrentIndex(0);

    // Create the main layout
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addWidget(pToolBar);
    pMainLayout->addWidget(pTabWidget);
    setLayout(pMainLayout);
}

//! Create a file dialog to open project
void SessionEditor::openProjectDialog()
{
    QString pathFile = QFileDialog::getOpenFileName(this, tr("Open Testlab Project"), Utility::getLastDirectory(mSettings).path(),
                                                    tr("Testlab file format (*.lms)"));
    if (pathFile.isEmpty())
        return;
    openProject(pathFile);
}

ResponseEditor::ResponseEditor(QSettings& settings, Session& session, QWidget* pParent)
    : QWidget(pParent)
    , mSettings(settings)
    , mSession(session)
{
    setFont(Utility::getFont());
    createContent();
}

ResponseCollection const& ResponseEditor::collection() const
{
    return mCollection;
}

int ResponseEditor::iSelectedBundle() const
{
    return mpBundleList->currentRow();
}

//! Add the response bundle
bool ResponseEditor::addBundle(Responses const& responses)
{
    if (responses.empty())
        return false;

    // Construct the default name
    QString path = QString::fromStdWString(responses.front().header.path);
    QString name;
    if (!path.isEmpty())
    {
        QStringList tokens = path.split('/', Qt::SkipEmptyParts);
        int numTokens = tokens.size();

        // Get the base name
        if (numTokens > 2)
            name = tokens[numTokens - 3];

        // Handle different types of records
        bool isSweep = name == "FRF" || name.contains("Sweep");
        bool isNoise = name == "FRFs";
        if (isSweep && numTokens > 4)
            name = tokens[numTokens - 5];
        else if (isNoise && numTokens > 3)
            name = tokens[numTokens - 4];
    }
    if (name.isEmpty())
        name = QObject::tr("Bundle %1").arg(mCollection.count() + 1);

    // Construct the bundle
    ResponseBundle bundle(name, responses);

    // Add to the collection
    mCollection.add(bundle);
    refresh();
    emit edited();
    qInfo() << tr("New response bundle is successfuly created. The number of responses is %1").arg(responses.size());
    return true;
}

//! Add the response bundle
bool ResponseEditor::addBundle(QStringList const& paths)
{
    auto responses = mSession.getResponses(paths);
    return addBundle(responses);
}

//! Add the selected response bundle
bool ResponseEditor::addSelectedBundle()
{
    auto responses = mSession.getSelectedResponses();
    return addBundle(responses);
}

//! Merge the currently selected bundle with the selected responses
bool ResponseEditor::mergeSelectedBundle()
{
    int iBundle = mpBundleList->currentRow();
    if (iBundle < 0)
        return false;
    auto responses = mSession.getSelectedResponses();
    if (responses.empty())
        return false;
    mCollection.merge(iBundle, responses);
    refresh();
    emit edited();
    qInfo() << tr("The selected responses are added to the selected bundle");
    return true;
}

//! Rename the currently active bundle
void ResponseEditor::editBundle()
{
    // Get the bundle
    int iBundle = mpBundleList->currentRow();
    if (iBundle < 0)
        return;
    ResponseBundle& bundle = mCollection.get(iBundle);

    // Create the editor
    ResponseBundleEditor* pEditor = new ResponseBundleEditor(bundle);
    connect(pEditor, &ResponseBundleEditor::edited, this, &ResponseEditor::processBundleEdited);
    Utility::showAsDialog(pEditor, tr("Response Bundle Editor"), this, false);
}

//! Remove the currently selected bundle
void ResponseEditor::removeBundle()
{
    int iBundle = mpBundleList->currentRow();
    if (mCollection.remove(iBundle))
    {
        refresh();
        emit edited();
        qInfo() << tr("Response bundle is deleted");
    }
}

//! Remove all the bundles
void ResponseEditor::removeAllBundles()
{
    mCollection.clear();
    refresh();
    emit edited();
    qInfo() << tr("All the response bundles are removed");
}

//! Create a dummy bundle to be further edited
void ResponseEditor::createBundle()
{
    ResponseBundle bundle(tr("New"), {ReportDefaults::response()});
    mCollection.add(bundle);
    refresh();
    emit edited();
    qInfo() << tr("The response bundle is created");
}

//! Read a bundle from a file
bool ResponseEditor::readBundle()
{
    // Get the file path
    QString pathFile = QFileDialog::getOpenFileName(this, tr("Read Response Bundle"), Utility::getLastDirectory(mSettings).path(),
                                                    tr("Response bundle format (*.%1)").arg(ResponseBundle::fileSuffix()));
    if (pathFile.isEmpty())
        return false;

    // Read the document
    ResponseBundle bundle;
    if (bundle.read(pathFile))
    {
        mCollection.add(bundle);
        refresh();
        emit edited();
        qInfo() << tr("Response bundle has been read from the file: %1").arg(pathFile);
        return true;
    }
    else
    {
        qWarning() << tr("Could not read a response bundle from the file: %1").arg(pathFile);
    }
    return false;
}

//! Write a bundle to a file
bool ResponseEditor::writeBundle()
{
    // Get the selected bundle
    int iBundle = mpBundleList->currentRow();
    if (iBundle < 0 || iBundle > mCollection.count())
    {
        qWarning() << tr("Response bundle selection is not valid for writing");
        return false;
    }
    ResponseBundle const& bundle = mCollection.get(iBundle);

    // Get the file path
    QString fileName = Utility::getLastDirectory(mSettings).path() + QDir::separator() + bundle.name;
    QString pathFile = QFileDialog::getSaveFileName(this, tr("Write Response Bundle"), fileName,
                                                    tr("Response bundle format (*.%1)").arg(ResponseBundle::fileSuffix()));
    if (pathFile.isEmpty())
        return false;

    // Modify the suffix, if necessary
    Utility::modifyFileSuffix(pathFile, ResponseBundle::fileSuffix());

    // Store the path
    Utility::setLastPathFile(mSettings, pathFile);

    // Write the bundle
    if (bundle.write(pathFile))
    {
        qInfo() << tr("Response bundle has been written to the file: %1").arg(pathFile);
        return true;
    }
    else
    {
        qWarning() << tr("Could not write the response bundle to the file: %1").arg(pathFile);
    }
    return false;
}

//! Move the selected bundle in the collection
void ResponseEditor::moveBundle(int iShift)
{
    // Get the selected bundle
    int iOld = mpBundleList->currentRow();

    // Perform the movement
    int iNew = iOld + iShift;
    if (mCollection.swap(iOld, iNew))
    {
        refresh();
        emit edited();
        qInfo() << tr("Bundle is moved");
        selectBundle(iNew);
    }
}

//! Select a bundle by its index
void ResponseEditor::selectBundle(int index)
{
    if (index >= 0 && index < mpBundleList->count())
        mpBundleList->setCurrentRow(index);
}

//! Update the widgets content
void ResponseEditor::refresh()
{
    // Add the bundles
    QSignalBlocker blockerBundleList(mpBundleList);
    int iBundle = mpBundleList->currentRow();
    mpBundleList->clear();
    int numBundles = mCollection.count();
    for (int i = 0; i != numBundles; ++i)
    {
        QListWidgetItem* pItem = new QListWidgetItem(mCollection.get(i).name);
        mpBundleList->addItem(pItem);
    }
    if (iBundle < 0 || iBundle >= numBundles)
        iBundle = numBundles - 1;
    mpBundleList->setCurrentRow(iBundle);

    // Set the bundle properties
    QSignalBlocker blockerBundleFreq(mpBundleFreqEdit);
    QSignalBlocker blockerBundleForce(mpBundleForceEdit);
    QSignalBlocker blockerBundleRefPoint(mpBundleRefPointEdit);
    mpBundleFreqEdit->setValue(0.0);
    mpBundleForceEdit->setValue(0.0);
    mpBundleRefPointEdit->setText(QString());
    iBundle = mpBundleList->currentRow();
    if (iBundle >= 0)
    {
        ResponseBundle const& bundle = mCollection.get(iBundle);
        mpBundleFreqEdit->setValue(bundle.freq);
        mpBundleForceEdit->setValue(bundle.force);
        mpBundleRefPointEdit->setText(bundle.refPoint);
        mpBundleInverseCheckBox->setChecked(bundle.isInverse);
    }

    // Add the responses
    QSignalBlocker blockerResponseList(mpResponseList);
    mpResponseList->clear();
    int numResponses = 0;
    if (iBundle >= 0)
    {
        ResponseBundle const& bundle = mCollection.get(iBundle);
        numResponses = bundle.size();
        for (int i = 0; i != numResponses; ++i)
        {
            Testlab::Response response = bundle.get(i);
            QString name = QString::fromStdWString(response.header.name);
            QIcon icon = Utility::getIcon(response.header.point.direction);
            QListWidgetItem* pItem = new QListWidgetItem(icon, name);
            mpResponseList->addItem(pItem);
        }
    }

    // Set the response label
    mpResponseCountLabel->setText(tr("Number of responses: %1").arg(numResponses));
}

//! Create all the widgets
void ResponseEditor::createContent()
{
    QHBoxLayout* pLayout = new QHBoxLayout;
    pLayout->addLayout(createBundleLayout());
    pLayout->addLayout(createResponseLayout());
    setLayout(pLayout);
}

//! Create bundle related widgets
QLayout* ResponseEditor::createBundleLayout()
{
    // Create the toolbar
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addAction(QIcon(":/icons/list-add.svg"), tr("Add bundle"), Qt::ALT | Qt::Key_A, this, &ResponseEditor::addSelectedBundle);
    pToolBar->addAction(QIcon(":/icons/list-merge.svg"), tr("Merge bundle"), Qt::ALT | Qt::Key_M, this, &ResponseEditor::mergeSelectedBundle);
    pToolBar->addAction(QIcon(":/icons/list-remove.svg"), tr("Remove bundle"), Qt::ALT | Qt::Key_D, this, &ResponseEditor::removeBundle);
    pToolBar->addSeparator();
    pToolBar->addAction(QIcon(":/icons/bundle-create.svg"), tr("Create bundle"), this, &ResponseEditor::createBundle);
    pToolBar->addAction(QIcon(":/icons/edit-edit.svg"), tr("Edit bundle"), Qt::ALT | Qt::Key_E, this, &ResponseEditor::editBundle);
    pToolBar->addAction(QIcon(":/icons/bundle-read.svg"), tr("Read bundle"), Qt::ALT | Qt::Key_R, this, &ResponseEditor::readBundle);
    pToolBar->addAction(QIcon(":/icons/bundle-write.svg"), tr("Write bundle"), Qt::ALT | Qt::Key_W, this, &ResponseEditor::writeBundle);
    pToolBar->addSeparator();
    pToolBar->addAction(QIcon(":/icons/arrow-up.svg"), tr("Move up"), Qt::ALT | Qt::Key_Up, this, [this]() { moveBundle(-1); });
    pToolBar->addAction(QIcon(":/icons/arrow-down.svg"), tr("Move down"), Qt::ALT | Qt::Key_Down, this, [this]() { moveBundle(+1); });
    Utility::setShortcutHints(pToolBar);

    // Create the list
    mpBundleList = new QListWidget;
    mpBundleList->setFont(font());
    mpBundleList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(mpBundleList, &QListWidget::itemSelectionChanged, this, &ResponseEditor::processBundleSelected);

    // Create the frequency edit
    mpBundleFreqEdit = new Edit1d;
    mpBundleFreqEdit->setMinimum(0.0);
    mpBundleFreqEdit->setMaximumWidth(90);
    connect(mpBundleFreqEdit, &Edit1d::valueChanged, this, &ResponseEditor::setBundleProperties);

    // Create the force edit
    mpBundleForceEdit = new Edit1d;
    mpBundleForceEdit->setMaximumWidth(90);
    connect(mpBundleForceEdit, &Edit1d::valueChanged, this, &ResponseEditor::setBundleProperties);

    // Create the reference point edit
    mpBundleRefPointEdit = new Edit1s;
    connect(mpBundleRefPointEdit, &Edit1s::editingFinished, this, &ResponseEditor::setBundleProperties);

    // Create the inverse checkbox
    mpBundleInverseCheckBox = new QCheckBox(tr("Inverse"));
    connect(mpBundleInverseCheckBox, &QCheckBox::clicked, this, &ResponseEditor::setBundleProperties);

    // Create the value layout
    QHBoxLayout* pValueLayout = new QHBoxLayout;
    pValueLayout->addWidget(new QLabel(tr("Freq.: ")));
    pValueLayout->addWidget(mpBundleFreqEdit);
    pValueLayout->addWidget(new QLabel(tr("Force: ")));
    pValueLayout->addWidget(mpBundleForceEdit);
    pValueLayout->addStretch();

    // Create the reference layout
    QHBoxLayout* pRefLayout = new QHBoxLayout;
    pRefLayout->addWidget(new QLabel(tr("Reference point: ")));
    pRefLayout->addWidget(mpBundleRefPointEdit);
    pRefLayout->addWidget(mpBundleInverseCheckBox);

    // Combine the widgets
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->setContentsMargins(0, 0, 0, 5);
    pMainLayout->addWidget(pToolBar);
    pMainLayout->addWidget(mpBundleList);
    pMainLayout->addLayout(pValueLayout);
    pMainLayout->addLayout(pRefLayout);
    return pMainLayout;
}

//! Create response related widgets
QLayout* ResponseEditor::createResponseLayout()
{
    // Create the toolbar
    QToolBar* pToolBar = new QToolBar;
    pToolBar->addAction(QIcon(":/icons/draw-graph.svg"), tr("Plot responses"), Qt::ALT | Qt::Key_V, this, &ResponseEditor::plotResponses);
    Utility::setShortcutHints(pToolBar);

    // Create the label
    mpResponseCountLabel = new QLabel;

    // Create the list
    mpResponseList = new QListWidget;
    mpResponseList->setFont(font());
    mpResponseList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(mpResponseList, &QListWidget::itemDoubleClicked, this, &ResponseEditor::plotResponses);

    // Create the layout
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->setContentsMargins(0, 0, 0, 5);
    pLayout->addWidget(pToolBar);
    pLayout->addWidget(mpResponseCountLabel);
    pLayout->addWidget(mpResponseList);
    return pLayout;
}

//! Process bundle selection
void ResponseEditor::processBundleSelected()
{
    refresh();
    emit selected();
}

//! Process bundle edition
void ResponseEditor::processBundleEdited()
{
    qInfo() << tr("Response bundle has been edited");
    refresh();
    emit edited();
}

//! Set the current bundle properties
void ResponseEditor::setBundleProperties()
{
    // Get the current bundle
    int iBundle = mpBundleList->currentRow();
    if (iBundle < 0)
        return;
    ResponseBundle& bundle = mCollection.get(iBundle);

    // Set the properties
    bundle.freq = mpBundleFreqEdit->value();
    bundle.force = mpBundleForceEdit->value();
    bundle.refPoint = mpBundleRefPointEdit->text();
    bundle.isInverse = mpBundleInverseCheckBox->isChecked();

    // Finish up the editing
    emit edited();
}

//! Plot selected responses
void ResponseEditor::plotResponses()
{
    // Get the current bundle
    int iBundle = mpBundleList->currentRow();
    if (iBundle < 0)
        return;
    ResponseBundle const& bundle = mCollection.get(iBundle);

    // Get the selected responses
    QModelIndexList indices = mpResponseList->selectionModel()->selectedIndexes();
    if (indices.isEmpty())
    {
        qWarning() << tr("There are no selected responses to be plotted");
        return;
    }
    Responses responses;
    for (QModelIndex const& index : std::as_const(indices))
    {
        int iResponse = index.row();
        if (iResponse >= 0 && iResponse < bundle.size())
            responses.push_back(bundle.get(iResponse));
    }

    // Show the view
    ResponseView* pView = new ResponseView;
    Utility::showAsDialog(pView, tr("Response View"), this, false);
    pView->plot(responses);
}

ResponseView::ResponseView(QWidget* pParent)
    : QWidget(pParent)
{
    setFont(Utility::getFont());
    createContent();
}

QSize ResponseView::sizeHint() const
{
    return QSize(800, 800);
}

//! Remove all the graphs
void ResponseView::clear()
{
    mpCurveList->clear();
    mpRealPlot->clear();
    mpImagPlot->clear();
    mpRealPlot->replot();
    mpImagPlot->replot();
}

//! Render the responses
void ResponseView::plot(std::vector<Testlab::Response> const& responses)
{
    // Constants
    QList<ReportCurve> const kCurves = ReportDefaults::curves();

    // Clear all the plottables
    clear();

    // Render all the responses
    int numResponses = responses.size();
    for (int iResponse = 0; iResponse != numResponses; ++iResponse)
    {
        // Get the response
        Testlab::Response const& response = responses[iResponse];
        QString name = Backend::Utility::getPointLabel(response.header.point);

        // Slice the data
        if (response.keys.empty())
            continue;
        QList<double> keys = Backend::Utility::convert(response.keys);
        QList<double> realValues = Backend::Utility::convert(response.realValues);
        QList<double> imagValues = Backend::Utility::convert(response.imagValues);

        // Add the plottables
        int iCurve = Utility::getRepeatedIndex(iResponse, kCurves.size());
        ReportCurve const& curve = kCurves[iCurve];
        addPlottable(mpRealPlot, keys, realValues, curve, name);
        addPlottable(mpImagPlot, keys, imagValues, curve, name);

        // Get the plottable icon
        QPen pen(curve.lineColor, curve.lineWidth, curve.lineStyle);
        QCPScatterStyle style((QCPScatterStyle::ScatterShape) curve.markerShape, curve.markerSize);
        if (curve.markerFill)
            style.setBrush(curve.lineColor);
        style.setPen(pen);
        bool isLine = curve.lineStyle != Qt::NoPen;
        bool isMarker = curve.markerShape != ReportMarkerShape::kNone;
        QIcon icon = Utility::getIcon(style, mpCurveList->iconSize(), isLine, isMarker);

        // Add the plottable to the list
        QListWidgetItem* pItem = new QListWidgetItem(icon, name);
        mpCurveList->addItem(pItem);
    }

    // Set the labels
    mpRealPlot->xAxis->setLabel(tr("Frequency, Hz"));
    mpRealPlot->yAxis->setLabel(tr("Real"));
    mpImagPlot->xAxis->setLabel(tr("Frequency, Hz"));
    mpImagPlot->yAxis->setLabel(tr("Imag"));

    // Show the auxiliary axes
    mpRealPlot->axisRect()->setupFullAxesBox(false);
    mpImagPlot->axisRect()->setupFullAxesBox(false);

    // Rescale the axes
    mpRealPlot->rescaleAxes();
    mpImagPlot->rescaleAxes();

    // Render the plot
    mpRealPlot->replot();
    mpImagPlot->replot();
}

//! Create all the widgets
void ResponseView::createContent()
{
    // Create the list of curves
    mpCurveList = new QListWidget;
    mpCurveList->setFont(font());
    mpCurveList->setSelectionMode(QAbstractItemView::NoSelection);
    mpCurveList->setResizeMode(QListWidget::Adjust);
    mpCurveList->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    mpCurveList->setIconSize(QSize(20, 20));

    // Create plots
    mpRealPlot = new CustomPlot;
    mpImagPlot = new CustomPlot;
    QVBoxLayout* pPlotLayout = new QVBoxLayout;
    pPlotLayout->addWidget(mpImagPlot);
    pPlotLayout->addWidget(mpRealPlot);
    QWidget* pPlotWidget = new QWidget;
    pPlotWidget->setLayout(pPlotLayout);

    // Create the splitter
    QSplitter* pSplitter = new QSplitter(Qt::Horizontal);
    pSplitter->addWidget(pPlotWidget);
    pSplitter->addWidget(mpCurveList);
    pSplitter->setStretchFactor(0, 1);
    pSplitter->setStretchFactor(1, 0);

    // Combine the widgets
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addWidget(pSplitter);
    setLayout(pMainLayout);
}

//! Create the plottable and add it to the requested plot
void ResponseView::addPlottable(CustomPlot* pPlot, QList<double> const& xData, QList<double> const& yData, ReportCurve const& curve,
                                QString const& name)
{
    // Define the style
    QPen pen(curve.lineColor, curve.lineWidth, curve.lineStyle);
    QCPScatterStyle scatterStyle((QCPScatterStyle::ScatterShape) curve.markerShape, curve.markerSize);
    if (curve.markerFill)
        scatterStyle.setBrush(curve.lineColor);

    // Modify the style, so that the markers are visible when the curve is not
    auto lineStyle = QCPCurve::lsLine;
    if (pen.style() == Qt::NoPen)
    {
        lineStyle = QCPCurve::lsNone;
        pen.setStyle(Qt::SolidLine);
    }

    // Create the plottable
    QCPCurve* pPlottable = new QCPCurve(pPlot->xAxis, pPlot->yAxis);
    pPlottable->setData(xData, yData);
    pPlottable->setLineStyle(lineStyle);
    pPlottable->setPen(pen);
    pPlottable->setName(name);
    pPlottable->setScatterStyle(scatterStyle);
    pPlottable->setScatterSkip(curve.markerSkip);
}

ResponseBundleEditor::ResponseBundleEditor(ResponseBundle& bundle, QWidget* pParent)
    : QWidget(pParent)
    , mBundle(bundle)
{
    setFont(Utility::getFont());
    createContent();
    createConnections();
    refresh();
}

QSize ResponseBundleEditor::sizeHint() const
{
    return QSize(800, 800);
}

//! Update all the widgets
void ResponseBundleEditor::refresh()
{
    // Set the name
    QSignalBlocker blockerName(mpNameEdit);
    mpNameEdit->setText(mBundle.name);

    // Get the data
    QString data;
    QTextStream stream(&data, QIODevice::WriteOnly);
    mBundle.write(stream);

    // Set the data
    QSignalBlocker blockerData(mpDataEdit);
    QTextBlock focusBlock = mpDataEdit->textCursor().block();
    mpDataEdit->clear();
    mpDataEdit->setPlainText(data);
    navigateByBlock(focusBlock);

    // Set the bookmarks
    QSignalBlocker blockerBookmark(mpBookmarkList);
    mpBookmarkList->clear();
    QRegularExpression const pattern(R"(^\s*Point\s*=\s*(.*?)\s*$)"); // "Point = Value"
    QTextBlock block = mpDataEdit->document()->begin();
    while (block.isValid())
    {
        QRegularExpressionMatch match = pattern.match(block.text());
        if (match.hasMatch())
        {
            QString text = match.captured(1);
            QIcon icon;
            if (text.isEmpty())
                text = tr("(unnamed)");
            else
                icon = Utility::getIcon(Backend::Utility::getDirValue(text.back()));
            QListWidgetItem* pItem = new QListWidgetItem(icon, text);
            pItem->setData(Qt::UserRole, block.blockNumber());
            mpBookmarkList->addItem(pItem);
        }
        block = block.next();
    }

    // Set the apply state
    mpApplyButton->setEnabled(false);
}

//! Create all the widgets
void ResponseBundleEditor::createContent()
{
    // Create the name edit
    mpNameEdit = new QLineEdit;
    QHBoxLayout* pNameLayout = new QHBoxLayout;
    pNameLayout->addWidget(new QLabel(tr("Name: ")));
    pNameLayout->addWidget(mpNameEdit);

    // Create the data edit
    mpDataEdit = new QPlainTextEdit;
    new ResponseBundleSyntaxHighlighter(mpDataEdit->document());
    QFont dataFont = Utility::getMonospaceFont();
    dataFont.setPointSize(dataFont.pointSize() - 1);
    mpDataEdit->setFont(dataFont);

    // Create the bookmark list
    mpBookmarkList = new QListWidget;
    mpBookmarkList->setFont(font());
    mpBookmarkList->setSelectionMode(QAbstractItemView::SingleSelection);
    mpBookmarkList->setResizeMode(QListWidget::Adjust);
    mpBookmarkList->setSizeAdjustPolicy(QListWidget::AdjustToContents);

    // Add the splitter
    QSplitter* pDataSplitter = new QSplitter(Qt::Horizontal);
    pDataSplitter->addWidget(mpDataEdit);
    pDataSplitter->addWidget(mpBookmarkList);
    pDataSplitter->setStretchFactor(0, 1);
    pDataSplitter->setStretchFactor(1, 0);

    // Create the apply button
    mpApplyButton = new QPushButton(QIcon(":/icons/apply.svg"), tr("Apply"));
    QHBoxLayout* pApplyLayout = new QHBoxLayout;
    pApplyLayout->addStretch();
    pApplyLayout->addWidget(mpApplyButton);

    // Combine all the widgets
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addLayout(pNameLayout);
    pMainLayout->addWidget(pDataSplitter);
    pMainLayout->addLayout(pApplyLayout);
    setLayout(pMainLayout);
}

//! Specify widget connections
void ResponseBundleEditor::createConnections()
{
    connect(mpNameEdit, &QLineEdit::textEdited, this, &ResponseBundleEditor::processStateChanged);
    connect(mpDataEdit, &QPlainTextEdit::textChanged, this, &ResponseBundleEditor::processStateChanged);
    connect(mpBookmarkList, &QListWidget::itemClicked, this, &ResponseBundleEditor::navigateByBookmark);
    connect(mpApplyButton, &QPushButton::clicked, this, &ResponseBundleEditor::apply);
}

//! Apply the changes
void ResponseBundleEditor::apply()
{
    // Get the data
    QString data = mpDataEdit->toPlainText();

    // Set the data
    QTextStream stream(&data, QIODevice::ReadOnly);
    mBundle.read(stream);

    // Set the name
    mBundle.name = mpNameEdit->text();
    mBundle.parseNameIntoProperties();

    // Finish up the editing
    refresh();
    emit edited();
}

//! Navigate by the selected bookmark
void ResponseBundleEditor::navigateByBookmark(QListWidgetItem* pItem)
{
    int blockNumber = pItem->data(Qt::UserRole).toInt();
    QTextBlock block = mpDataEdit->document()->findBlockByNumber(blockNumber);
    navigateByBlock(block);
}

//! Navigate by the block number
void ResponseBundleEditor::navigateByBlock(QTextBlock const& block)
{
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    mpDataEdit->setTextCursor(cursor);
    mpDataEdit->centerCursor();
    mpDataEdit->setFocus();
}

//! Handle any change in editor
void ResponseBundleEditor::processStateChanged()
{
    mpApplyButton->setEnabled(true);
}

ResponseBundleSyntaxHighlighter::ResponseBundleSyntaxHighlighter(QTextDocument* pDocument)
    : QSyntaxHighlighter(pDocument)
{
    // [Label] section headers, e.g. "[Response]"
    mLabelFormat.setForeground(QColor("black"));
    mLabelFormat.setFontWeight(QFont::Bold);

    // Property name, e.g. "Name" in "Name = value"
    mPropertyNameFormat.setForeground(QColor("blue"));

    // The '=' character itself
    mEqualsFormat.setForeground(QColor("gray"));

    // Comments, starting with '#' or '//'
    mCommentFormat.setFontItalic(true);

    HighlightRule ruleLabel;
    ruleLabel.pattern = QRegularExpression(R"(^\[.*\]$)");
    ruleLabel.format = mLabelFormat;
    mRules.append(ruleLabel);

    HighlightRule rulePropertyName;
    rulePropertyName.pattern = QRegularExpression(R"(^\s*\w+(?=\s*=))");
    rulePropertyName.format = mPropertyNameFormat;
    mRules.append(rulePropertyName);

    HighlightRule ruleEquals;
    ruleEquals.pattern = QRegularExpression(R"(=)");
    ruleEquals.format = mEqualsFormat;
    mRules.append(ruleEquals);

    // Comment pattern kept separate so it can override earlier rules on the same line
    mCommentPattern = QRegularExpression(R"((#|//).*$)");
}

void ResponseBundleSyntaxHighlighter::highlightBlock(QString const& text)
{
    for (HighlightRule const& rule : std::as_const(mRules))
    {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Apply comment formatting last so it overrides label/property/equals
    QRegularExpressionMatch commentMatch = mCommentPattern.match(text);
    if (commentMatch.hasMatch())
        setFormat(commentMatch.capturedStart(), commentMatch.capturedLength(), mCommentFormat);
}

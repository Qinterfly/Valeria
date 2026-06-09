#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>

#include "customlineedit.h"
#include "customtabwidget.h"
#include "geometryview.h"
#include "mathutility.h"
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

    // Helper function to parse name
    auto parseValue = [](QString const& text, QString const& postfix)
    {
        QString escapedPostfix = QRegularExpression::escape(postfix);
        QString pattern = QString("(-?\\d+[\\.,]?\\d*)\\s*%1").arg(escapedPostfix);
        QRegularExpression re(pattern);
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch())
        {
            QString valueStr = match.captured(1);
            valueStr.replace(",", ".");
            return valueStr.toDouble();
        }
        return 0.0;
    };

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
    bundle.freq = parseValue(name, "Гц");
    bundle.force = parseValue(name, "Н");

    // Estimate the frequency by the first root, if necessary
    if (bundle.freq < std::numeric_limits<double>::epsilon())
    {
        if (!responses.empty())
        {
            Testlab::Response const& response = responses.front();
            QList<double> xData = Backend::Utility::convert(response.keys);
            QList<double> yData = Backend::Utility::convert(response.realValues);
            auto roots = Backend::Utility::findRoots(xData, yData);
            if (!roots.empty())
                bundle.freq = roots.front().key;
        }
    }

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
void ResponseEditor::renameBundle()
{
    int iBundle = mpBundleList->currentRow();
    if (iBundle < 0)
        return;
    ResponseBundle& bundle = mCollection.get(iBundle);
    bool isOk;
    QString name = QInputDialog::getText(this, tr("Change bundle name"), tr("New name:"), QLineEdit::Normal, bundle.name, &isOk);
    if (isOk)
    {
        bundle.name = name;
        refresh();
        qInfo() << tr("The selected bundle is renamed");
        emit edited();
    }
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
            QListWidgetItem* pItem = new QListWidgetItem(QString::fromStdWString(response.header.name));
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
    pToolBar->addAction(QIcon(":/icons/list-add.svg"), tr("Add bundle"), this, &ResponseEditor::addSelectedBundle);
    pToolBar->addAction(QIcon(":/icons/list-merge.svg"), tr("Merge bundle"), this, &ResponseEditor::mergeSelectedBundle);
    pToolBar->addAction(QIcon(":/icons/list-rename.svg"), tr("Rename bundle"), this, &ResponseEditor::renameBundle);
    pToolBar->addAction(QIcon(":/icons/list-remove.svg"), tr("Remove bundle"), this, &ResponseEditor::removeBundle);

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
    // Create the label
    mpResponseCountLabel = new QLabel;

    // Create the list
    mpResponseList = new QListWidget;
    mpResponseList->setFont(font());
    mpResponseList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Create the layout
    QVBoxLayout* pLayout = new QVBoxLayout;
    pLayout->setContentsMargins(0, 0, 0, 5);
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

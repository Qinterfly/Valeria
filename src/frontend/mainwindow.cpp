#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>

#include "config.h"
#include "customstatusbar.h"
#include "geometryview.h"
#include "mainwindow.h"
#include "reportworkspace.h"
#include "sessioneditor.h"
#include "uiconstants.h"
#include "uiutility.h"

using namespace Frontend;
using namespace Backend::Core;

QString MainWindow::language = "ru";
CustomStatusBar* MainWindow::pStatusBar = nullptr;

void switchTranslator(QTranslator& translator, QString const& fileName, QString const& language, bool isResource);

MainWindow::MainWindow(QWidget* pParent, bool isRestore)
    : QMainWindow(pParent)
    , mSettings(Constants::Settings::skFileName, QSettings::IniFormat)
{
    initializeWindow();
    createContent();
    createConnections();
    setWindowState(Qt::WindowMaximized);
    if (isRestore)
        restoreSettings();
}

MainWindow::~MainWindow()
{
}

SessionEditor* MainWindow::sessionEditor()
{
    return mpSessionEditor;
}

ReportWorkspace* MainWindow::reportWorkspace()
{
    return mpReportWorkspace;
}

//! Set a state and geometry of the main window
void MainWindow::initializeWindow()
{
    setWindowTitle(QString(APP_NAME) + "[*]");
    setTheme();
    applyLanguage();
    qInstallMessageHandler(Frontend::logMessage);
}

//! Save project and settings before exit
void MainWindow::closeEvent(QCloseEvent* pEvent)
{
    bool isClose = saveChangesDialog();
    if (isClose)
    {
        saveSettings();
        pEvent->accept();
    }
    else
    {
        pEvent->ignore();
    }
}

//! Create all the widgets and corresponding actions
void MainWindow::createContent()
{
    // Top widgets
    createLanguageActions();
    createHelpActions();

    // Create the widgets
    mpSessionEditor = new SessionEditor(mSettings);
    mpReportWorkspace = new ReportWorkspace(mSettings, mpSessionEditor->geometryView(), mpSessionEditor->responseEditor());
    QSplitter* pSplitter = new QSplitter(Qt::Horizontal);
    pSplitter->addWidget(mpSessionEditor);
    pSplitter->addWidget(mpReportWorkspace);
    pSplitter->setHandleWidth(3);
    pSplitter->setStretchFactor(0, 4);
    pSplitter->setStretchFactor(1, 7);
    setCentralWidget(pSplitter);

    // Create the status bar
    MainWindow::pStatusBar = new CustomStatusBar(this);
    setStatusBar(pStatusBar);
}

//! Connect the widgets between each other
void MainWindow::createConnections()
{
    // Dependency editor
    connect(mpSessionEditor->geometryView()->dependencyEditor(), &GeometryDependencyEditor::edited, mpReportWorkspace, &ReportWorkspace::refresh);

    // Response editor
    connect(mpSessionEditor->responseEditor(), &ResponseEditor::edited, mpReportWorkspace, &ReportWorkspace::refresh);
    connect(mpSessionEditor->responseEditor(), &ResponseEditor::selected, mpReportWorkspace, &ReportWorkspace::refresh);

    // Report workspace
    connect(mpReportWorkspace, &ReportWorkspace::edited, this, [this]() { setModified(true); });
    connect(mpReportWorkspace, &ReportWorkspace::saved, this, [this]() { setModified(false); });
}

//! Create the action to change the application language
void MainWindow::createLanguageActions()
{
    // Create the menu
    QMenu* pMenu = new QMenu(tr("&Language"), this);
    pMenu->setFont(font());
    menuBar()->addMenu(pMenu);

    // Create the language group
    QActionGroup* pGroup = new QActionGroup(pMenu);
    pGroup->setExclusive(true);

    // Retrieve the available translations
    QDir dir(":/translations");
    QStringList fileNames = dir.entryList(QStringList("application_*.qm"));

    // Create the corresponding actions
    int numFiles = fileNames.size();
    for (int i = 0; i != numFiles; ++i)
    {
        // Extract the locale from the filename
        QString locale = fileNames[i];
        locale.truncate(locale.lastIndexOf('.'));
        locale.remove(0, locale.lastIndexOf('_') + 1);

        // Get the action properties
        QString text = QLocale::languageToString(QLocale(locale).language());
        QIcon icon(QString(":/icons/lang-%1.svg").arg(locale));

        // Create the action
        QAction* action = new QAction(icon, text, this);
        action->setCheckable(true);
        action->setData(locale);

        // Set default translators and language checked
        if (language == locale)
            action->setChecked(true);

        // Add the action
        pMenu->addAction(action);
        pGroup->addAction(action);
    }

    // Set the connections
    connect(pGroup, &QActionGroup::triggered, this, [this](QAction* pAction) { setLanguage(pAction->data().toString()); });
}

//! Create the actions to show the program info
void MainWindow::createHelpActions()
{
    // Create the actions
    QAction* pAboutAction = new QAction(tr("&About"), this);
    QAction* pAboutQtAction = new QAction(tr("&About Qt"), this);

    // Connect the actions
    connect(pAboutAction, &QAction::triggered, this, &MainWindow::about);
    connect(pAboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);

    // Fill up the menu
    QMenu* pHelpMenu = new QMenu(tr("&Help"), this);
    pHelpMenu->setFont(font());
    pHelpMenu->addAction(pAboutAction);
    pHelpMenu->addAction(pAboutQtAction);
    menuBar()->addMenu(pHelpMenu);
}

//! Whenever a project has been modified
void MainWindow::setModified(bool flag)
{
    setWindowModified(flag);
}

//! Set the application font, icon and style
void MainWindow::setTheme()
{
    // Set the fonts
    QFontDatabase::addApplicationFont(":/fonts/Roboto.ttf");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Monofur.ttf");
    QFont font = Utility::getFont();
    setFont(font);
    qApp->setFont(font);
    menuBar()->setFont(font);

    // Set the icon
    qApp->setWindowIcon(QIcon(":/icons/application.svg"));

    // Set the Fusion style
    qApp->setStyle("Fusion");

    // Set up the colors for the light palette
    const QColor windowText = Qt::black;
    const QColor backGround = QColor(239, 239, 239);
    const QColor light = backGround.lighter(150);
    const QColor mid = (backGround.darker(130));
    const QColor midLight = mid.lighter(110);
    const QColor base = Qt::white;
    const QColor disabledBase(backGround);
    const QColor dark = backGround.darker(150);
    const QColor darkDisabled = QColor(209, 209, 209).darker(110);
    const QColor text = Qt::black;
    const QColor highlight = QColor(48, 140, 198);
    const QColor hightlightedText = Qt::white;
    const QColor disabledText = QColor(190, 190, 190);
    const QColor button = backGround;
    const QColor shadow = dark.darker(135);
    const QColor disabledShadow = shadow.lighter(150);
    const QColor disabledHighlight(145, 145, 145);
    QColor placeholder = text;
    placeholder.setAlpha(128);

    // Set the light palette
    QPalette fusionPalette(windowText, backGround, light, dark, mid, text, base);
    fusionPalette.setBrush(QPalette::Midlight, midLight);
    fusionPalette.setBrush(QPalette::Button, button);
    fusionPalette.setBrush(QPalette::Shadow, shadow);
    fusionPalette.setBrush(QPalette::HighlightedText, hightlightedText);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::Text, disabledText);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::WindowText, disabledText);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::ButtonText, disabledText);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::Base, disabledBase);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::Dark, darkDisabled);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::Shadow, disabledShadow);
    fusionPalette.setBrush(QPalette::Active, QPalette::Highlight, highlight);
    fusionPalette.setBrush(QPalette::Inactive, QPalette::Highlight, highlight);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::Highlight, disabledHighlight);
    fusionPalette.setBrush(QPalette::Active, QPalette::Accent, highlight);
    fusionPalette.setBrush(QPalette::Inactive, QPalette::Accent, highlight);
    fusionPalette.setBrush(QPalette::Disabled, QPalette::Accent, disabledHighlight);
    fusionPalette.setBrush(QPalette::PlaceholderText, placeholder);
    qApp->setPalette(fusionPalette);

    // Set the widget style
    QFile file(":/styles/modern.qss");
    if (file.open(QFile::ReadOnly))
    {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
    }
}

//! Set the new language for application
void MainWindow::setLanguage(QString const& newLanguage)
{
    if (newLanguage == language)
        return;
    language = newLanguage;
    applyLanguage();
    restart();
}

//! Specify the application language based on its locale
void MainWindow::applyLanguage()
{
    // Check if the requested language can be provided
    if (language != "en" && language != "ru")
        language = "ru";

    // Force the application locale
    setLocale(QLocale::C);
    QLocale::setDefault(locale());

    // Assign the translators
    switchTranslator(mTranslatorApplication, "application", language, true);
    switchTranslator(mTranslatorQt, "qt", language, false);
}

//! Restart the window
void MainWindow::restart()
{
    close();
    MainWindow* pWindow = new MainWindow(nullptr, false);
    pWindow->show();
}

//! Save window settings to a file
void MainWindow::saveSettings()
{
    mSettings.beginGroup(Constants::Settings::skMainWindow);
    mSettings.setValue(Constants::Settings::skLanguage, MainWindow::language);
    mSettings.setValue(Constants::Settings::skGeometry, saveGeometry());
    mSettings.setValue(Constants::Settings::skState, saveState());
    mSettings.endGroup();
    if (mSettings.status() == QSettings::NoError)
        qInfo() << tr("Settings were written to the file %1").arg(Constants::Settings::skFileName);
}

//! Restore window settings from a file
void MainWindow::restoreSettings()
{
    if (mSettings.allKeys().empty())
        return;
    mSettings.beginGroup(Constants::Settings::skMainWindow);
    QString lang = mSettings.value(Constants::Settings::skLanguage).toString();
    if (lang == language)
    {
        bool isOk = restoreGeometry(mSettings.value(Constants::Settings::skGeometry).toByteArray())
                    && restoreState(mSettings.value(Constants::Settings::skState).toByteArray());
        if (isOk)
            qInfo() << tr("Settings were restored from the file %1").arg(Constants::Settings::skFileName);
    }
    mSettings.endGroup();
}

//! Save changes through dialog
bool MainWindow::saveChangesDialog()
{
    if (isWindowModified())
    {
        QString title = tr("Save changes");
        QString message = tr("Would you like to save report document before exiting?");
        int iResult = Utility::showSaveDialog(this, title, message);
        if (iResult < 0)
            return false;
        if (iResult == 1)
            mpReportWorkspace->saveAsDocumentDialog();
    }
    return true;
}

//! Show information about the program
void MainWindow::about()
{
    QString const date = QStringLiteral(__DATE__) + QStringLiteral(" ") + QStringLiteral(__TIME__);
    QString const author = tr("Pavel Lakiza");
    QString const contact = "qinterfly@gmail.com";
    QString const message = tr("%1 is a program to generate reports from Testlab\n\n"
                               "Build: %2 (%3)\n\n"
                               "Author (C) %4 (%5)")
                                .arg(APP_NAME, VERSION_FULL, date, author, contact);
    QString const title = tr("About %1").arg(APP_NAME);
    QMessageBox::about(this, title, message);
}

//! Update the main window state after editing
void MainWindow::processEdited()
{
    setModified(true);
    qInfo() << tr("Editing finished");
}

//! Helper function to log all the messages
void Frontend::logMessage(QtMsgType type, QMessageLogContext const& /*context*/, QString const& message)
{
    int const kTimeout = 10000;
    if (MainWindow::pStatusBar)
        MainWindow::pStatusBar->showMessage(type, message, kTimeout);
}

//! Helper function to switch translators
void switchTranslator(QTranslator& translator, QString const& fileName, QString const& language, bool isResource)
{
    // Remove the old translator
    qApp->removeTranslator(&translator);

    // Load the new translator
    QString baseDir = isResource ? ":" : QApplication::applicationDirPath();
    QString dir = baseDir + "/translations";
    QString pathFile = QString("%1/%2_%3.qm").arg(dir, fileName, language);
    if (translator.load(pathFile))
        qApp->installTranslator(&translator);
}

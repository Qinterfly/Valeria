#ifndef REPORTWORKSPACE_H
#define REPORTWORKSPACE_H

#include <QWidget>

#include "reportdocument.h"

QT_FORWARD_DECLARE_CLASS(QSettings)

namespace Frontend
{

class CustomTabWidget;
class CustomTable;
class ReportDesigner;
class GeometryView;
class ResponseEditor;

struct ReportWorkspaceOptions
{
    ReportWorkspaceOptions();
    ~ReportWorkspaceOptions() = default;

    QString lastPathFile;
    QString autoSavePathFile;
    int autoSaveDuration; // Milliseconds
};

class ReportWorkspace : public QWidget
{
    Q_OBJECT

public:
    ReportWorkspace(QSettings& settings, GeometryView* pGeometryView, ResponseEditor* pResponseEditor,
                    ReportWorkspaceOptions const& options = ReportWorkspaceOptions(), QWidget* pParent = nullptr);
    virtual ~ReportWorkspace() = default;

    QSize sizeHint() const override;
    Backend::Core::ReportDocument const& document() const;
    ReportDesigner* currentDesigner();
    ReportDesigner* designer(int iPage);
    ReportDesigner* designer(QString const& name);
    void refresh();
    void render(bool isPrint = false);

    // Document
    void setNewDocument();
    void setDefaultDocument();
    void setDocument(Backend::Core::ReportDocument const& document);
    void openDocumentDialog();
    void saveDocument();
    void saveAsDocumentDialog();
    bool printDocument(QString const& pathFile);
    bool printPage(QString const& pathFile, ReportDesigner* pDesigner);
    void printDocumentDialog();
    void printPageDialog(ReportDesigner* pDesigner);

    // Designer
    void addPage();
    void renameCurrentPage();
    void duplicateCurrentPage();
    void removeCurrentPage();
    void moveCurrentPage(int iShift);

signals:
    void edited();
    void saved();

private:
    void createContent();
    void createConnections();
    void recreateDesigners();

    // Widget
    void setNewDocumentDialog();
    void setDefaultDocumentDialog();
    void addDesigner(int iPage);
    void setAutoSave();
    QString getPrintPathDialog();

    // Slots
    void processDesignerSelected();
    void processDesignerEdited();
    void processTextEngineEdited();
    void processGlobalDataEdited();
    void editTextEngine();
    void editGlobalData();
    void setUniteModeshapeRange();

private:
    QSettings& mSettings;
    GeometryView* mpGeometryView;
    ResponseEditor* mpResponseEditor;
    ReportWorkspaceOptions mOptions;
    Backend::Core::ReportDocument mDocument;
    CustomTabWidget* mpDesignerTabs;
};

class ReportTextEngineEditor : public QWidget
{
    Q_OBJECT

public:
    ReportTextEngineEditor(QSettings& settings, Backend::Core::ReportTextEngine& textEngine, QWidget* pParent = nullptr);
    virtual ~ReportTextEngineEditor() = default;

    QSize sizeHint() const override;
    void refresh();
    void addVariable();
    void removeVariables();

signals:
    void edited();

private:
    void createContent();
    QList<int> selectedRows();

    // Slots
    void processItemChanged();

private:
    QSettings& mSettings;
    Backend::Core::ReportTextEngine& mTextEngine;
    CustomTable* mpTable;
};
}

#endif // REPORTWORKSPACE_H

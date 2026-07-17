#ifndef SESSIONEDITOR_H
#define SESSIONEDITOR_H

#include <QLabel>
#include <QWidget>

#include "session.h"
#include "uialiasdata.h"

QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QCheckBox)

namespace Frontend
{

class GeometryView;
class ResponseEditor;

class SessionEditor : public QWidget
{
    Q_OBJECT

public:
    SessionEditor(QSettings& settings, QWidget* pParent = nullptr);
    virtual ~SessionEditor() = default;

    QSize sizeHint() const;

    GeometryView* geometryView();
    ResponseEditor* responseEditor();
    bool openProject(QString const& pathFile);

private:
    void createContent();

    // Slots
    void openProjectDialog();

private:
    QSettings& mSettings;
    Backend::Core::Session mSession;

    // Project
    Edit1s* mpProjectPath;

    // Geometry
    GeometryView* mpGeometryView;

    // Response
    ResponseEditor* mpResponseEditor;
};

class ResponseEditor : public QWidget
{
    Q_OBJECT

public:
    ResponseEditor(QSettings& settings, Backend::Core::Session& session, QWidget* pParent = nullptr);
    virtual ~ResponseEditor() = default;

    Backend::Core::ResponseCollection const& collection() const;
    int iSelectedBundle() const;

    bool addBundle(Backend::Core::Responses const& responses);
    bool addBundle(QStringList const& paths);
    bool addSelectedBundle();
    bool mergeSelectedBundle();
    void renameBundle();
    void removeBundle();
    void removeAllBundles();
    bool readBundle();
    bool writeBundle();

signals:
    void edited();
    void selected();

private:
    void refresh();
    void createContent();
    QLayout* createBundleLayout();
    QLayout* createResponseLayout();

    // Slots
    void processBundleSelected();
    void setBundleProperties();

private:
    QSettings& mSettings;
    Backend::Core::Session& mSession;
    Backend::Core::ResponseCollection mCollection;

    // Bundle
    QListWidget* mpBundleList;
    Edit1d* mpBundleFreqEdit;
    Edit1d* mpBundleForceEdit;
    Edit1s* mpBundleRefPointEdit;
    QCheckBox* mpBundleInverseCheckBox;

    // Response
    QListWidget* mpResponseList;
    QLabel* mpResponseCountLabel;
};
}

#endif // SESSIONEDITOR_H

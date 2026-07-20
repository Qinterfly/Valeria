#ifndef SESSIONEDITOR_H
#define SESSIONEDITOR_H

#include <QLabel>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QWidget>

#include "session.h"
#include "uialiasdata.h"

QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)
QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace Backend::Core
{
class ReportCurve;
}

namespace Frontend
{

class GeometryView;
class ResponseEditor;
class CustomPlot;

//! Class to manipulate current geometry and response context
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

//! Class to edit and view collection of response bundles
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
    void editBundle();
    void removeBundle();
    void removeAllBundles();
    void createBundle();
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
    void processBundleEdited();
    void setBundleProperties();
    void plotResponses();

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

//! Class to plot responses
class ResponseView : public QWidget
{
    Q_OBJECT

public:
    ResponseView(QWidget* pParent = nullptr);
    virtual ~ResponseView() = default;

    void clear();
    void plot(std::vector<Testlab::Response> const& responses);

protected:
    QSize sizeHint() const override;

private:
    void createContent();
    void addPlottable(CustomPlot* pPlot, QList<double> const& xData, QList<double> const& yData, Backend::Core::ReportCurve const& curve,
                      QString const& name);

private:
    CustomPlot* mpRealPlot;
    CustomPlot* mpImagPlot;
    QListWidget* mpCurveList;
};

//! Class to edit a response bundle
class ResponseBundleEditor : public QWidget
{
    Q_OBJECT

public:
    ResponseBundleEditor(Backend::Core::ResponseBundle& bundle, QWidget* pParent = nullptr);
    virtual ~ResponseBundleEditor() = default;

    void refresh();

signals:
    void edited();

protected:
    QSize sizeHint() const override;

private:
    void createContent();
    void createConnections();
    void apply();
    void navigateByBookmark(QListWidgetItem* pItem);
    void navigateByBlock(QTextBlock const& block);
    void processStateChanged();

private:
    Backend::Core::ResponseBundle& mBundle;
    QLineEdit* mpNameEdit;
    QPlainTextEdit* mpDataEdit;
    QListWidget* mpBookmarkList;
    QPushButton* mpApplyButton;
};

//! Class to highlight text syntax of a response bundle
class ResponseBundleSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    ResponseBundleSyntaxHighlighter(QTextDocument* pDocument);

protected:
    void highlightBlock(QString const& text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> mRules;

    QTextCharFormat mLabelFormat;
    QTextCharFormat mPropertyNameFormat;
    QTextCharFormat mEqualsFormat;
    QTextCharFormat mCommentFormat;

    QRegularExpression mCommentPattern;
};
}

#endif // SESSIONEDITOR_H

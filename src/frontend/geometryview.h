#ifndef GEOMETRYVIEW_H
#define GEOMETRYVIEW_H

#include <QDialog>
#include <QWidget>

#include <Eigen/Core>

#include <vtkColor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPolyDataMapper.h>

#include <testlab/common.h>

class QVTKOpenGLNativeWidget;
class vtkCameraOrientationWidget;
class vtkPoints;
class vtkCellArray;
class vtkProperty;

QT_FORWARD_DECLARE_CLASS(QListWidget)
QT_FORWARD_DECLARE_CLASS(QListWidgetItem)

namespace Frontend
{

class CustomTable;

struct GeometrySelection
{
    GeometrySelection();
    GeometrySelection(int uiComponent, int uiNode);
    ~GeometrySelection() = default;

    bool isValid() const;

    bool operator==(GeometrySelection const& another) const;
    bool operator!=(GeometrySelection const& another) const;
    bool operator<(GeometrySelection const& another) const;
    bool operator>(GeometrySelection const& another) const;
    bool operator<=(GeometrySelection const& another) const;
    bool operator>=(GeometrySelection const& another) const;

    int iComponent;
    int iNode;
};

//! Class to process mouse and key events associated with a geometry view
class GeometryInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    friend class GeometryView;
    static GeometryInteractorStyle* New();

    GeometryInteractorStyle();
    virtual ~GeometryInteractorStyle() = default;

    void OnLeftButtonDown() override;
    void OnKeyPress() override;

    QList<GeometrySelection> selections() const;

public:
    double pickTolerance;
    double pickFactor;

private:
    // Selection
    void select(QList<GeometrySelection> const& keys);
    void select(vtkActor* actor);

    // Deselection
    void deselect(vtkActor* actor);
    void deselect(GeometrySelection key);
    void deselectAll();
    void clear();

    // Acquisition
    void registerActor(GeometrySelection const& key, vtkActor* value);
    GeometrySelection find(vtkActor* actor) const;
    int index(vtkActor* actor);

private:
    QList<vtkActor*> mSelectedActors;
    QMap<vtkActor*, vtkSmartPointer<vtkProperty>> mProperties;
    QMap<GeometrySelection, vtkActor*> mActors;
};

//! Rendering options
struct GeometryViewOptions
{
    GeometryViewOptions();
    ~GeometryViewOptions() = default;

    // Color scheme
    vtkColor3d sceneColor;
    vtkColor3d sceneColor2;
    vtkColor3d edgeColor;
    QList<vtkColor3d> componentColors;

    // Opacity
    double edgeOpacity;

    // Flags
    QList<bool> maskComponents;
    bool showEdges;
    bool showWireframe;
    bool showLabels;
    bool showVertices;
    bool showLines;
    bool showTrias;
    bool showQuads;

    // Dimensions
    Eigen::Vector3d sceneScale;
    double lineWidth;
    double pointScale;
    double fontSize;
    double pickTolerance;
    double pickFactor;
};

//! Class to edit view options
class GeometryViewEditor : public QDialog
{
    Q_OBJECT

public:
    GeometryViewEditor(Testlab::Geometry const& geometry, GeometryViewOptions& options, QWidget* pParent = nullptr);
    virtual ~GeometryViewEditor() = default;

    void refresh();

signals:
    void edited();

private:
    void createContent();
    void createConnections();

    // Slots
    void processComponentSelectionChanged();
    void processComponentDoubleClicked(QListWidgetItem* pItem);

private:
    Testlab::Geometry const& mGeometry;
    GeometryViewOptions& mOptions;
    QListWidget* mpComponentList;
};

//! Class to edit dependencies
class GeometryDependencyEditor : public QDialog
{
    Q_OBJECT

public:
    GeometryDependencyEditor(Testlab::Geometry& geometry, QWidget* pParent = nullptr);
    ~GeometryDependencyEditor() = default;

    QSize sizeHint() const override;

    void refresh();
    void clear();

signals:
    void edited();

private:
    void createContent();
    void setData();

    QString getFlagLabel(int value);
    int getFlagValue(QString const& label);

private:
    Testlab::Geometry& mGeometry;
    CustomTable* mpTable;
};

//! Class to plot a Testlab geometry
class GeometryView : public QWidget
{
    Q_OBJECT

public:
    GeometryView(GeometryViewOptions const& options = GeometryViewOptions());
    virtual ~GeometryView();

    void clear();
    void plot();
    void refresh();

    int numSelected();
    QList<GeometrySelection> selections() const;
    QList<QPair<QString, QString>> selectionPairs() const;
    Testlab::Geometry const& getGeometry() const;
    GeometryDependencyEditor* dependencyEditor();

    void clearSelection();
    bool addSelection(QString const& componentName, QString const& nodeName);
    void setGeometry(Testlab::Geometry geometry);

private:
    void initialize();

    // Content
    void createContent();
    void createConnections();

    // Drawing
    void drawGeometry();
    void drawComponent(int iComponent);
    void drawVertices(vtkSmartPointer<vtkPoints> points, int iComponent, vtkColor3d color, double opacity = 1.0);
    void drawVertexLabel(vtkSmartPointer<vtkActor> actor);
    void drawElements(vtkSmartPointer<vtkPoints> points, std::vector<std::vector<int>> const& indices, vtkColor3d color, double opacity = 1.0);
    vtkSmartPointer<vtkPoints> createPoints(std::vector<Testlab::Node> const& nodes);

    // Slots
    void showWidgetAtCenter(QWidget* pWidget);

private:
    Testlab::Geometry mGeometry;
    GeometryViewOptions mOptions;
    GeometryViewEditor* mpViewEditor;
    GeometryDependencyEditor* mpDependencyEditor;
    QVTKOpenGLNativeWidget* mRenderWidget;
    vtkSmartPointer<vtkRenderWindow> mRenderWindow;
    vtkSmartPointer<vtkRenderer> mRenderer;
    vtkSmartPointer<vtkCameraOrientationWidget> mOrientationWidget;
    vtkSmartPointer<GeometryInteractorStyle> mStyle;
    double mMaximumDimension;
};
}

#endif // GEOMETRYVIEW_H

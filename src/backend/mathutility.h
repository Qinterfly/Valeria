#ifndef MATHUTILITY_H
#define MATHUTILITY_H

#include <QList>

#include <testlab/api.h>

#include <Eigen/Core>

#include "reportcommon.h"

QT_FORWARD_DECLARE_CLASS(QRectF)

namespace Backend::Core
{
class ResponseBundle;
}

namespace Backend::Core
{
using GeometryState = QMap<Backend::Core::ReportPoint, Eigen::Vector3d>;
}

namespace Backend::Utility
{

// Common
QList<double> convert(std::vector<double> const& data);
std::vector<double> convert(QList<double> const& data);
Eigen::Vector3d convert3d(std::vector<double> const& data);
int findClosestKey(Testlab::Response const& response, double searchKey);
QString getDirLabel(Backend::Core::ReportDirection dir);
Backend::Core::ReportDirection getDirValue(QString const& label);
Backend::Core::ReportPoint getPoint(std::wstring const& name);
double getSignedAbsMax(Eigen::Vector3d const& data);
double parsePostfixValue(QString const& text, QString const& postfix);

// Response
Testlab::Response multiplyResponse(Testlab::Response const& response, double factor);
int findResponse(Backend::Core::ResponseBundle const& bundle, Backend::Core::ReportPoint const& point, Backend::Core::ReportDirection dir,
                 Testlab::ResponseType type, QString const& unit = QString());
Testlab::Response getAcceleration(Backend::Core::ResponseBundle const& bundle, Backend::Core::ReportPoint const& point,
                                  Backend::Core::ReportDirection targetDir, QString const& targetUnit);
Testlab::Response convertAcceleration(Backend::Core::ResponseBundle const& bundle, Testlab::Response const& accel, QString const& targetUnit);
Testlab::Node getNode(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName);
std::vector<double> getNodeCoords(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName);
std::vector<double> getNodeAngles(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName);
Eigen::Vector3cd projectResponse(Testlab::Response const& response, Testlab::Geometry const& geometry, int iKey);

// Geometry
double getMaximumDimension(Testlab::Geometry const& geometry);
Backend::Core::GeometryState getGeometryState(QString const& unit, Backend::Core::ResponseBundle const& bundle,
                                              Testlab::Geometry const& geometry);
void resolveGeometryStateSlaves(Backend::Core::GeometryState& state, Testlab::Geometry const& geometry);
PairDouble getColorMagnitudeRange(Backend::Core::GeometryState const& state, Testlab::Geometry const& geometry,
                                  Backend::Core::ReportColorTransform transform);
double getColorMagnitude(Eigen::Vector3d const& data, Backend::Core::ReportColorTransform transform);
Eigen::Vector3d getNodeValues(Backend::Core::GeometryState const& state, QString const& componentName, QString const& nodeName);
Eigen::Vector3d projectVector(Eigen::Vector3d const& current, Eigen::Vector3d const& base);
bool findLineIntersect(Eigen::Vector2d const& a1, Eigen::Vector2d const& a2, Eigen::Vector2d const& b1, Eigen::Vector2d const& b2,
                       Eigen::Vector2d& x);
void clampToBounds(QRectF& rect, QRectF const& bounds);
bool resolveOverlaps(QList<QRectF>& rects, QRectF const& bounds, int maxIterations = 20, double stiffness = 0.5);

// Roots
struct Root
{
    double key;
    double value;
    int index;
};
std::vector<Root> findRoots(QList<double> const& keys, QList<double> const& values);

// Interpolation
Eigen::VectorXd interpolateIDW(Eigen::VectorXd const& query, Eigen::MatrixXd const& points, Eigen::MatrixXd const& values, double power = 2.0);
}

#endif // MATHUTILITY_H

#ifndef MATHUTILITY_H
#define MATHUTILITY_H

#include <QList>

#include <testlab/api.h>

#include <Eigen/Core>

#include "reportitem.h"

namespace Backend::Core
{
class ResponseBundle;
}

namespace Backend::Utility
{

// Common
QList<double> convert(std::vector<double> const& data);
std::vector<double> convert(QList<double> const& data);
int findClosestKey(Testlab::Response const& response, double searchKey);
QString getDirLabel(Backend::Core::ReportDirection dir);

// Response
Testlab::Response multiplyResponse(Testlab::Response const& response, double factor);
int findResponse(Backend::Core::ResponseBundle const& bundle, Backend::Core::GraphReportPoint const& point, Backend::Core::ReportDirection dir,
                 Testlab::ResponseType type, QString const& unit = QString());
Testlab::Response getAcceleration(Backend::Core::ResponseBundle const& bundle, Backend::Core::GraphReportPoint const& point,
                                  Backend::Core::ReportDirection targetDir, QString const& targetUnit);
Testlab::Response convertAcceleration(Backend::Core::ResponseBundle const& bundle, Testlab::Response const& accel, QString const& targetUnit);
Testlab::Node getNode(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName);
std::vector<double> getNodeCoords(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName);
std::vector<double> getNodeAngles(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName);
Testlab::Response projectResponse(Testlab::Response const& response, Testlab::Geometry const& geometry, Backend::Core::ReportDirection dir);
Eigen::Vector3cd projectResponse(Testlab::Response const& response, Testlab::Geometry const& geometry, int iKey);

// Geometry
double getMaximumDimension(Testlab::Geometry const& geometry);

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

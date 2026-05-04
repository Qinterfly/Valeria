#include <Eigen/Geometry>
#include <complex.h>
#include <QMap>

#include "constants.h"
#include "mathutility.h"
#include "session.h"

using namespace Backend::Constants;
using namespace Backend::Core;
using namespace Eigen;

// Constants
static double const skEps = std::numeric_limits<double>::epsilon();
static double const skInf = std::numeric_limits<double>::infinity();

namespace Backend::Utility
{

QList<double> convert(std::vector<double> const& data)
{
    return QList<double>(data.begin(), data.end());
}

std::vector<double> convert(QList<double> const& data)
{
    return std::vector<double>(data.begin(), data.end());
}

//! Find closest key to the requested one
int findClosestKey(Testlab::Response const& response, double searchKey)
{
    int iFound = -1;
    double minDist = skInf;
    int numKeys = response.keys.size();
    for (int iKey = 0; iKey != numKeys; ++iKey)
    {
        double dist = std::abs(response.keys[iKey] - searchKey);
        if (dist < minDist)
        {
            minDist = dist;
            iFound = iKey;
        }
    }
    return iFound;
}

//! Get direction label
QString getDirLabel(ReportDirection dir)
{
    switch (dir)
    {
    case ReportDirection::kX:
        return "X";
    case ReportDirection::kY:
        return "Y";
    case ReportDirection::kZ:
        return "Z";
    default:
        break;
    }
    return QString();
}

//! Multiply real and imaginary parts of response by the specified factor
Testlab::Response multiplyResponse(Testlab::Response const& response, double factor)
{
    Testlab::Response result = response;
    int numKeys = result.keys.size();
    for (int i = 0; i != numKeys; ++i)
    {
        result.realValues[i] *= factor;
        result.imagValues[i] *= factor;
    }
    return result;
}

//! Find the response measured at the specified point along the requested direction
int findResponse(ResponseBundle const& bundle, GraphReportPoint const& point, ReportDirection dir, Testlab::ResponseType type,
                 QString const& unit)
{
    int iFound = -1;
    int numResponses = bundle.responses.size();
    for (int i = 0; i != numResponses; ++i)
    {
        Testlab::Response const& response = bundle.responses[i];

        // Slice the response data
        Testlab::ResponsePoint const& responsePoint = response.header.point;
        QString componentName = QString::fromStdWString(responsePoint.component);
        QString nodeName = QString::fromStdWString(responsePoint.node);
        QString unitName = QString::fromStdWString(response.header.unit.name);

        // Check the flags
        bool isPoint = componentName == point.component && nodeName == point.node;
        bool isDir = dir == ReportDirection::kNone ? true : (int) dir == (int) responsePoint.direction;
        bool isType = response.header.type == type;
        bool isUnit = unit.isEmpty() ? true : unit == unitName;
        if (isPoint && isDir && isType && isUnit)
            return i;
    }
    return iFound;
}

//! Retrieve acceleration response
Testlab::Response getAcceleration(ResponseBundle const& bundle, GraphReportPoint const& point, ReportDirection targetDir,
                                  QString const& targetUnit)
{
    int iResponse = findResponse(bundle, point, targetDir, Testlab::ResponseType::kAccel);
    if (iResponse < 0)
        return Testlab::Response();
    Testlab::Response const& accel = bundle.responses[iResponse];
    return convertAcceleration(bundle, accel, targetUnit);
}

//! Convert the acceleration to the requested units
Testlab::Response convertAcceleration(ResponseBundle const& bundle, Testlab::Response const& accel, QString const& targetUnit)
{
    // Constants
    double kMToMM = 1000.0;

    // Check if the response has the requested unit or the units are not set
    QString unit = QString::fromStdWString(accel.header.unit.name);
    if (unit.isEmpty() || targetUnit.isEmpty() || unit == targetUnit)
        return accel;

    // Build up all the possible units as to choose from them later on
    QMap<QString, Testlab::Response> responseSet;
    responseSet[unit] = accel;

    // Set the reference point
    bool isFRF = unit == Units::skM_S2_N;
    GraphReportPoint refPoint;
    ReportDirection refDir = ReportDirection::kNone;
    if (isFRF)
    {
        QString refComponent = QString::fromStdWString(accel.header.refPoint.component);
        QString refNode = QString::fromStdWString(accel.header.refPoint.node);
        refPoint = GraphReportPoint(refComponent, refNode);
        refDir = (ReportDirection) accel.header.refPoint.direction;
    }
    else
    {
        refPoint = GraphReportPoint(bundle.refPoint);
    }

    // Process the force, if presented
    int numKeys = accel.keys.size();
    int iForce = findResponse(bundle, refPoint, refDir, Testlab::ResponseType::kForce, Units::skN);
    if (iForce >= 0)
    {
        Testlab::Response const force = bundle.responses[iForce];
        Testlab::Response response = accel;
        for (int i = 0; i != numKeys; ++i)
        {
            std::complex<double> a = {accel.realValues[i], accel.imagValues[i]};
            std::complex<double> F = {force.realValues[i], force.imagValues[i]};
            std::complex<double> r = {0.0, 0.0};
            if (isFRF)
                r = a * F;
            else if (std::abs(F) > skEps)
                r = a / F;
            response.realValues[i] = r.real();
            response.imagValues[i] = r.imag();
        }
        if (isFRF)
            responseSet[Units::skM_S2] = response;
        else
            responseSet[Units::skM_S2_N] = response;
    }

    // Double integrate to compute displacements
    if (responseSet.contains(Units::skM_S2))
    {
        Testlab::Response response = responseSet[Units::skM_S2];
        for (int i = 0; i != numKeys; ++i)
        {
            std::complex<double> a = {response.realValues[i], response.imagValues[i]};
            std::complex<double> r = {0.0, 0.0};
            if (std::abs(response.keys[i]) > skEps)
                r = -a / std::pow(2.0 * M_PI * response.keys[i], 2.0);
            response.realValues[i] = r.real();
            response.imagValues[i] = r.imag();
        }
        responseSet[Units::skM] = response;
    }

    // Compute the results in millimeters
    if (responseSet.contains(Units::skM_S2))
        responseSet[Units::skMM_S2] = multiplyResponse(responseSet[Units::skM_S2], kMToMM);
    if (responseSet.contains(Units::skM_S2_N))
        responseSet[Units::skMM_S2_N] = multiplyResponse(responseSet[Units::skM_S2_N], kMToMM);
    if (responseSet.contains(Units::skM))
        responseSet[Units::skMM] = multiplyResponse(responseSet[Units::skM], kMToMM);

    // Return the result
    if (responseSet.contains(targetUnit))
        return responseSet[targetUnit];
    return Testlab::Response();
}

//! Find the Testlab associated node associated with the graph point
Testlab::Node getNode(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName)
{
    QString tName;

    // Loop through all the components
    int numComponents = geometry.components.size();
    for (int iComponent = 0; iComponent != numComponents; ++iComponent)
    {
        Testlab::Component const& component = geometry.components[iComponent];

        // Check if the component is the same
        tName = QString::fromStdWString(component.name);
        if (tName != componentName)
            continue;

        // Loop through all the nodes
        int numNodes = component.nodes.size();
        for (int iNode = 0; iNode != numNodes; ++iNode)
        {
            Testlab::Node const& node = component.nodes[iNode];

            // Check if the node is the same
            tName = QString::fromStdWString(node.name);
            if (tName == nodeName)
                return node;
        }
    }
    return {};
}

//! Get node coordinates
std::vector<double> getNodeCoords(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName)
{
    return getNode(geometry, componentName, nodeName).coordinates;
}

//! Get node angles
std::vector<double> getNodeAngles(Testlab::Geometry const& geometry, QString const& componentName, QString const& nodeName)
{
    return getNode(geometry, componentName, nodeName).angles;
}

//! Project response onto the target direction
Testlab::Response projectResponse(Testlab::Response const& response, Testlab::Geometry const& geometry, Backend::Core::ReportDirection dir)
{
    // Get the point angles
    QString component = QString::fromStdWString(response.header.point.component);
    QString node = QString::fromStdWString(response.header.point.node);
    std::vector<double> angles = getNodeAngles(geometry, component, node);
    if (angles.empty())
        return response;

    // Slice the directions
    int iRespDir = (int) response.header.point.direction - 1;
    int iDir = (int) dir - 1;
    if (iRespDir < 0 || iDir < 0)
        return response;

    // Construct the transformation matrix
    AngleAxisd rotX(angles[2], Vector3d::UnitX()); // YZ
    AngleAxisd rotY(angles[1], Vector3d::UnitY()); // XZ
    AngleAxisd rotZ(angles[0], Vector3d::UnitZ()); // XY
    Quaterniond q = rotX * rotY * rotZ;
    Matrix3d transform = q.toRotationMatrix();
    Vector3d proj = transform * Vector3d::Unit(iRespDir);
    double factor = proj[iDir];

    // Multiply the response
    Testlab::Response result = response;
    int numKeys = result.keys.size();
    for (int i = 0; i != numKeys; ++i)
    {
        result.realValues[i] *= factor;
        result.imagValues[i] *= factor;
    }

    return result;
}

//! Project the reponse value onto the global coordinate axes
Vector3cd projectResponse(Testlab::Response const& response, Testlab::Geometry const& geometry, int iKey)
{
    Vector3cd zero = Vector3cd::Zero();

    // Check if the key is valid
    int numKeys = response.keys.size();
    if (iKey < 0 || iKey >= numKeys)
        return zero;

    // Get the point angles
    QString component = QString::fromStdWString(response.header.point.component);
    QString node = QString::fromStdWString(response.header.point.node);
    std::vector<double> angles = getNodeAngles(geometry, component, node);
    if (angles.empty())
        return zero;

    // Slice the directions
    int iRespDir = (int) response.header.point.direction - 1;
    if (iRespDir < 0)
        return zero;

    // Construct the transformation matrix
    AngleAxisd rotX(angles[2], Vector3d::UnitX()); // YZ
    AngleAxisd rotY(angles[1], Vector3d::UnitY()); // XZ
    AngleAxisd rotZ(angles[0], Vector3d::UnitZ()); // XY
    Quaterniond q = rotX * rotY * rotZ;
    Matrix3d transform = q.toRotationMatrix();
    Vector3d proj = transform * Vector3d::Unit(iRespDir);

    // Multiply the projection
    std::complex<double> value(response.realValues[iKey], response.imagValues[iKey]);
    return value * proj;
}

//! Estimate the maximum dimension of the model
double getMaximumDimension(Testlab::Geometry const& geometry)
{
    // Constants
    double const kInf = std::numeric_limits<double>::infinity();
    int numComponents = geometry.components.size();

    // Find the minimum and maximum coordinates
    Vector3d minCoords;
    Vector3d maxCoords;
    minCoords.fill(kInf);
    maxCoords.fill(-kInf);
    int numCoords = minCoords.size();
    for (int iComponent = 0; iComponent != numComponents; ++iComponent)
    {
        Testlab::Component const& component = geometry.components[iComponent];
        int numNodes = component.nodes.size();
        for (int iNode = 0; iNode != numNodes; ++iNode)
        {
            Testlab::Node const& node = component.nodes[iNode];
            for (int iCoord = 0; iCoord != numCoords; ++iCoord)
            {
                double coord = node.coordinates[iCoord];
                minCoords[iCoord] = std::min(minCoords[iCoord], coord);
                maxCoords[iCoord] = std::max(maxCoords[iCoord], coord);
            }
        }
    }

    // Estimate the dimensions
    double result = 0.0;
    result = std::max(result, std::abs(maxCoords[0] - minCoords[0]));
    result = std::max(result, std::abs(maxCoords[1] - minCoords[1]));
    result = std::max(result, std::abs(maxCoords[2] - minCoords[2]));

    return result;
}

//! Find all the response roots
std::vector<Root> findRoots(QList<double> const& keys, QList<double> const& values)
{
    int numValues = values.size();
    std::vector<Root> roots;
    roots.reserve(numValues);
    for (int i = 0; i != numValues - 1; ++i)
    {
        double y1 = values[i];
        double y2 = values[i + 1];
        if (y1 * y2 < 0.0)
        {
            double x1 = keys[i];
            double x2 = keys[i + 1];
            double xc = x1 - y1 * (x2 - x1) / (y2 - y1);
            double yc = (xc - x1) / (x2 - x1) * (y2 - y1) + y1;
            Root root({xc, yc, i});
            roots.push_back(root);
        }
    }
    return roots;
}

//! Interpolate the data at the query point use the inverse distance weighting method
VectorXd interpolateIDW(Eigen::VectorXd const& query, Eigen::MatrixXd const& points, Eigen::MatrixXd const& values, double power)
{
    if (points.size() == 0 || points.size() != values.size())
        return {};
    int numPoints = points.rows();
    int numDirs = points.cols();
    VectorXd result(numDirs);
    for (int iDir = 0; iDir != numDirs; ++iDir)
    {
        double tResult = 0.0;
        double num = 0.0;
        double denom = 0.0;
        for (int iPoint = 0; iPoint != numPoints; ++iPoint)
        {
            double value = values(iPoint, iDir);
            double dist = (query - points.row(iPoint).transpose()).norm();
            if (dist < skEps)
            {
                tResult = value;
                break;
            }
            double w = 1.0 / std::pow(dist, power);
            num += w * value;
            denom += w;
        }
        if (denom > skEps)
            tResult = num / denom;
        result[iDir] = tResult;
    }
    return result;
}
}

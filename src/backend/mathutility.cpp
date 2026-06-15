#include <Eigen/Geometry>
#include <complex.h>
#include <QMap>
#include <QRect>
#include <QtMath>

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

//! Convert to three dimensional vector
Vector3d convert3d(std::vector<double> const& data)
{
    Vector3d result;
    if (data.size() == result.size())
        std::copy(data.begin(), data.end(), result.begin());
    return result;
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
    case ReportDirection::kN:
        return "N";
    default:
        break;
    }
    return QString();
}

//! Get point out of full node name
ReportPoint getPoint(std::wstring const& name)
{
    return ReportPoint(QString::fromStdWString(name));
}

//! Get signed absolute maximum value
double getSignedAbsMax(Vector3d const& data)
{
    int numData = data.size();
    double result = 0.0;
    for (int i = 0; i != numData; ++i)
    {
        if (std::abs(data[i]) > std::abs(result))
            result = data[i];
    }
    return result;
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
int findResponse(ResponseBundle const& bundle, ReportPoint const& point, ReportDirection dir, Testlab::ResponseType type, QString const& unit)
{
    int iFound = -1;
    int numResponses = bundle.size();
    for (int i = 0; i != numResponses; ++i)
    {
        Testlab::Response response = bundle.get(i);

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
Testlab::Response getAcceleration(ResponseBundle const& bundle, ReportPoint const& point, ReportDirection targetDir, QString const& targetUnit)
{
    int iResponse = findResponse(bundle, point, targetDir, Testlab::ResponseType::kAccel);
    if (iResponse < 0)
        return Testlab::Response();
    Testlab::Response accel = bundle.get(iResponse);
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
    ReportPoint refPoint;
    ReportDirection refDir = ReportDirection::kNone;
    if (isFRF)
    {
        QString refComponent = QString::fromStdWString(accel.header.refPoint.component);
        QString refNode = QString::fromStdWString(accel.header.refPoint.node);
        refPoint = ReportPoint(refComponent, refNode);
        refDir = (ReportDirection) accel.header.refPoint.direction;
    }
    else
    {
        refPoint = ReportPoint(bundle.refPoint);
    }

    // Process the force, if presented
    int numKeys = accel.keys.size();
    int iForce = findResponse(bundle, refPoint, refDir, Testlab::ResponseType::kForce, Units::skN);
    if (iForce >= 0)
    {
        Testlab::Response const force = bundle.get(iForce);
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

//! Project the reponse value onto the global coordinate axes
Vector3cd projectResponse(Testlab::Response const& response, Testlab::Geometry const& geometry, int iKey)
{
    Vector3cd zero = Vector3cd::Zero();

    // Check if the key is valid
    int numKeys = response.keys.size();
    if (iKey < 0 || iKey >= numKeys)
        return zero;

    // Get the point angles
    Testlab::ResponsePoint const& point = response.header.point;
    QString component = QString::fromStdWString(point.component);
    QString node = QString::fromStdWString(point.node);
    std::vector<double> angles = getNodeAngles(geometry, component, node);
    if (angles.empty())
        return zero;

    // Create the directional vector
    int iDir = (int) point.direction - 1;
    if (iDir < 0)
        return zero;
    Vector3d dir = point.sign * Vector3d::Unit(iDir);

    // Construct the transformation matrix
    AngleAxisd rotX(angles[2], Vector3d::UnitX()); // YZ
    AngleAxisd rotY(angles[1], Vector3d::UnitY()); // XZ
    AngleAxisd rotZ(angles[0], Vector3d::UnitZ()); // XY
    Quaterniond q = rotX * rotY * rotZ;
    Matrix3d transform = q.toRotationMatrix();
    Vector3d proj = transform * dir;

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

//! Get the deformed geometry state
GeometryState getGeometryState(QString const& unit, ResponseBundle const& bundle, Testlab::Geometry const& geometry)
{
    GeometryState result;
    int numResponses = bundle.size();
    int iFound = -1;
    for (int i = 0; i != numResponses; ++i)
    {
        // Retrieve the acceleration which has the requested units
        Testlab::Response response = bundle.get(i);
        if (response.header.type != Testlab::ResponseType::kAccel)
            continue;
        Testlab::Response accel = Backend::Utility::convertAcceleration(bundle, response, unit);
        int numKeys = accel.keys.size();
        if (numKeys == 0)
            continue;

        // Find the closest frequency to the resonance one
        if (iFound < 0 || iFound > numKeys)
            iFound = Backend::Utility::findClosestKey(accel, bundle.freq);
        if (iFound < 0)
            continue;

        // Set the field value
        QString componentName = QString::fromStdWString(accel.header.point.component);
        QString nodeName = QString::fromStdWString(accel.header.point.node);
        Vector3d value = projectResponse(accel, geometry, iFound).imag();
        ReportPoint point(componentName, nodeName);
        if (!result.contains(point))
            result[point] = Vector3d::Zero();
        result[point] += value;
    }
    return result;
}

//! Resolve dependencies between state values
void resolveGeometryStateSlaves(Backend::Core::GeometryState& state, Testlab::Geometry const& geometry)
{
    int numSlaves = geometry.dependencies.size();
    for (int iSlave = 0; iSlave != numSlaves; ++iSlave)
    {
        Testlab::Dependency const& dependency = geometry.dependencies[iSlave];

        // Get the slave
        ReportPoint slavePoint = getPoint(dependency.slave);
        if (!state.contains(slavePoint))
            continue;
        Vector3d slaveCoords = convert3d(getNodeCoords(geometry, slavePoint.component, slavePoint.node));
        Vector3d slaveValues = state[slavePoint];

        // Count the valid master nodes
        int numMasters = dependency.masters.size();
        int numValidMasters = 0;
        for (int iMaster = 0; iMaster != numMasters; ++iMaster)
        {
            ReportPoint masterPoint = getPoint(dependency.masters[iMaster]);
            if (!state.contains(masterPoint))
                continue;
            ++numValidMasters;
        }
        if (numValidMasters == 0)
            continue;

        // Get the data of master nodes
        int numDirs = slaveValues.size();
        MatrixXd masterCoords(numValidMasters, numDirs);
        MatrixXd masterValues(numValidMasters, numDirs);
        for (int iMaster = 0; iMaster != numMasters; ++iMaster)
        {
            ReportPoint masterPoint = getPoint(dependency.masters[iMaster]);
            if (!state.contains(masterPoint))
                continue;
            masterCoords.row(iMaster) = convert3d(getNodeCoords(geometry, masterPoint.component, masterPoint.node));
            masterValues.row(iMaster) = state[masterPoint];
        }

        // Interpolate the master values
        int numFlags = dependency.flags.size();
        VectorXd interpValues = interpolateIDW(slaveCoords, masterCoords, masterValues);
        for (int iFlag = 0; iFlag != numFlags; ++iFlag)
        {
            if (dependency.flags[iFlag] > 0)
                slaveValues[iFlag] = interpValues[iFlag];
        }

        // Store the result
        state[slavePoint] = slaveValues;
    }
}

//! Get the range of magnitudes
PairDouble getMagnitudeRange(GeometryState const& state, Testlab::Geometry const& geometry)
{
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    int numComponents = geometry.components.size();
    for (int iComponent = 0; iComponent != numComponents; ++iComponent)
    {
        Testlab::Component const& component = geometry.components[iComponent];
        QString componentName = QString::fromStdWString(component.name);
        int numNodes = component.nodes.size();
        for (int iNode = 0; iNode != numNodes; ++iNode)
        {
            Testlab::Node const& node = component.nodes[iNode];
            QString nodeName = QString::fromStdWString(node.name);
            Vector3d values = getNodeValues(state, componentName, nodeName);
            min = std::min(min, values.minCoeff());
            max = std::max(max, values.maxCoeff());
        }
    }
    return {min, max};
}

//! Get the vertex field value related to the node
Vector3d getNodeValues(GeometryState const& state, QString const& componentName, QString const& nodeName)
{
    Vector3d result = Vector3d::Zero();
    ReportPoint point(componentName, nodeName);
    if (state.contains(point))
        result = state[point];
    return result;
}

//! Project the vector onto another vector
Vector3d projectVector(Vector3d const& current, Vector3d const& base)
{
    double norm = base.squaredNorm();
    if (norm < skEps)
        return Vector3d::Zero();
    return (current.dot(base) / norm) * base;
}

//! Find intersection of two lines given by four points: a1->a2, b1->b2
bool findLineIntersect(Vector2d const& a1, Vector2d const& a2, Vector2d const& b1, Vector2d const& b2, Vector2d& x)
{
    // Build the directional vectors
    Eigen::Vector2d r = a2 - a1;
    Eigen::Vector2d s = b2 - b1;

    // Check if the lines are parallel
    double denom = r.cross(s);
    if (std::abs(denom) < skEps)
        return false;

    // Compute the intersection
    double t = (b1 - a1).cross(s) / denom;
    x = a1 + t * r;

    return true;
}

//! Move the rectangle so it does not intersect boundaries
void clampToBounds(QRectF& rect, QRectF const& bounds)
{
    double dx = 0.0;
    double dy = 0.0;

    if (rect.left() < bounds.left())
        dx = bounds.left() - rect.left();
    else if (rect.right() > bounds.right())
        dx = bounds.right() - rect.right();

    if (rect.top() < bounds.top())
        dy = bounds.top() - rect.top();
    else if (rect.bottom() > bounds.bottom())
        dy = bounds.bottom() - rect.bottom();

    rect.translate(dx, dy);
}

//! Resolve overlaps between rectangles
bool resolveOverlaps(QList<QRectF>& rects, QRectF const& bounds, int maxIterations, double stiffness)
{
    // Constants
    double const kEps = std::numeric_limits<double>::epsilon();

    // Ensure that the rects are inside boundaries
    int numRects = rects.size();
    for (int i = 0; i != numRects; ++i)
        clampToBounds(rects[i], bounds);

    // Iterate till there is no overlap between rectangles
    QList<QPointF> shifts(numRects);
    for (int iIter = 0; iIter != maxIterations; ++iIter)
    {
        // Compute the shifts
        bool isOverlap = false;
        shifts.fill(QPointF(0.0, 0.0));
        for (int iRect = 0; iRect != numRects; ++iRect)
        {
            QRectF const& rectI = rects[iRect];
            QPointF centerI = rectI.center();
            for (int jRect = iRect + 1; jRect != numRects; ++jRect)
            {
                QRectF const& rectJ = rects[jRect];
                QPointF centerJ = rectJ.center();

                // Compute the overlap
                QRectF overlap = rectI.intersected(rectJ);
                if (overlap.isEmpty())
                    continue;
                isOverlap = true;

                // Handle coincident centers
                double dx = centerJ.x() - centerI.x();
                double dy = centerJ.y() - centerI.y();
                if (std::abs(dx) < kEps && std::abs(dy) < kEps)
                {
                    dx = 1.0;
                    dy = 0.0;
                }
                double signX = std::copysign(1.0, dx);
                double signY = std::copysign(1.0, dy);

                // Compute the push intensity
                double pushX = 0.0;
                double pushY = 0.0;
                if (std::abs(dx) > kEps)
                    pushX = (overlap.width() / 2 + 1.0) * stiffness;
                if (std::abs(dy) > kEps)
                    pushY = (overlap.height() / 2 + 1.0) * stiffness;

                // Compute the shift
                QPointF shift(pushX * signX, pushY * signY);

                // Accumulate the shifts
                shifts[iRect] -= shift;
                shifts[jRect] += shift;
            }
        }
        if (!isOverlap)
            return true;

        // Apply the shifts
        for (int iRect = 0; iRect != numRects; ++iRect)
        {
            rects[iRect].translate(shifts[iRect]);
            clampToBounds(rects[iRect], bounds);
        }
    }
    return false;
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

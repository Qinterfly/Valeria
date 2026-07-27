#include <QList>
#include <QObject>

#include <QRegularExpression>

#include <testlab/api.h>

#include "fileutility.h"
#include "mathutility.h"
#include "session.h"

using namespace Backend;
using namespace Backend::Core;

void writeDataTable(QTextStream& stream, Testlab::Response const& response);

ResponseBundle::ResponseBundle()
    : freq(0.0)
    , force(0.0)
    , isInverse(false)
{
}

ResponseBundle::ResponseBundle(QString const& uName, Responses const& uResponses)
    : ResponseBundle()
{
    name = uName;
    mResponses = uResponses;
    parseNameIntoProperties();
}

bool ResponseBundle::isEmpty() const
{
    return mResponses.empty();
}

int ResponseBundle::size() const
{
    return mResponses.size();
}

//! Get a response by its index
Testlab::Response ResponseBundle::get(int index) const
{
    if (index < 0 || index >= mResponses.size())
        return Testlab::Response();
    Testlab::Response result = mResponses[index];
    if (isInverse)
    {
        size_t numKeys = result.keys.size();
        for (size_t i = 0; i != numKeys; ++i)
        {
            result.realValues[i] *= -1.0;
            result.imagValues[i] *= -1.0;
        }
    }
    return result;
}

//! Merge current set of responses with given ones
void ResponseBundle::merge(Responses const& uResponses)
{
    for (Testlab::Response const& v : uResponses)
        mResponses.push_back(v);
}

//! Get associated file suffix
QString ResponseBundle::fileSuffix()
{
    return "txt";
}

//! Read responses from a text stream
bool ResponseBundle::read(QTextStream& stream)
{
    // Erase the data
    *this = ResponseBundle();

    // Helpers
    auto isEqual = [](QString const& lhs, QString const& rhs) { return lhs.compare(rhs, Qt::CaseInsensitive) == 0; };
    auto stringToPoint = [](QString const& value)
    {
        Testlab::ResponsePoint point;
        auto tokens = value.split(':');
        int numTokens = tokens.size();
        if (numTokens >= 2)
        {
            point.component = tokens[0].toStdWString();
            point.node = tokens[1].toStdWString();
            point.name = point.component + L":" + point.node;
            if (numTokens == 3 && tokens[2].size() == 2)
            {
                point.sign = (tokens[2].at(0) == '+') ? 1 : -1;
                point.direction = (Testlab::Direction) Utility::getDirValue(tokens[2].at(1));
            }
        }
        return point;
    };

    // Read the file
    Testlab::Response* pResponse = nullptr;
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        QString trimmed = line.trimmed();

        // Finish the response block if the empty line occurs
        if (trimmed.isEmpty())
        {
            pResponse = nullptr;
            continue;
        }

        // Skip the comments
        if (trimmed.startsWith('#') || trimmed.startsWith("//"))
            continue;

        // Parse the block label
        if (isEqual(trimmed, "[Response]"))
        {
            pResponse = &mResponses.emplace_back();
            continue;
        }

        // Do not parse if the response is not created
        if (!pResponse)
            continue;

        // Parse the header
        int pos = trimmed.indexOf('=');
        if (pos >= 0)
        {
            QString key = trimmed.left(pos).trimmed();
            QString value = trimmed.mid(pos + 1).trimmed();

            // Parse the properties
            bool isParsed = true;
            if (isEqual(key, "Name"))
                pResponse->header.name = value.toStdWString();
            else if (isEqual(key, "Point"))
                pResponse->header.point = stringToPoint(value);
            else if (isEqual(key, "RefPoint"))
                pResponse->header.refPoint = stringToPoint(value);
            else if (isEqual(key, "Unit"))
                pResponse->header.unit.name = value.toStdWString();
            else if (isEqual(key, "Type"))
                pResponse->header.type = (Testlab::ResponseType) value.toInt();
            else
                isParsed = false;

            // Set default properties
            if (pResponse->header.name.empty() && !pResponse->header.point.name.empty())
                pResponse->header.name = pResponse->header.point.name;
            if (pResponse->header.type == Testlab::ResponseType::kNone)
                pResponse->header.type = Testlab::ResponseType::kAccel;

            // Go to the next line if the property is parsed successfully
            if (isParsed)
                continue;
        }

        // Parse the data
        QTextStream lineStream(&line);
        line.replace(',', '.');
        double key, real, imag;
        QString label;
        // 1. Key
        lineStream >> key;
        if (lineStream.status() != QTextStream::Ok)
            continue;
        // 2. Real
        lineStream >> real;
        if (lineStream.status() != QTextStream::Ok)
            continue;
        // 3. Imag (optional)
        qint64 linePos = lineStream.pos();
        lineStream >> imag;
        if (lineStream.status() != QTextStream::Ok)
        {
            imag = 0.0;
            lineStream.resetStatus();
            lineStream.seek(linePos);
        }
        // 4. Label (optional)
        label = lineStream.readAll().trimmed();

        // Add the data
        pResponse->keys.push_back(key);
        pResponse->realValues.push_back(real);
        pResponse->imagValues.push_back(imag);
        pResponse->labels.push_back(label.toStdWString());
    }

    return !mResponses.empty();
}

//! Write responses to a text stream
bool ResponseBundle::write(QTextStream& stream) const
{
    // Helper functions
    auto writeProperty = [&stream](QString const& name, QString const& value)
    {
        if (value.isEmpty())
            return;
        stream << QString("%1 = %2\r\n").arg(name, value);
    };

    // Loop through all the responses
    int numResponses = mResponses.size();
    for (int iResponse = 0; iResponse != numResponses; ++iResponse)
    {
        Testlab::Response const& response = mResponses[iResponse];

        // Write the label
        stream.setFieldAlignment(QTextStream::AlignLeft);
        stream << Qt::endl << "[Response]" << Qt::endl;

        // Write the header
        writeProperty("Name", QString::fromStdWString(response.header.name));
        writeProperty("Point", Utility::getPointLabel(response.header.point));
        writeProperty("RefPoint", Utility::getPointLabel(response.header.refPoint));
        writeProperty("Unit", QString::fromStdWString(response.header.unit.name));
        writeProperty("Type", QString::number((int) response.header.type));

        // Write the data
        writeDataTable(stream, response);
    }

    return true;
}

//! Read responses from a text file
bool ResponseBundle::read(QString const& pathFile)
{
    // Open file for reading
    auto pFile = Utility::openFile(pathFile, fileSuffix(), QIODevice::ReadOnly);
    if (!pFile)
        return false;

    // Create the stream
    QTextStream stream(pFile.data());

    // Read the data
    bool isRead = read(stream);

    // Set the properties
    name = QFileInfo(pathFile).baseName();
    parseNameIntoProperties();

    return isRead;
}

//! Write responses to a text file
bool ResponseBundle::write(QString const& pathFile) const
{
    // Open file for writing
    auto pFile = Utility::openFile(pathFile, fileSuffix(), QIODevice::WriteOnly);
    if (!pFile)
        return false;

    // Create the stream
    QTextStream stream(pFile.data());

    // Write the data
    bool isWrite = write(stream);

    return isWrite;
}

//! Estimate frequency and force by name
void ResponseBundle::parseNameIntoProperties()
{
    // Parse the name
    freq = Utility::parsePostfixValue(name, "Гц");
    force = Utility::parsePostfixValue(name, "Н");

    // Estimate the frequency by the first root, if necessary
    if (freq < std::numeric_limits<double>::epsilon())
    {
        if (!mResponses.empty())
        {
            Testlab::Response const& response = mResponses.front();
            QList<double> xData = Utility::convert(response.keys);
            QList<double> yData = Utility::convert(response.realValues);
            auto roots = Utility::findRoots(xData, yData);
            if (!roots.empty())
                freq = roots.front().key;
        }
    }
}

ResponseCollection::ResponseCollection()
{
}

bool ResponseCollection::isEmpty() const
{
    return mBundles.isEmpty();
}

bool ResponseCollection::isExist(int index) const
{
    return index >= 0 && index < mBundles.size();
}

int ResponseCollection::count() const
{
    return mBundles.size();
}

ResponseBundle& ResponseCollection::get(int index)
{
    return mBundles[index];
}

ResponseBundle const& ResponseCollection::get(int index) const
{
    return mBundles[index];
}

ResponseBundle& ResponseCollection::add(Responses const& responses, QString const& name)
{
    return mBundles.emplace_back(name, responses);
}

void ResponseCollection::add(ResponseBundle const& bundle)
{
    mBundles.push_back(bundle);
}

bool ResponseCollection::merge(int index, Responses const& responses)
{
    if (!isExist(index))
        return false;
    ResponseBundle& bundle = mBundles[index];
    bundle.merge(responses);
    return true;
}

bool ResponseCollection::remove(int index)
{
    if (!isExist(index))
		return false;
	mBundles.remove(index);
	return true;
}

void ResponseCollection::clear()
{
    mBundles.clear();
}

bool ResponseCollection::swap(int iFirst, int iSecond)
{
    if (!isExist(iFirst) || !isExist(iSecond))
        return false;
    mBundles.swapItemsAt(iFirst, iSecond);
    return true;
}

Session::Session()
    : mpProject(nullptr)
{
}

Session::~Session()
{
    closeProject();
}

//! Check if the project is valid
bool Session::isProjectValid()
{
    return mpProject && mpProject->isValid();
}

//! Open a Testlab project
bool Session::openProject(QString const& pathFile)
{
    closeProject();
    mpProject = Testlab::openProject(pathFile.toStdWString());
    return mpProject != nullptr;
}

//! Close the project, if opened
void Session::closeProject()
{
    if (!mpProject)
        return;
    delete mpProject;
    mpProject = nullptr;
}

//! Retrieve the current geometry
Testlab::Geometry Session::getGeometry()
{
    if (isProjectValid())
        return mpProject->getGeometry();
    return {};
}

//! Retrieve the responses using their paths inside the project
Responses Session::getResponses(QStringList const& paths)
{
    if (!isProjectValid())
        return {};
    int numPaths = paths.size();
    std::vector<std::wstring> cPaths(numPaths);
    for (int i = 0; i != numPaths; ++i)
        cPaths[i] = paths[i].toStdWString();
    return mpProject->getResponses(cPaths);
}

//! Retrieve the currently selected responses
Responses Session::getSelectedResponses()
{
    if (isProjectValid())
        return mpProject->getSelectedResponses();
    return {};
}

//! Helper function to print a data table to a text stream
void writeDataTable(QTextStream& stream, Testlab::Response const& response)
{
    // Constants
    int const kGap = 2;

    // Helper functions
    auto valueToString = [](double value) { return QString::number(value, 'g', 6); };
    auto maxLength = [](QVector<QString> const& values)
    {
        int maxLen = 0;
        for (QString const& value : values)
            maxLen = std::max(maxLen, (int) value.size());
        return maxLen;
    };

    // Convert the data to strings so column widths can be measured
    int numData = response.keys.size();
    QVector<QString> keyStrs(numData), realStrs(numData), imagStrs(numData), labelStrs(numData);
    bool isLabels = response.labels.size() == numData;
    for (int iData = 0; iData != numData; ++iData)
    {
        keyStrs[iData] = valueToString(response.keys[iData]);
        realStrs[iData] = valueToString(response.realValues[iData]);
        imagStrs[iData] = valueToString(response.imagValues[iData]);
        if (isLabels)
            labelStrs[iData] = QString::fromStdWString(response.labels[iData]);
    }

    // Determine the widest value in each column, plus a small gap
    int widthKey = maxLength(keyStrs) + kGap;
    int widthReal = maxLength(realStrs) + kGap;
    int widthImag = maxLength(imagStrs) + kGap;
    int widthLabel = maxLength(labelStrs) + kGap;

    // Write the data as a fixed-width table
    stream.setFieldAlignment(QTextStream::AlignRight);
    for (int iData = 0; iData != numData; ++iData)
    {
        // Key
        stream.setFieldWidth(widthKey);
        stream << keyStrs[iData];
        // Real
        stream.setFieldWidth(widthReal);
        stream << realStrs[iData];
        // Imag
        stream.setFieldWidth(widthImag);
        stream << imagStrs[iData];
        // Label
        stream.setFieldWidth(widthLabel);
        stream << labelStrs[iData];
        // New line
        stream.setFieldWidth(0);
        stream << Qt::endl;
    }
    stream.setFieldAlignment(QTextStream::AlignLeft);
}

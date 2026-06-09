#include <QList>
#include <QObject>

#include <testlab/api.h>

#include "session.h"

using namespace Backend::Core;

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
}

bool ResponseBundle::isEmpty() const
{
    return mResponses.empty();
}

int ResponseBundle::size() const
{
    return mResponses.size();
}

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

void ResponseBundle::merge(Responses const& uResponses)
{
    for (Testlab::Response const& v : uResponses)
        mResponses.push_back(v);
}

ResponseCollection::ResponseCollection()
{
}

bool ResponseCollection::isEmpty() const
{
    return mBundles.isEmpty();
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

void ResponseCollection::merge(int index, Responses const& responses)
{
    if (index < 0 || index >= mBundles.size())
        return;
    ResponseBundle& bundle = mBundles[index];
    bundle.merge(responses);
}

bool ResponseCollection::remove(int index)
{
    if (index >= 0 && index < mBundles.size())
    {
        mBundles.remove(index);
        return true;
    }
    return false;
}

void ResponseCollection::clear()
{
    mBundles.clear();
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

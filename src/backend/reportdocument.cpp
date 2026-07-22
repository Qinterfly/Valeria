#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <config.h>

#include "fileutility.h"
#include "reportdocument.h"
#include "reportitem.h"

using namespace Backend::Core;

ReportPage::ReportPage()
{
    layout.setPageSize(QPageSize::A4);
    layout.setMargins(QMargins(0, 0, 0, 0));
    layout.setUnits(QPageLayout::Millimeter);
    layout.setOrientation(QPageLayout::Portrait);
}

ReportPage::ReportPage(QString const& uName)
    : ReportPage()
{
    name = uName;
}

ReportPage::ReportPage(ReportPage const& another)
{
    *this = another;
}

ReportPage& ReportPage::operator=(ReportPage const& another)
{
    if (this == &another)
        return *this;
    clear();
    layout = another.layout;
    name = another.name;
    int numItems = another.mItems.size();
    mItems.resize(numItems);
    for (int i = 0; i != numItems; ++i)
        mItems[i] = another.mItems[i]->clone();
    return *this;
}

ReportPage::~ReportPage()
{
    clear();
}

int ReportPage::count() const
{
    return mItems.size();
}

bool ReportPage::isExist(int index) const
{
    return index >= 0 && index < mItems.size();
}

ReportItem* ReportPage::get(int index)
{
    if (isExist(index))
        return mItems[index];
    return nullptr;
}

ReportItem const* ReportPage::get(int index) const
{
    if (isExist(index))
        return mItems[index];
    return nullptr;
}

ReportItem* ReportPage::get(QString const& name)
{
    int numItems = mItems.size();
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem* pItem = mItems[i];
        if (pItem->name == name)
            return pItem;
    }
    return nullptr;
}

ReportItem* ReportPage::get(QUuid const& id)
{
    int numItems = mItems.size();
    for (int i = 0; i != numItems; ++i)
    {
        ReportItem* pItem = mItems[i];
        if (pItem->id == id)
            return pItem;
    }
    return nullptr;
}

void ReportPage::add(ReportItem* pItem)
{
    mItems.push_back(pItem);
}

bool ReportPage::remove(ReportItem* pItem)
{
    int index = find(pItem);
    if (!isExist(index))
        return false;
    delete mItems[index];
    mItems.remove(index);
    return true;
}

bool ReportPage::swap(int iFirst, int iSecond)
{
    if (!isExist(iFirst) || !isExist(iSecond))
        return false;
    mItems.swapItemsAt(iFirst, iSecond);
    return true;
}

int ReportPage::find(ReportItem* pItem) const
{
    int numItems = mItems.size();
    for (int i = 0; i != numItems; ++i)
    {
        if (mItems[i] == pItem)
            return i;
    }
    return -1;
}

void ReportPage::clear()
{
    int numItems = mItems.size();
    for (int i = 0; i != numItems; ++i)
        delete mItems[i];
    mItems.clear();
}

QJsonObject ReportPage::toJson() const
{
    QJsonObject obj;
    obj["layout"] = Utility::toJson(layout);
    obj["name"] = name;
    QJsonArray jsonItems;
    for (auto const& it : mItems)
        jsonItems.push_back(it->toJson());
    obj["items"] = jsonItems;
    return obj;
}

void ReportPage::fromJson(QJsonObject const& obj)
{
    Utility::fromJson(layout, obj["layout"]);
    name = obj["name"].toString();
    QJsonArray jsonItems = obj["items"].toArray();
    int numItems = jsonItems.count();
    mItems.resize(numItems);
    for (int i = 0; i != numItems; ++i)
    {
        QJsonObject jsonItem = jsonItems[i].toObject();
        auto type = (ReportItem::Type) jsonItem["type"].toInt();
        ReportItem* pItem = createItem(type);
        pItem->fromJson(jsonItem);
        mItems[i] = pItem;
    }
}

ReportDocument::ReportDocument()
{
}

ReportDocument::ReportDocument(ReportDocument const& another)
{
    *this = another;
}

ReportDocument::~ReportDocument()
{
    clear();
}

ReportDocument& ReportDocument::operator=(ReportDocument const& another)
{
    clear();
    name = another.name;
    textEngine = another.textEngine;
    int numPages = another.mPages.size();
    mPages.resize(numPages);
    for (int i = 0; i != numPages; ++i)
        mPages[i] = new ReportPage(*another.mPages[i]);
    return *this;
}

bool ReportDocument::isEmpty() const
{
    return mPages.isEmpty();
}

int ReportDocument::count() const
{
    return mPages.count();
}

ReportPage& ReportDocument::get(int index)
{
    return *mPages[index];
}

ReportPage* ReportDocument::add()
{
    ReportPage* pPage = new ReportPage;
    mPages.push_back(pPage);
    return pPage;
}

void ReportDocument::add(ReportPage const& page)
{
    mPages.push_back(new ReportPage(page));
}

bool ReportDocument::remove(int index)
{
    if (index >= 0 && index < mPages.size())
    {
        ReportPage* pPage = mPages[index];
        mPages.remove(index);
        delete pPage;
        return true;
    }
    return false;
}

void ReportDocument::swap(int iFirst, int iSecond)
{
    int numPages = mPages.size();
    bool isFirst = iFirst >= 0 && iFirst < numPages;
    bool isSecond = iSecond >= 0 && iSecond < numPages;
    if (isFirst && isSecond)
        mPages.swapItemsAt(iFirst, iSecond);
}

void ReportDocument::clear()
{
    while (!isEmpty())
        remove(0);
}

QString ReportDocument::fileVersion()
{
    return VERSION_FULL;
}

QString ReportDocument::fileSuffix()
{
    return "json";
}

QJsonObject ReportDocument::toJson() const
{
    QJsonObject obj;
    obj["version"] = fileVersion();
    obj["name"] = name;
    obj["textEngine"] = textEngine.toJson();
    QJsonArray jsonPages;
    for (ReportPage* pPage : mPages)
        jsonPages.push_back(pPage->toJson());
    obj["pages"] = jsonPages;
    return obj;
}

void ReportDocument::fromJson(QJsonObject const& obj)
{
    clear();
    name = obj["name"].toString();
    textEngine.fromJson(obj["textEngine"].toObject());
    QJsonArray jsonPages = obj["pages"].toArray();
    int numPages = jsonPages.size();
    for (int i = 0; i != numPages; ++i)
    {
        ReportPage* pPage = add();
        pPage->fromJson(jsonPages[i].toObject());
    }
}

//! Read a document layout from a file
bool ReportDocument::read(QString const& pathFile)
{
    // Open file for reading
    auto pFile = Utility::openFile(pathFile, fileSuffix(), QIODevice::ReadOnly);
    if (!pFile)
        return false;

    // Read the file
    QByteArray jsonData = pFile->readAll();
    pFile->close();

    // Parse the JSON data
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    // Check for errors
    if (parseError.error != QJsonParseError::NoError)
    {
        qWarning() << QObject::tr("JSON parse error at offset %1:%2").arg(parseError.offset).arg(parseError.errorString());
        return false;
    }
    if (jsonDoc.isNull())
    {
        qWarning() << QObject::tr("Failed to read the document");
        return false;
    }

    // Parse the object
    fromJson(jsonDoc.object());
    qInfo() << QObject::tr("Document has been read from the file %1").arg(pathFile);

    return true;
}

//! Write a document layout to a file
bool ReportDocument::write(QString const& pathFile) const
{
    // Open file for writing
    auto pFile = Utility::openFile(pathFile, fileSuffix(), QIODevice::WriteOnly);
    if (!pFile)
        return false;

    // Write the document
    QJsonDocument jsonDoc(toJson());
    pFile->write(jsonDoc.toJson());
    pFile->close();
    qInfo() << QObject::tr("Document has been written to the file %1").arg(pathFile);

    return true;
}

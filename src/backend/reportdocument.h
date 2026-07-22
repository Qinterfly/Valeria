#ifndef REPORTDOCUMENT_H
#define REPORTDOCUMENT_H

#include <QList>
#include <QPageLayout>

#include "reporttextengine.h"

namespace Backend::Core
{

class ReportItem;
class ReportCurve;

//! Class to define report page layout
class ReportPage : public ISerializable
{
public:
    ReportPage();
    ReportPage(QString const& uName);
    ReportPage(ReportPage const& another);
    ~ReportPage();

    ReportPage& operator=(ReportPage const& another);

    int count() const;
    bool isExist(int index) const;
    ReportItem* get(int index);
    ReportItem const* get(int index) const;
    ReportItem* get(QString const& name);
    ReportItem* get(QUuid const& id);
    void add(ReportItem* pItem);
    bool remove(ReportItem* pItem);
    bool swap(int iFirst, int iSecond);
    int find(ReportItem* pItem) const;
    void clear();

    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;

public:
    QPageLayout layout;
    QString name;

private:
    QList<ReportItem*> mItems;
};

//! Collection of report pages
class ReportDocument : public ISerializable
{
public:
    ReportDocument();
    ReportDocument(ReportDocument const& another);
    ~ReportDocument();

    ReportDocument& operator=(ReportDocument const& another);

    bool isEmpty() const;
    int count() const;
    ReportPage& get(int index);

    ReportPage* add();
    void add(ReportPage const& page);
    bool remove(int index);
    void swap(int iFirst, int iSecond);
    void clear();

    static QString fileVersion();
    static QString fileSuffix();
    QJsonObject toJson() const override;
    void fromJson(QJsonObject const& obj) override;
    bool read(QString const& pathFile);
    bool write(QString const& pathFile) const;

public:
    QString name;
    ReportTextEngine textEngine;

private:
    QList<ReportPage*> mPages;
};
}

#endif // REPORTDOCUMENT_H

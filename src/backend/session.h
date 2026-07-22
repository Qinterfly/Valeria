#ifndef SESSION_H
#define SESSION_H

#include <QList>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QTextStream)

namespace Testlab
{
class IProject;
struct Geometry;
struct Response;
}

namespace Backend::Core
{

using Responses = std::vector<Testlab::Response>;

class ResponseBundle
{
public:
    ResponseBundle();
    ResponseBundle(QString const& uName, Responses const& uResponses);
    ~ResponseBundle() = default;

    bool isEmpty() const;
    int size() const;
    Testlab::Response get(int index) const;

    void merge(Responses const& uResponses);

    static QString fileSuffix();
    bool read(QTextStream& stream);
    bool write(QTextStream& stream) const;
    bool read(QString const& pathFile);
    bool write(QString const& pathFile) const;
    void parseNameIntoProperties();

public:
    QString name;
    double freq;
    double force;
    QString refPoint;
    bool isInverse;

private:
    Responses mResponses;
};

class ResponseCollection
{
public:
    ResponseCollection();
    ~ResponseCollection() = default;

    bool isEmpty() const;
    bool isExist(int index) const;
    int count() const;
    ResponseBundle& get(int index);
    ResponseBundle const& get(int index) const;
    ResponseBundle& add(Responses const& responses, QString const& name);
    void add(ResponseBundle const& bundle);
    bool merge(int index, Responses const& responses);
    bool remove(int index);
    void clear();
    bool swap(int iFirst, int iSecond);

private:
    QList<ResponseBundle> mBundles;
};

class Session
{
public:
    Session();
    ~Session();

    bool isProjectValid();

    bool openProject(QString const& pathFile);
    void closeProject();
    Testlab::Geometry getGeometry();
    Responses getResponses(QStringList const& paths);
    Responses getSelectedResponses();

private:
    Testlab::IProject* mpProject;
};

}

#endif // SESSION_H

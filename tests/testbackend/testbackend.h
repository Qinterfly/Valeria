
#ifndef TESTBACKEND_H
#define TESTBACKEND_H

#include <QTest>

namespace Tests
{

class TestBackend : public QObject
{
    Q_OBJECT

public:
    TestBackend();
    virtual ~TestBackend() = default;

private slots:
    void placeRects();
};
}

#endif // TESTBACKEND_H

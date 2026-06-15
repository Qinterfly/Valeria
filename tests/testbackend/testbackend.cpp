#include "testbackend.h"
#include "mathutility.h"

using namespace Tests;
using namespace Backend;
using namespace Backend::Core;

TestBackend::TestBackend()
{
}

//! Test placing rectangles so they do not overlap
void TestBackend::placeRects()
{
    QRectF bounds(0, 0, 10, 10);
    QVector<QRectF> rects{QRectF(0, 2, 2, 2), QRectF(1, 1, 2, 2), QRectF(3, 5, 4, 2),
                          QRectF(6, 4, 5, 4), QRectF(9, 1, 2, 2), QRectF(3, 9, 2, 2)};
    QVERIFY(Utility::resolveOverlaps(rects, bounds));
}

QTEST_MAIN(TestBackend)

#ifndef REPORTDEFAULTS_H
#define REPORTDEFAULTS_H

#include <QList>

namespace Testlab
{
struct Response;
}

namespace Backend::Core
{

class ReportCurve;
class ReportDocument;
class ReportPage;
class ReportTextEngine;

class ReportDefaults
{
public:
    ReportDefaults() = delete;

    static QList<ReportCurve> curves();
    static ReportTextEngine textEngine();
    static ReportDocument document();
    static ReportPage imRePage();
    static ReportPage multiImRePage();
    static ReportPage freqAmpPage();
    static ReportPage projModeYPage();
    static ReportPage mode3DPage();
    static ReportPage diagramPage();
    static Testlab::Response response();
};

}

#endif // REPORTDEFAULTS_H

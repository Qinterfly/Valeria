#ifndef REPORTDEFAULTS_H
#define REPORTDEFAULTS_H

#include <QList>

namespace Backend::Core
{

class ReportCurve;
class ReportDocument;
class ReportPage;

class ReportDefaults
{
public:
    ReportDefaults() = delete;

    static QList<ReportCurve> curves();
    static ReportDocument document();
    static ReportPage imRePage();
    static ReportPage multiImRePage();
    static ReportPage freqAmpPage();
    static ReportPage projModeYPage();
    static ReportPage mode3DPage();
    static ReportPage diagramPage();
};

}

#endif // REPORTDEFAULTS_H

#ifndef UICONSTANTS_H
#define UICONSTANTS_H

#include <QIcon>
#include <QString>

namespace Frontend::Constants
{

namespace Settings
{

const QString skLanguage = "language";
const QString skGeometry = "geometry";
const QString skState = "state";
const QString skDockingState = "dockingState";
const QString skRecent = "recent";
const QString skFileName = "Settings.ini";
const QString skMainWindow = "mainWindow";
const QString skLastPathFile = "lastPathFile";
}

namespace Color
{

QList<QColor> const skStandardSet = {"red",    "green", "blue",      "orange", "cyan",      "magenta",  "gray",
                                     "purple", "brown", "chocolate", "olive",  "steelblue", "firebrick"};
}

namespace Size
{

QSize const skToolBarIcon = QSize(25, 25);
uint const skMaxRecentProjects = 5;
}

namespace Symbol
{
const QChar skDeg = QChar(0x00b0);
const QChar skPow2 = QChar(0x00B2);
}
}

#endif // UICONSTANTS_H

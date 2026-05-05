
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport)
find_package(VTK REQUIRED COMPONENTS
    CommonColor
    CommonCore
    CommonDataModel
    InfovisLayout
    FiltersHybrid
    FiltersSources
    FiltersGeometry
    InteractionStyle
    InteractionWidgets
    RenderingAnnotation
    RenderingContextOpenGL2
    RenderingCore
    RenderingFreeType
    RenderingGL2PSOpenGL2
    RenderingOpenGL2
    GUISupportQt
    RenderingQt
    ViewsCore
    ViewsInfovis
    IOImage
)

set(FRONTEND_HEADERS
    uiconstants.h
    uialiasdata.h
    uiutility.h
    mainwindow.h
    customstatusbar.h
    customtable.h
    customtabwidget.h
    customlineedit.h
    customplot.h
    sessioneditor.h
    reportworkspace.h
    reportdesigner.h
    geometryview.h
    reportsceneitem.h
    reportpropertyeditor.h
    reportdataeditor.h
    graphreportsceneitem.h
    modereportsceneitem.h
    diagramreportsceneitem.h
)

set(FRONTEND_SOURCES
    uiutility.cpp
    mainwindow.cpp
    customstatusbar.cpp
    customtable.cpp
    customtabwidget.cpp
    customlineedit.cpp
    customplot.cpp
    sessioneditor.cpp
    reportworkspace.cpp
    reportdesigner.cpp
    geometryview.cpp
    reportsceneitem.cpp
    reportpropertyeditor.cpp
    reportdataeditor.cpp
    graphreportsceneitem.cpp
    modereportsceneitem.cpp
    diagramreportsceneitem.cpp
)

qt_add_library(frontend STATIC
    ${FRONTEND_HEADERS}
    ${FRONTEND_SOURCES}
    ${RESOURCES_FILE}
)

target_link_libraries(frontend PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Svg
    Qt::PrintSupport
    ${VTK_LIBRARIES}
    qtpropertybrowser
    qcustomplot
    backend
)

target_include_directories(frontend PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(frontend INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

vtk_module_autoinit(
    TARGETS frontend
    MODULES ${VTK_LIBRARIES}
)

qt_add_translations(frontend
    TS_FILES "${TRANSLATIONS_DIR}/application_ru.ts" "${TRANSLATIONS_DIR}/application_en.ts"
    RESOURCE_PREFIX "/translations"
    -no-obsolete
)


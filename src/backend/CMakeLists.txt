
find_package(Qt6 REQUIRED COMPONENTS Core Gui LinguistTools)
find_package(magic_enum CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

set(BACKEND_HEADERS
    fileutility.h
    session.h
    reportitem.h
    reportdocument.h
    reporttextengine.h
    reportdefaults.h
    reportinterface.h
    reportcommon.h
    mathutility.h
    constants.h
)

set(BACKEND_SOURCES
    fileutility.cpp
    session.cpp
    reportitem.cpp
    reportdocument.cpp
    reporttextengine.cpp
    reportdefaults.cpp
    reportcommon.cpp
    mathutility.cpp
)

qt_add_library(backend STATIC
    ${BACKEND_HEADERS}
    ${BACKEND_SOURCES}
)

target_include_directories(backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(backend PUBLIC
    Qt::Core
    Qt::Gui
    magic_enum::magic_enum
    Eigen3::Eigen
    ${TESTLAB_LIB}
)

PROJECT( marble-touch )

INCLUDE_DIRECTORIES(
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_CURRENT_BINARY_DIR}
 ${QT_INCLUDE_DIR}
)
if( QT4_FOUND)
 INCLUDE(${QT_USE_FILE})
else()
INCLUDE_DIRECTORIES(
  ${Qt5Qml_INCLUDE_DIRS}
  ${Qt5Quick_INCLUDE_DIRS}
)
endif()

set( my_SRCS
  main.cpp
 )

FILE (GLOB QML_FILES *.qml activities/*.qml plasma/*.qml harmattan/*.qml)
marble_add_project_resources(${QML_FILES})

qt_add_resources(my_SRCS meego.qrc harmattan/harmattan.qrc)
marble_qt4_automoc( ${my_SRCS} )
add_executable( marble-touch ${my_SRCS} )
target_link_libraries( marble-touch ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTMAIN_LIBRARY} ${QT_QTDECLARATIVE_LIBRARY} marblewidget )
install( TARGETS marble-touch RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )

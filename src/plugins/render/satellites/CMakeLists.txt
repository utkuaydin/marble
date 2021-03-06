PROJECT(SatellitesPlugin)

INCLUDE_DIRECTORIES(
 ${CMAKE_CURRENT_SOURCE_DIR}
 ${CMAKE_CURRENT_BINARY_DIR}
 ${QT_INCLUDE_DIR}
)
if( QT4_FOUND )
  INCLUDE(${QT_USE_FILE})
endif()

set( sgp4_SRCS
 sgp4/sgp4ext.cpp
 sgp4/sgp4unit.cpp
 sgp4/sgp4io.cpp )

set( mex_SRCS
 mex/astrolib.cpp
 mex/attlib.cpp
 mex/planetarySats.cpp )

set( satellites_SRCS
 TrackerPluginModel.cpp
 TrackerPluginItem.cpp

 SatellitesPlugin.cpp
 SatellitesModel.cpp
 SatellitesMSCItem.cpp
 SatellitesTLEItem.cpp
 SatellitesConfigModel.cpp
 SatellitesConfigDialog.cpp
 SatellitesConfigAbstractItem.cpp
 SatellitesConfigNodeItem.cpp
 SatellitesConfigLeafItem.cpp)

qt_wrap_ui( satellites_SRCS SatellitesConfigDialog.ui )

qt_add_resources( satellites_SRCS satellites.qrc )

marble_add_plugin( SatellitesPlugin
 ${satellites_SRCS}
 ${sgp4_SRCS}
 ${mex_SRCS} )


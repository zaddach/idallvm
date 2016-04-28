# This module finds the IDA SDK prerequisites.
# It requires the CMAKE variables IDA_DIRECTORY
# to be set to the IDA binary directory, and
# IDA_SDK_DIRECTORY to the IDA SDK directory.
#
# It will set the variables IDA_LIBRARIES,
# IDA_INCLUDE_DIRECTORIES and IDA_PLUGIN_SUFFIX.

FIND_LIBRARY( 
    IDA_LIB
    NAMES "ida"
    PATHS "${IDA_DIRECTORY}" )

FIND_PATH(
    IDA_HEADER_PATH
    NAMES "ida.hpp"
    PATHS ${IDA_SDK_DIRECTORY} ${IDA_DIRECTORY}
    PATH_SUFFIXES "include" "sdk/include" "idasdk68/include"
    NO_DEFAULT_PATH )

IF( IDA_LIB AND IDA_HEADER_PATH )
    IF( WIN32 )
        ADD_DEFINITIONS( 
            -D__NT__=1
            -DUNICODE
            -DWIN32
            -D__IDP__ )
        SET( IDA_PLUGIN_SUFFIX ".plw" )
    ELSEIF( APPLE )
        ADD_DEFINITIONS( -D__MAC__=1 )
        SET( IDA_PLUGIN_SUFFIX ".pmc" )
    ELSEIF( UNIX )
        ADD_DEFINITIONS( -D__LINUX__=1 )
        SET( IDA_PLUGIN_SUFFIX ".plx" )
    ENDIF( WIN32 )

    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -std=c++11" )

    SET( IDA_LIBRARIES "${IDA_LIB}" )
    SET( IDA_INCLUDE_DIRECTORIES "${IDA_HEADER_PATH}" )

    MESSAGE( STATUS "IDA_LIBRARIES: " ${IDA_LIBRARIES} )
    MESSAGE( STATUS "IDA_INCLUDE_DIRECTORIES: " ${IDA_INCLUDE_DIRECTORIES} ) 
ELSE( IDA_LIB AND IDA_HEADER_PATH )
    MESSAGE( FATAL_ERROR "IDA library and include path not found. Please set the IDA_DIRECTORY and IDA_SDK_DIRECTORY CMAKE variables." )
ENDIF( IDA_LIB AND IDA_HEADER_PATH )

MARK_AS_ADVANCED( IDA_LIB IDA_HEADER_PATH )



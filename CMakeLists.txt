PROJECT( ida-llvm )
CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

SET( CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH} )

FIND_PACKAGE( IDA )
FIND_PACKAGE( LLVM REQUIRED COMPONENTS jit bitreader bitwriter ipo linker engine irreader)
FIND_PACKAGE( Qt4 REQUIRED QtCore )
FIND_PACKAGE( PythonLibs REQUIRED ) 

INCLUDE( ExternalProject )

INCLUDE_DIRECTORIES( ${IDA_INCLUDE_DIRECTORIES} ${LLVM_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS} )
ADD_DEFINITIONS( ${LLVM_DEFINITIONS} -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DQT_NAMESPACE=QT )
LINK_DIRECTORIES( ${LLVM_LIBRARY_DIRS})

# Add configuration for IDA Qt.
# Do not use the FIND_PACKAGE script for this, as it picks up the wrong
# shared library.
EXECUTE_PROCESS( 
    COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_HEADERS 
    OUTPUT_VARIABLE QT_INCLUDE_DIRECTORY )

INCLUDE_DIRECTORIES( ${QT_INCLUDE_DIRECTORY} )
# IDA Pro is compiled with Qt encapsulated in namespace "QT"
ADD_DEFINITIONS( -DQT_NAMESPACE=QT )
MARK_AS_ADVANCED( QT_INCLUDE_DIRECTORY )

# Build qemulib
IF( NOT CMAKE_BUILD_TYPE )
    SET( QEMU_DEBUG )
ELSEIF( ${CMAKE_BUILD_TYPE} EQUAL "Debug" )
        SET (QEMU_DEBUG --enable-debug)
ELSE( )
        SET (QEMU_DEBUG )
ENDIF( )

#First check if the submodule has already been checked out.
#If not, we can do that right here during the CMAKE generation.
IF (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libqemu/configure")
    EXECUTE_PROCESS (
        COMMAND git submodule update --init libqemu
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
ENDIF (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libqemu/configure")

ExternalProject_Add( libqemu
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libqemu
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/libqemu/configure --enable-lib --target-list=arm-lib,i386-lib,x86_64-lib 
            --prefix=${CMAKE_CURRENT_BINARY_DIR}/libqemu --cxx=clang++-3.5 --clang=clang-3.5 --with-llvm-config=${LLVM_CONFIG}  
            --enable-llvm-shared ${QEMU_DEBUG}
        BUILD_COMMAND $(MAKE)
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libqemu
        INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libqemu
        INSTALL_COMMAND make install
)

INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/libqemu/include )

SET( SOURCE_FILES
    "src/plugin_main.cpp"
    "src/ida_util.cpp"
    "src/libqemu.cpp"
    "src/plugin_python.cpp"
    "src/llvm_passes.cpp" )

ADD_LIBRARY( ida-llvm MODULE ${SOURCE_FILES} )
SET_TARGET_PROPERTIES( 
    ida-llvm
    PROPERTIES
    PREFIX ""
    SUFFIX "${IDA_PLUGIN_SUFFIX}"
    OUTPUT_NAME ida-llvm )
ADD_DEPENDENCIES( ida-llvm libqemu )
TARGET_LINK_LIBRARIES( ida-llvm ${IDA_LIBRARIES} ${LLVM_LIBRARIES} ${PYTHON_LIBRARIES} Qt4::QtCore ) 

INSTALL( 
    TARGETS ida-llvm
    DESTINATION "${IDA_DIRECTORY}/plugins" )

INSTALL(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/libqemu/arm-lib/libqemu-arm.so"
          "${CMAKE_CURRENT_BINARY_DIR}/libqemu/i386-lib/libqemu-i386.so"
          "${CMAKE_CURRENT_BINARY_DIR}/libqemu/x86_64-lib/libqemu-x86_64.so"
    DESTINATION "${IDA_DIRECTORY}/plugins" )

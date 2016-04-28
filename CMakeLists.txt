PROJECT( ida-llvm )
CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

SET( CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH} )

FIND_PACKAGE( IDA )
FIND_PACKAGE( LLVM REQUIRED COMPONENTS jit bitreader bitwriter ipo linker engine irreader)

INCLUDE( ExternalProject )

INCLUDE_DIRECTORIES( ${IDA_INCLUDE_DIRECTORIES} ${LLVM_INCLUDE_DIRS} )
ADD_DEFINITIONS( ${LLVM_DEFINITIONS} -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS )
LINK_DIRECTORIES( ${LLVM_LIBRARY_DIRS})

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
            --prefix=${CMAKE_CURRENT_BINARY_DIR}/libqemu --cxx=clang++-3.5 --clang=clang-3.5 --with-llvm-config=${LLVM_CONFIG}  ${QEMU_DEBUG}
        BUILD_COMMAND $(MAKE)
        BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libqemu
        INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/libqemu
        INSTALL_COMMAND make install
)

SET( QEMU_LIB_ARM ${CMAKE_CURRENT_BINARY_DIR}/libqemu/arm-lib/libqemu-arm.so )
SET( QEMU_LIB_I386 ${CMAKE_CURRENT_BINARY_DIR}/libqemu/i386-lib/libqemu-i386.so )

SET( SOURCE_FILES
    "src/plugin_main.cpp" )

ADD_LIBRARY( ida-llvm MODULE ${SOURCE_FILES} )
SET_TARGET_PROPERTIES( 
    ida-llvm
    PROPERTIES
    PREFIX ""
    SUFFIX "${IDA_PLUGIN_SUFFIX}"
    OUTPUT_NAME ida-llvm )
ADD_DEPENDENCIES( ida-llvm libqemu )
TARGET_LINK_LIBRARIES( ida-llvm ${IDA_LIBRARIES} ) 

INSTALL( 
    TARGETS ida-llvm
    DESTINATION "${IDA_DIRECTORY}/plugins" )

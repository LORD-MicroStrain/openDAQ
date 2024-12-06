# On windows, we will assume that the libraries are already installed into a specified library path
if(WIN32)
    if(DEFINED ENV{LIB_PATH})
        set(MICROSTRAIN_LIB_DIR "$ENV{LIB_PATH}" CACHE STRING "Directory where most dependencies are installed")
    else()
        set(MICROSTRAIN_LIB_DIR "C:/lib" CACHE STRING "Directory where most dependencies are installed")
    endif()

    # Detect if this is a x64 or x86 build
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MSCL_X64 ON)
        set(MSCL_ARCH_NAME "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(MSCL_X86 ON)
        set(MSCL_ARCH_NAME "x86")
    endif()

    # Setup the directories so we can find the libraries
    set(Boost_ROOT "${MICROSTRAIN_LIB_DIR}/boost")
    if(MSCL_X64)
        set(OPENSSL_ROOT_DIR "${MICROSTRAIN_LIB_DIR}/openssl/lib64")
        set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/../include64 CACHE INTERNAL "")
    elseif(MSCL_X86)
        set(OPENSSL_ROOT_DIR "${MICROSTRAIN_LIB_DIR}/openssl/lib")
        set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/../include CACHE INTERNAL "")
    endif()
endif()

# On anything that is not Windows, we will build boost from source, and then assume OpenSSL is somewhere logical
if(UNIX)
    include(FetchContent)

    # Download the boost version needed by MSCL. We need to build this seperately because this old version doesn't use CMake to build
    message(STATUS "Fetching boost 1.68.0 for MSCL...")
    set(MSCL_BOOST_TARGET mscl_boost)
    FetchContent_Declare(${MSCL_BOOST_TARGET}
        URL "https://boostorg.jfrog.io/artifactory/main/release/1.68.0/source/boost_1_68_0.tar.gz"
    )
    FetchContent_MakeAvailable(${MSCL_BOOST_TARGET})

    # Build this version of boost for MSCL
    include(ProcessorCount) #PETER PETER
    message(STATUS "Building boost 1.68.0 for MSCL...")
    ProcessorCount(N)
    if(WIN32)
        set(MSCL_BOOST_BOOSTRAP_COMMAND bootstrap.bat)
        set(MSCL_BOOST_B2_COMMAND b2.exe)
    else()
        set(MSCL_BOOST_BOOSTRAP_COMMAND ./bootstrap.sh)
        set(MSCL_BOOST_B2_COMMAND ./b2)
    endif()
    execute_process(
        COMMAND ${MSCL_BOOST_BOOSTRAP_COMMAND}
        WORKING_DIRECTORY "${${MSCL_BOOST_TARGET}_SOURCE_DIR}"
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND ${MSCL_BOOST_B2_COMMAND} -j ${N} variant=release cxxflags="-fPIC"
        #COMMAND ${MSCL_BOOST_B2_COMMAND} -j ${N} variant=release
        WORKING_DIRECTORY "${${MSCL_BOOST_TARGET}_SOURCE_DIR}"
        OUTPUT_QUIET
    )
    execute_process(
        COMMAND ${MSCL_BOOST_B2_COMMAND} install --prefix=${${MSCL_BOOST_TARGET}_BINARY_DIR}
        WORKING_DIRECTORY "${${MSCL_BOOST_TARGET}_SOURCE_DIR}"
        OUTPUT_QUIET
    )

    # Now that we have build boost from source, point MSCL towards the right boost install
    set(Boost_ROOT ${${MSCL_BOOST_TARGET}_BINARY_DIR})
endif()

# Use different targets depending on what type of build we were requested to do
set(BUILD_SHARED_LIBS OFF)
set(MSCL_EXPECT_TARGET MSCL_Static)

# Build MSCL from source
opendaq_dependency(
    NAME                mscl
    REQUIRED_VERSION    v65.0.0
    GIT_REPOSITORY      https://github.com/LORD-MicroStrain/MSCL.git
    GIT_REF             master
    EXPECT_TARGET       ${MSCL_EXPECT_TARGET}
)

if(MSVC)
    target_compile_options(MSCL_Static PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/MD$<$<CONFIG:Debug>:d>>
    )
endif()

# Add some include directories so we can find the right include files
target_include_directories(${MSCL_EXPECT_TARGET} PUBLIC
    ${Boost_ROOT}/include
    ${OPENSSL_INCLUDE_DIR}
    ${mscl_SOURCE_DIR}/MSCL/source
)

# Add some public libraries required to link
find_package(OpenSSL REQUIRED)
find_package(Boost 1.68.0 EXACT REQUIRED COMPONENTS system filesystem)
if(WIN32)
    list(APPEND OPENSSL_LIBRARIES CRYPT32.lib)
endif()
target_link_libraries(${MSCL_EXPECT_TARGET} PUBLIC ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

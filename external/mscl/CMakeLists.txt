include(FetchContent)
include(ProcessorCount)

# Download the boost version needed by MSCL. We need to build this seperately because this old version doesn't use CMake to build
message(STATUS "Fetching boost 1.68.0 for MSCL...")
set(MSCL_BOOST_TARGET mscl_boost)
FetchContent_Declare(${MSCL_BOOST_TARGET}
    URL "https://boostorg.jfrog.io/artifactory/main/release/1.68.0/source/boost_1_68_0.tar.gz"
)
FetchContent_MakeAvailable(${MSCL_BOOST_TARGET})

# Build this version of boost for MSCL
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
    COMMAND ${MSCL_BOOST_B2_COMMAND} -j ${N} variant=release
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

# Use different targets depending on what type of build we were requested to do
if (OPENDAQ_LINK_3RD_PARTY_LIBS_STATICALY)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(OPENSSL_MSVC_STATIC_RT TRUE)
    set(BUILD_SHARED_LIBS OFF)
    set(MSCL_EXPECT_TARGET MSCL_Static)
else()
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(OPENSSL_USE_STATIC_LIBS FALSE)
    set(OPENSSL_MSVC_STATIC_RT FALSE)
    set(BUILD_SHARED_LIBS ON)
    set(MSCL_EXPECT_TARGET MSCL_Shared)
endif()

# Build MSCL from source
opendaq_dependency(
    NAME                mscl
    REQUIRED_VERSION    v65.0.0
    GIT_REPOSITORY      https://github.com/LORD-MicroStrain/MSCL.git
    GIT_REF             master
    EXPECT_TARGET       ${MSCL_EXPECT_TARGET}
)

# Add some include directories so we can find the right include files
target_include_directories(${MSCL_EXPECT_TARGET} PUBLIC
    ${${MSCL_BOOST_TARGET}_BINARY_DIR}/include
    ${mscl_SOURCE_DIR}/MSCL/source
)

# Add some public libraries required to link
find_package(OpenSSL REQUIRED)
find_package(Boost 1.68.0 EXACT REQUIRED COMPONENTS system filesystem)
if(WIN32)
    list(APPEND OPENSSL_LIBRARIES CRYPT32.lib)
endif()
target_link_libraries(${MSCL_EXPECT_TARGET} PUBLIC ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
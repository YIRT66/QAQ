cmake_minimum_required(VERSION 3.21)
project(EvolveMusic VERSION 0.18.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# v0.11.1: Always anchor source paths to the directory containing this
# CMakeLists.txt. This avoids Qt/CMake resolving relative C++ paths against
# build-mingw during a clean configure on some Windows/MinGW setups.
set(EVOLVEMUSIC_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
file(TO_CMAKE_PATH "${EVOLVEMUSIC_SOURCE_ROOT}" EVOLVEMUSIC_SOURCE_ROOT)
message(STATUS "EvolveMusic v0.18.1 OURCRAFT MUSIC BACKEND + EVOLVE SOCIAL CLOUD: ACTIVE")
message(STATUS "EvolveMusic source root: ${EVOLVEMUSIC_SOURCE_ROOT}")
message(STATUS "EvolveMusic binary root: ${CMAKE_CURRENT_BINARY_DIR}")

if(EVOLVEMUSIC_SOURCE_ROOT STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR
        "EvolveMusic source and build directories must be different. "
        "Source: ${EVOLVEMUSIC_SOURCE_ROOT}")
endif()

# The user's current kit is Qt 6.10 MinGW. Qt 6.10 is also the first release
# that provides the native Windows WebView2 backend in Qt WebView.
find_package(Qt6 6.10 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Network Widgets)
find_package(Qt6 6.10 QUIET OPTIONAL_COMPONENTS WebView)
qt_standard_project_setup(REQUIRES 6.10)

# MinGW 13.1 on this Windows/Qt 6.10 setup has repeatedly ICE'd while
# compiling qmlcachegen-generated *_qml.cpp files from the large EvolveUI
# component set. Qt 6.8.1+ officially supports QT_QML_NO_CACHEGEN, which
# keeps the raw QML resources but skips build-time QML -> C++/bytecode
# generation. This makes the build much smaller and avoids that GCC failure.
option(EVOLVE_ENABLE_QML_CACHEGEN "Enable build-time QML cachegen/AOT" OFF)
if(EVOLVE_ENABLE_QML_CACHEGEN)
    message(STATUS "EvolveMusic QML cachegen: ENABLED")
else()
    set(QT_QML_NO_CACHEGEN TRUE)
    message(STATUS "EvolveMusic QML cachegen: DISABLED (MinGW stability mode)")
endif()

set(EVOLVEUI_DIR "${EVOLVEMUSIC_SOURCE_ROOT}/third_party/EvolveUI")
if(NOT EXISTS "${EVOLVEUI_DIR}/components/ETheme.qml")
    message(FATAL_ERROR "EvolveUI is missing. Run scripts/setup_evolveui.bat first.")
endif()

file(GLOB EVOLVEUI_QML CONFIGURE_DEPENDS RELATIVE ${EVOLVEMUSIC_SOURCE_ROOT}
    "third_party/EvolveUI/components/*.qml")
file(GLOB APP_COMPONENTS CONFIGURE_DEPENDS RELATIVE ${EVOLVEMUSIC_SOURCE_ROOT}
    "components/*.qml")
list(REMOVE_ITEM APP_COMPONENTS "components/WebPlaybackHost.qml")
file(GLOB APP_PAGES CONFIGURE_DEPENDS RELATIVE ${EVOLVEMUSIC_SOURCE_ROOT}
    "pages/*.qml")

# The real embedded-login page imports QtWebView. Keep a fallback page in builds
# where the optional Qt WebView module has not been installed into this kit.
list(REMOVE_ITEM APP_PAGES "pages/LoginBrowserPage.qml")
if(TARGET Qt6::WebView)
    list(APPEND APP_PAGES "pages/LoginBrowserPage.qml")
    list(APPEND APP_COMPONENTS "components/WebPlaybackHost.qml")
    set(EVOLVEMUSIC_WEBVIEW ON)
    message(STATUS "EvolveMusic embedded login browser: ENABLED (Qt WebView / WebView2)")
else()
    set(EVOLVEMUSIC_WEBVIEW OFF)
    message(WARNING "Qt WebView not found. Music playback builds normally, but embedded web login is disabled.")
endif()

set(EVOLVEMUSIC_CPP_SOURCES
    "${EVOLVEMUSIC_SOURCE_ROOT}/main.cpp"

    "${EVOLVEMUSIC_SOURCE_ROOT}/core/AppController.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/AppController.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/PlayerController.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/PlayerController.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/MultiSourceManager.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/MultiSourceManager.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/AccountManager.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/AccountManager.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/LocalProfileManager.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/LocalProfileManager.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/CloudPolicyClient.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/CloudPolicyClient.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/CloudMusicClient.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/CloudMusicClient.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/CachedNetworkAccessManagerFactory.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/CachedNetworkAccessManagerFactory.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/DesktopIntegration.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/DesktopIntegration.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/RuntimeDiagnostics.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/core/RuntimeDiagnostics.cpp"

    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/IMusicProvider.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/NeteaseProvider.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/NeteaseProvider.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/OurcraftProvider.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/OurcraftProvider.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/JsonGatewayProvider.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/JsonGatewayProvider.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/WebProviderUtils.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/WebProviderUtils.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/YuetingProvider.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/YuetingProvider.cpp"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/GequhaiProvider.h"
    "${EVOLVEMUSIC_SOURCE_ROOT}/providers/GequhaiProvider.cpp"
)

# Fail early with the real expected path instead of letting qt_add_executable
# reinterpret a relative path inside the build directory.
foreach(_src IN LISTS EVOLVEMUSIC_CPP_SOURCES)
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "Required EvolveMusic source file is missing: ${_src}")
    endif()
endforeach()

if(WIN32)
    enable_language(RC)
endif()

qt_add_executable(EvolveMusic
    ${EVOLVEMUSIC_CPP_SOURCES}
)

if(WIN32)
    target_sources(EvolveMusic PRIVATE
        "${EVOLVEMUSIC_SOURCE_ROOT}/installer/app.rc"
    )
endif()

qt_add_qml_module(EvolveMusic
    URI EvolveMusic
    VERSION 1.0
    QML_FILES
        Main.qml
        ${APP_COMPONENTS}
        ${APP_PAGES}
        ${EVOLVEUI_QML}
)

if(EXISTS "${EVOLVEUI_DIR}/src.qrc")
    qt_add_resources(EVOLVEUI_RESOURCES "${EVOLVEUI_DIR}/src.qrc")
    target_sources(EvolveMusic PRIVATE ${EVOLVEUI_RESOURCES})
endif()

target_link_libraries(EvolveMusic PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    Qt6::Widgets
)

if(EVOLVEMUSIC_WEBVIEW)
    target_link_libraries(EvolveMusic PRIVATE Qt6::WebView)
    target_compile_definitions(EvolveMusic PRIVATE EVOLVEMUSIC_HAS_WEBVIEW=1)
endif()

if(WIN32)
    # Used only to encrypt saved provider sessions with the current Windows
    # account through DPAPI. No password/cookie is stored in plaintext by us.
    target_link_libraries(EvolveMusic PRIVATE crypt32 dbghelp)
endif()

set_target_properties(EvolveMusic PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

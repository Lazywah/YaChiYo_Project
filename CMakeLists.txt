cmake_minimum_required(VERSION 3.16)

project(YaChiYo_Project VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

# ZH: Multimedia 為可選元件，未安裝時音效模組自動降級為無聲 (見 petsound.cpp)
# EN: Multimedia is optional; PetSound degrades to silent when it is absent (see petsound.cpp)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Multimedia)

set(PROJECT_SOURCES
        src/main.cpp
        src/core/mainwindow.cpp
        src/core/mainwindow.h
        src/core/mainwindow.ui
        src/ui/settingscenter.h
        src/ui/settingscenter.cpp
        src/ui/settingscenter.ui
        src/modules/petphysics.h
        src/modules/petphysics.cpp
        src/modules/petbehavior.h
        src/modules/petbehavior.cpp
        src/modules/aiclient.h
        src/modules/aiclient.cpp
        src/modules/petsound.h
        src/modules/petsound.cpp
        src/modules/petsettings.h
        src/modules/petsettings.cpp
        src/modules/petautostart.h
        src/modules/petautostart.cpp
        src/modules/petskin.h
        src/modules/petskin.cpp
        resources/resources.qrc
)

# ZH: Windows 專屬資源檔（執行檔圖示 + 版本資訊）| EN: Windows-only resource (exe icon + version info)
if(WIN32)
    list(APPEND PROJECT_SOURCES resources/app.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(YaChiYo_Project
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET YaChiYo_Project APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(YaChiYo_Project SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(YaChiYo_Project
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# ZH: 各模組目錄加入 include 路徑，原始碼可直接 #include "xxx.h" 不需相對路徑
# EN: Add module directories to include path so sources can #include "xxx.h" directly
target_include_directories(YaChiYo_Project PRIVATE
    src/core
    src/modules
    src/ui
)

target_link_libraries(YaChiYo_Project PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

# ZH: 找得到 Multimedia 才連結並定義巨集，啟用實際音效 | EN: Link Multimedia and enable real audio only if found
if(TARGET Qt${QT_VERSION_MAJOR}::Multimedia)
    target_link_libraries(YaChiYo_Project PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
    target_compile_definitions(YaChiYo_Project PRIVATE YACHIYO_HAS_MULTIMEDIA)
    message(STATUS "YaChiYo: Qt Multimedia found — sound enabled")
else()
    message(STATUS "YaChiYo: Qt Multimedia NOT found — sound disabled (install qt6-multimedia to enable)")
endif()

# ===== Live2D Cubism (可選 / optional) =====
# ZH: 偵測到 SDK 才整合，找不到時桌寵照常建置 (Live2D 停用)。
#     MinGW 不能用 Core 的 MSVC 靜態庫，改直連 C-API 的 Live2DCubismCore.dll；GLEW 用 msys64 套件。
# EN: Integrate only if the SDK is present. MinGW can't use the MSVC core libs, so we link the
#     C-API Live2DCubismCore.dll directly; GLEW comes from the msys64 package.
set(CUBISM_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CubismSdkForNative-5-r.5")
if(WIN32 AND EXISTS "${CUBISM_SDK_DIR}/Core/include/Live2DCubismCore.h")
    set(CUBISM_CORE_DIR      "${CUBISM_SDK_DIR}/Core")
    set(CUBISM_FRAMEWORK_DIR "${CUBISM_SDK_DIR}/Framework")
    set(CUBISM_CORE_DLL      "${CUBISM_CORE_DIR}/dll/windows/x86_64/Live2DCubismCore.dll")

    # ZH: Framework 由原始碼編成靜態庫，選用 OpenGL 渲染後端 | EN: Framework built from source, OpenGL backend
    set(FRAMEWORK_SOURCE OpenGL)
    add_subdirectory("${CUBISM_FRAMEWORK_DIR}" "${CMAKE_BINARY_DIR}/CubismFramework")

    # ZH: Framework 非 Qt 目標，關掉 AUTOMOC/UIC/RCC 避免誤掃 | EN: not a Qt target — disable autogen
    set_target_properties(Framework PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

    target_compile_definitions(Framework PUBLIC CSM_TARGET_WIN_GL)
    # ZH: Framework 為 MSVC 取向的第三方碼，MinGW/g++ 較嚴格 (如 PROC→void*)，放行這類轉換
    # EN: Framework is MSVC-oriented; g++ is stricter (e.g. PROC→void*) — permit such conversions
    target_compile_options(Framework PRIVATE -fpermissive)
    target_include_directories(Framework PUBLIC "${CUBISM_CORE_DIR}/include")
    # ZH: 直連 Core DLL (已驗證 MinGW 可連) + msys64 GLEW + 系統 OpenGL
    target_link_libraries(Framework "${CUBISM_CORE_DLL}" glew32 opengl32)

    # ZH: Live2D 渲染面需要 Qt OpenGLWidgets | EN: the Live2D surface needs Qt OpenGLWidgets
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS OpenGLWidgets)

    # ZH: 只有啟用 Live2D 時才編譯渲染面原始碼 | EN: compile the Live2D surface only when enabled
    target_sources(YaChiYo_Project PRIVATE
        src/live2d/live2dwidget.h
        src/live2d/live2dwidget.cpp
    )
    target_include_directories(YaChiYo_Project PRIVATE src/live2d)

    target_link_libraries(YaChiYo_Project PRIVATE Framework Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
    target_compile_definitions(YaChiYo_Project PRIVATE YACHIYO_HAS_LIVE2D)
    message(STATUS "YaChiYo: Live2D Cubism SDK found — Live2D enabled")
else()
    message(STATUS "YaChiYo: Live2D Cubism SDK not found — Live2D disabled")
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.YaChiYo_Project)
endif()
set_target_properties(YaChiYo_Project PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS YaChiYo_Project
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(YaChiYo_Project)
endif()

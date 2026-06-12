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

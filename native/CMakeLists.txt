cmake_minimum_required(VERSION 3.24)
project(awakened-poe-trade-native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(CTest)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 6.6 REQUIRED COMPONENTS
  Core Gui Widgets Network WebEngineCore WebEngineWidgets DBus Concurrent
)
find_package(KF6 6.0 REQUIRED COMPONENTS
  GlobalAccel WindowSystem
)
find_package(LayerShellQt REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Tesseract REQUIRED IMPORTED_TARGET tesseract)

if(BUILD_TESTING)
  find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
endif()

set(APT_RENDERER_DEFAULT_PATH "" CACHE PATH "Default path to renderer/dist")
set(APT_NATIVE_VERSION "${PROJECT_VERSION}" CACHE STRING
  "Application version reported to the unchanged upstream renderer")
set(APT_UPDATE_REPOSITORY "" CACHE STRING
  "GitHub owner/repository containing native AppImage releases")

add_executable(awakened-poe-trade-native
  src/main.cpp
  src/Types.h
  src/Logger.h src/Logger.cpp
  src/DesktopLaunch.h src/DesktopLaunch.cpp
  src/ConfigStore.h src/ConfigStore.cpp
  src/EventServer.h src/EventServer.cpp
  src/OverlayWindow.h src/OverlayWindow.cpp
  src/ShortcutManager.h src/ShortcutManager.cpp
  src/InputInjector.h src/InputInjector.cpp
  src/InputEventMonitor.h src/InputEventMonitor.cpp
  src/ScreenshotService.h src/ScreenshotService.cpp
  src/HeistOcrService.h src/HeistOcrService.cpp
  src/UpdateService.h src/UpdateService.cpp
  src/ClipboardService.h src/ClipboardService.cpp
  src/GameConfigReader.h src/GameConfigReader.cpp
  src/GameLogWatcher.h src/GameLogWatcher.cpp
  src/GameWindowTracker.h src/GameWindowTracker.cpp
  src/AppTray.h src/AppTray.cpp
  src/NativeHost.h src/NativeHost.cpp
)

target_compile_definitions(awakened-poe-trade-native PRIVATE
  APT_NATIVE_VERSION="${APT_NATIVE_VERSION}"
  APT_UPDATE_REPOSITORY="${APT_UPDATE_REPOSITORY}"
  APT_RENDERER_DEFAULT_PATH="${APT_RENDERER_DEFAULT_PATH}"
  QT_NO_CAST_FROM_ASCII
  QT_NO_CAST_TO_ASCII
)

set(APT_UPSTREAM_ICON
  "${CMAKE_CURRENT_SOURCE_DIR}/../main/build/icons/128x128.png")
if(EXISTS "${APT_UPSTREAM_ICON}")
  qt_add_resources(awakened-poe-trade-native "apt_native_icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../main/build/icons"
    FILES "${APT_UPSTREAM_ICON}"
  )
  target_compile_definitions(awakened-poe-trade-native PRIVATE
    APT_HAS_APP_ICON=1
  )
endif()

target_compile_options(awakened-poe-trade-native PRIVATE
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

target_link_libraries(awakened-poe-trade-native PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
  Qt6::WebEngineCore Qt6::WebEngineWidgets Qt6::DBus Qt6::Concurrent
  KF6::GlobalAccel
  KF6::WindowSystem
  LayerShellQt::Interface
)

target_compile_definitions(awakened-poe-trade-native PRIVATE
  APT_HAS_NATIVE_OCR=1
)
target_link_libraries(awakened-poe-trade-native PRIVATE
  ${OpenCV_LIBS}
  PkgConfig::Tesseract
)
target_include_directories(awakened-poe-trade-native PRIVATE
  ${OpenCV_INCLUDE_DIRS}
)
message(STATUS "Native Heist OCR enabled")

if(BUILD_TESTING)
  add_executable(awakened-poe-trade-native-tests
    tests/NativeCoreTests.cpp
    src/Types.h
    src/Logger.h src/Logger.cpp
    src/DesktopLaunch.h src/DesktopLaunch.cpp
    src/ConfigStore.h src/ConfigStore.cpp
    src/EventServer.h src/EventServer.cpp
    src/ClipboardService.h src/ClipboardService.cpp
    src/GameConfigReader.h src/GameConfigReader.cpp
    src/GameLogWatcher.h src/GameLogWatcher.cpp
    src/GameWindowTracker.h src/GameWindowTracker.cpp
    src/InputInjector.h src/InputInjector.cpp
    src/UpdateService.h src/UpdateService.cpp
  )
  target_include_directories(awakened-poe-trade-native-tests PRIVATE src)
  target_compile_definitions(awakened-poe-trade-native-tests PRIVATE
    APT_NATIVE_VERSION="${APT_NATIVE_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
  )
  target_compile_options(awakened-poe-trade-native-tests PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
  )
  target_link_libraries(awakened-poe-trade-native-tests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::DBus Qt6::Test
  )
  add_test(NAME native-core COMMAND awakened-poe-trade-native-tests)
  set_tests_properties(native-core PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endif()

install(TARGETS awakened-poe-trade-native
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES resources/awakened-poe-trade-native.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)
install(FILES resources/awakened-poe-trade-native.appdata.xml
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)
if(EXISTS "${APT_UPSTREAM_ICON}")
  install(FILES "${APT_UPSTREAM_ICON}"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps
    RENAME awakened-poe-trade-native.png
  )
endif()
if(APT_RENDERER_DEFAULT_PATH AND EXISTS "${APT_RENDERER_DEFAULT_PATH}/index.html")
  install(DIRECTORY "${APT_RENDERER_DEFAULT_PATH}/"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/awakened-poe-trade-native/renderer
  )
endif()
install(DIRECTORY kwin/awakened-poe-trade-native-focus
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/kwin/scripts
)

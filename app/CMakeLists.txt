cmake_minimum_required(VERSION 3.21)
project(nightstand VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Target is Qt 6.11 (buildroot 2026.08.1); newer Qt works for desktop development.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick Multimedia LinguistTools)

# Source strings are English; German translations live in i18n/nightstand_de.ts
# (refresh with `cmake --build build --target update_translations`).
qt_standard_project_setup(REQUIRES 6.8 I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES de)

qt_add_executable(nightstand
    src/main.cpp
    src/AppSettings.h src/AppSettings.cpp
    src/AbsClient.h src/AbsClient.cpp
    src/Library.h src/Library.cpp
    src/DemoData.h src/DemoData.cpp
    src/Palette.h src/Palette.cpp
    src/CoverProvider.h src/CoverProvider.cpp
    src/PlaybackController.h src/PlaybackController.cpp
    src/DeviceControl.h src/DeviceControl.cpp
    src/PairingServer.h src/PairingServer.cpp
    third_party/qrcodegen/qrcodegen.hpp third_party/qrcodegen/qrcodegen.cpp
    src/audio/AudioBackend.h
    src/audio/QtAudioBackend.h src/audio/QtAudioBackend.cpp
    src/audio/SimulatedBackend.h src/audio/SimulatedBackend.cpp
)

target_compile_definitions(nightstand PRIVATE NIGHTSTAND_VERSION="${PROJECT_VERSION}")
target_include_directories(nightstand PRIVATE src third_party/qrcodegen)

set_source_files_properties(qml/Theme.qml qml/Ui.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(nightstand
    URI Nightstand
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Nightstand
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Ui.qml
        qml/Icon.qml
        qml/Cover.qml
        qml/Shadow.qml
        qml/Background.qml
        qml/PillButton.qml
        qml/CircleButton.qml
        qml/BackPill.qml
        qml/ClockLabel.qml
        qml/SegmentedRow.qml
        qml/CoverFlow.qml
        qml/Keyboard.qml
        qml/InputField.qml
        qml/SetupScreen.qml
        qml/LibraryScreen.qml
        qml/DetailScreen.qml
        qml/PlayerScreen.qml
        qml/ClockScreen.qml
        qml/SettingsScreen.qml
        qml/SidePanel.qml
        qml/VolumeOsd.qml
        qml/ScreenOff.qml
        qml/PairingView.qml
        qml/WifiList.qml
        qml/WifiJoin.qml
        qml/QrCode.qml
        qml/CornerMask.qml
        qml/LetterJump.qml
    RESOURCES
        fonts/InstrumentSans.ttf
        web/index.html
)

qt_add_translations(nightstand
    TS_FILE_DIR i18n
    TS_FILE_BASE nightstand
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none
)

target_link_libraries(nightstand PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::Multimedia)

set_target_properties(nightstand PROPERTIES MACOSX_BUNDLE FALSE)

install(TARGETS nightstand RUNTIME DESTINATION bin)

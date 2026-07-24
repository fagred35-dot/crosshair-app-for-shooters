cmake_minimum_required(VERSION 3.20)

project(AimPoint VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(aimpoint_model STATIC
    src/CrosshairPreset.cpp
)
target_include_directories(aimpoint_model PUBLIC src)

if(MSVC)
    target_compile_options(aimpoint_model PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(aimpoint_model PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    enable_language(RC)

    add_executable(AimPoint WIN32
        src/WinMain.cpp
        src/MainWindow.cpp
        src/OverlayWindow.cpp
        src/CrosshairRenderer.cpp
        src/ClickerEngine.cpp
        src/SettingsStore.cpp
        resources/AimPoint.rc
    )

    target_include_directories(AimPoint PRIVATE src)
    target_compile_definitions(AimPoint PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
        AIMPOINT_VERSION="${PROJECT_VERSION}"
    )
    target_link_libraries(AimPoint PRIVATE
        aimpoint_model
        user32
        gdi32
        shell32
        comctl32
        advapi32
    )

    if(MSVC)
        target_compile_options(AimPoint PRIVATE /W4 /permissive- /utf-8)
        target_link_options(AimPoint PRIVATE /SUBSYSTEM:WINDOWS)
    else()
        target_compile_options(AimPoint PRIVATE -Wall -Wextra -Wpedantic)
        target_link_options(AimPoint PRIVATE -municode)
    endif()

    install(TARGETS AimPoint RUNTIME DESTINATION .)
else()
    message(STATUS "AimPoint GUI targets Windows; building the portable preset model and tests only.")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(preset_catalog_test tests/preset_catalog_test.cpp)
    target_link_libraries(preset_catalog_test PRIVATE aimpoint_model)
    add_test(NAME preset_catalog_test COMMAND preset_catalog_test)
endif()

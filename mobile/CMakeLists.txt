find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Concurrent)
qt_standard_project_setup(REQUIRES 6.8)

add_library(ticket_wallet STATIC
    wallet/phantom_protocol.cpp wallet/phantom_protocol.hpp
    wallet/wallet_controller.cpp wallet/wallet_controller.hpp
    ../third_party/tweetnacl/tweetnacl.c
)
target_include_directories(ticket_wallet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/wallet
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tweetnacl)
target_link_libraries(ticket_wallet PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
set_target_properties(ticket_wallet PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Concurrent)
    target_link_libraries(ticket_wallet PRIVATE Qt6::Concurrent)
endif()

add_library(umit_account STATIC app_settings.cpp app_settings.hpp account_controller.cpp account_controller.hpp app_info.hpp)
target_link_libraries(umit_account PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Network Qt6::Concurrent)
target_include_directories(umit_account PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(umit_account PROPERTIES POSITION_INDEPENDENT_CODE ON)

qt_add_executable(ticket_mobile main.cpp)
option(UMIT_UI_VERIFY "Build desktop-only UI verification hooks" OFF)
if(UMIT_UI_VERIFY AND NOT ANDROID AND NOT IOS)
    target_sources(ticket_mobile PRIVATE ui_verification.cpp)
    target_compile_definitions(ticket_mobile PRIVATE UMIT_UI_VERIFY)
endif()
qt_add_resources(ticket_mobile umit_config PREFIX "/config" FILES app_config.json)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.png")
    qt_add_resources(ticket_mobile umit_logo PREFIX "/branding" BASE assets FILES assets/logo.png)
endif()
if(IOS)
    set_target_properties(ticket_mobile PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")
endif()
if(ANDROID)
    set_target_properties(ticket_mobile PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_CODE 3
        QT_ANDROID_VERSION_NAME "0.3-umit"
        QT_ANDROID_APP_NAME "UMIT"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.png")
        set(umit_android_package "${CMAKE_CURRENT_BINARY_DIR}/umit-android-package")
        file(GLOB_RECURSE umit_android_templates CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/android" "android/*")
        foreach(template IN LISTS umit_android_templates)
            configure_file("android/${template}" "${umit_android_package}/${template}" COPYONLY)
        endforeach()
        configure_file(assets/logo.png "${umit_android_package}/res/drawable/umit_logo.png" COPYONLY)
        set_target_properties(ticket_mobile PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${umit_android_package}"
            QT_ANDROID_APP_ICON "@drawable/umit_logo")
    endif()
endif()
qt_add_qml_module(ticket_mobile
    URI TicketApp
    VERSION 1.0
    QML_FILES Main.qml ui/Strings.js ui/UText.qml ui/UCard.qml ui/UButton.qml ui/HomePage.qml
        ui/TicketsPage.qml ui/ProfilePage.qml ui/SettingsPage.qml ui/SupportPage.qml
    SOURCES ticket_controller.hpp ticket_controller.cpp
)
target_include_directories(ticket_mobile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ticket_mobile PRIVATE
    ticket_core ticket_wallet umit_account Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)
set_target_properties(ticket_mobile PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.example.ticketdemo"
    MACOSX_BUNDLE_BUNDLE_VERSION "3"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.3"
)
if(MSVC)
    target_compile_options(ticket_wallet PRIVATE /utf-8)
    target_compile_options(umit_account PRIVATE /utf-8)
    target_compile_options(ticket_mobile PRIVATE /W4 /utf-8)
else()
    target_compile_options(ticket_mobile PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING AND NOT ANDROID AND NOT IOS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(wallet_tests ../tests/wallet_tests.cpp)
    target_link_libraries(wallet_tests PRIVATE ticket_wallet Qt6::Test)
    target_include_directories(wallet_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tweetnacl)
    add_test(NAME phantom_wallet COMMAND wallet_tests)
    set_tests_properties(phantom_wallet PROPERTIES TIMEOUT 30)
    foreach(test_name IN ITEMS app_settings account)
        qt_add_executable(${test_name}_tests ../tests/${test_name}_tests.cpp)
        target_link_libraries(${test_name}_tests PRIVATE umit_account Qt6::Test)
        add_test(NAME ${test_name} COMMAND ${test_name}_tests)
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 180)
    endforeach()
endif()

## QIMGV TESTS
# qimgv_tests      Qt Test suite for UI-independent components.
# qimgv_qml_tests  Qt Quick Test suite for the QML modules.

# Tests run from the build tree without windeployqt: Qt's bin directory
# provides the DLLs, and QLibraryInfo then resolves the Qt plugin and QML
# import paths from the Qt installation.
set(QIMGV_TEST_QT_BIN_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")

# qimgv_configure_test(<target>)
# Applies the project compiler profile to a test executable and registers it
# with CTest, headless (offscreen platform).
function(qimgv_configure_test target)
    target_compile_features(${target} PRIVATE cxx_std_23)
    if(MSVC)
        # Same C++26 working-draft switch as qimgv-plus.
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>")
    endif()
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qimgv_apply_msvc_profile(${target})
    qimgv_suppress_crt_deprecation(${target})

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${QIMGV_TEST_QT_BIN_DIR};QT_QPA_PLATFORM=set:offscreen"
    )
endfunction()

add_subdirectory(unit)
add_subdirectory(qml)

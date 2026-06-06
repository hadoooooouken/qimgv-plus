include(GNUInstallDirs)

set(CMAKE_VERBOSE_MAKEFILE ON)

# TRANSLATIONS
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS LinguistTools)
set(TS_FILES
    "zh_CN.ts"
    "uk_UA.ts"
    "es_ES.ts"
    "de_DE.ts"
    "fr_FR.ts"
    "tr_TR.ts"
    "ja_JP.ts"
    "ru_RU.ts"
)
list(TRANSFORM TS_FILES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/res/translations/)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION ${PROJECT_BINARY_DIR}/qimgv/translations)

# 1. Create 'qimgv_lupdate' target to update .ts files (needs to be called manually)
# 2. Compile .ts > .qm
if(${QT_VERSION_MAJOR} EQUAL 5)
    add_custom_target(qimgv_lupdate COMMAND ${Qt5_LUPDATE_EXECUTABLE} -recursive
        ${CMAKE_CURRENT_SOURCE_DIR} -ts ${TS_FILES})
    qt5_add_translation(QM_FILES ${TS_FILES})
endif()

# ADD EXECUTABLE
add_executable(qimgv-plus
    appversion.cpp
    core.cpp
    main.cpp
    settings.cpp
    themestore.cpp
    sharedresources.cpp
    shortcutbuilder.cpp
    proxystyle.cpp
    resources.qrc
    qimgv.rc
    ${QM_FILES}
)

# update .ts, compile .qm (qt6 case)
if(${QT_VERSION_MAJOR} EQUAL 6)
    qt_add_lupdate(qimgv-plus TS_FILES ${TS_FILES})
    qt_add_lrelease(qimgv-plus TS_FILES ${TS_FILES})
endif()

# ADD SOURCES
add_subdirectory(components)
add_subdirectory(sourcecontainers)
add_subdirectory(gui)
add_subdirectory(utils)

add_subdirectory(3rdparty/kimageformats)

# C++ STANDARD
target_compile_features(qimgv-plus PRIVATE cxx_std_17)
set_target_properties(qimgv-plus PROPERTIES 
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../release"
)

# LINK STUFF
target_link_libraries(qimgv-plus PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg Qt${QT_VERSION_MAJOR}::PrintSupport Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Sql)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(qimgv-plus PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets Qt${QT_VERSION_MAJOR}::SvgWidgets)
endif()

# OPTION DEFINITIONS, LINKING
if(EXIV2)
    target_link_libraries(qimgv-plus PRIVATE Exiv2::exiv2lib)
    target_compile_definitions(qimgv-plus PRIVATE USE_EXIV2)
endif()


if(UPSCAYL_SUPPORT)
    target_link_libraries(qimgv-plus PRIVATE upscayl-ncnn)
    target_compile_definitions(qimgv-plus PRIVATE USE_UPSCAYL)
endif()


# generate proper GUI program on specified platform
if(MSVC) # Check if we are using the Visual Studio compiler
    set_target_properties(qimgv-plus PROPERTIES WIN32_EXECUTABLE ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(qimgv-plus PRIVATE "-mwindows")
elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS} -Wno-shift-negative-value")
    set(CMAKE_EXE_LINKER_FLAGS "$ENV{CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    target_link_options(qimgv-plus PRIVATE "-mwindows")
else()
    message(SEND_ERROR "You are using an unsupported Windows compiler! (Not MSVC or GCC)")
endif(MSVC)

## INSTALLATION
install(TARGETS qimgv-plus DESTINATION bin)

# Install internationalization language files
install(DIRECTORY "${PROJECT_BINARY_DIR}/qimgv/translations" DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Automate deployment of ALL dependencies on Windows
if(WIN32)
    # 1. Qt dependencies
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6Core_DIR}/../../../bin" "${Qt5Core_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations --no-compiler-runtime --no-opengl-sw "$<TARGET_FILE:qimgv-plus>"
            COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:qimgv-plus>/vc_redist.x64.exe"
            COMMENT "Deploying Qt dependencies and cleaning up redistributable..."
        )
    endif()

    # 2. Exiv2
    if(EXIV2)
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/exiv2/bin/exiv2.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
            COMMENT "Copying Exiv2 DLL..."
        )
    endif()


    # 5. kimageformats dependencies (safe check)
    set(_kimg_plugins ani dds ff hdr iff pcx pic pfm psd pxr qoi ras rgb sct tim tga xcf exr raw avif jxl kra ora jxr heif jp2 jph)
    foreach(_plugin ${_kimg_plugins})
        if(TARGET kimg_${_plugin})
            add_dependencies(qimgv-plus kimg_${_plugin})
            message(STATUS "Linked plugin: kimg_${_plugin}")
        endif()
    endforeach()
    
    # 6. kimageformats plugins deployment
    add_custom_command(TARGET qimgv-plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qimgv-plus>/imageformats"
        COMMENT "Preparing imageformats output directory..."
    )
    # Copy each plugin only if its target was actually built
    set(_kimg_required_plugins ani dds ff hdr iff pcx pic pfm psd pxr qoi ras rgb sct tim tga xcf exr raw kra ora jp2 jph)
    foreach(_plugin ${_kimg_required_plugins})
        if(TARGET kimg_${_plugin})
            add_custom_command(TARGET qimgv-plus POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_${_plugin}>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
                COMMENT "Deploying kimg_${_plugin}..."
            )
        else()
            message(WARNING "Plugin kimg_${_plugin} was not built, skipping deployment")
        endif()
    endforeach()
    # Optional format plugins
    if(TARGET kimg_heif)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_heif>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    if(TARGET kimg_avif)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_avif>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    if(TARGET kimg_jxl)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_jxl>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    if(TARGET kimg_jxr)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_jxr>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()



    # 7. Translation files deployment
    add_custom_command(TARGET qimgv-plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qimgv-plus>/translations"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/qimgv/translations" "$<TARGET_FILE_DIR:qimgv-plus>/translations"
        COMMENT "Deploying translation files..."
    )

    # 8. Copy OpenMP runtime for LibRaw
    set(OPENMP_DLL_PATH "E:/MVSC/VC/Redist/MSVC/14.51.36231/x64/Microsoft.VC145.OpenMP/vcomp140.dll")
    if(EXISTS "${OPENMP_DLL_PATH}")
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPENMP_DLL_PATH}" "$<TARGET_FILE_DIR:qimgv-plus>/"
            COMMENT "Deploying OpenMP runtime (vcomp140.dll)..."
        )
    else()
        message(WARNING "OpenMP runtime NOT found at: ${OPENMP_DLL_PATH}")
    endif()

    # 9. Copy OpenEXR and Imath runtime dependencies for kimg_exr
    set(OPENEXR_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../formats/openexr/install/bin")
    set(IMATH_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../formats/Imath/install/bin")

    set(EXR_DLLS
        "${OPENEXR_BIN_DIR}/OpenEXR-4_0.dll"
        "${OPENEXR_BIN_DIR}/OpenEXRCore-4_0.dll"
        "${OPENEXR_BIN_DIR}/Iex-4_0.dll"
        "${OPENEXR_BIN_DIR}/IlmThread-4_0.dll"
        "${OPENEXR_BIN_DIR}/OpenEXRUtil-4_0.dll"
        "${IMATH_BIN_DIR}/Imath-3_2.dll"
    )

    foreach(_dll ${EXR_DLLS})
        if(EXISTS "${_dll}")
            add_custom_command(TARGET qimgv-plus POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:qimgv-plus>/"
                COMMENT "Deploying OpenEXR/Imath/OpenJPH dependency: ${_dll}..."
            )
        else()
            message(WARNING "EXR dependency DLL NOT found at: ${_dll}")
        endif()
    endforeach()

    # 10. Copy upscayl-ncnn DLL and models directory
    if(UPSCAYL_SUPPORT)
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:upscayl-ncnn>" "$<TARGET_FILE_DIR:qimgv-plus>/"
            COMMENT "Deploying upscayl-ncnn DLL..."
        )
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qimgv-plus>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../upscayl-ncnn/models" "$<TARGET_FILE_DIR:qimgv-plus>/models"
            COMMENT "Deploying Upscayl models directory..."
        )
    endif()
endif()



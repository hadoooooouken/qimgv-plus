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
if(OPENCV_SUPPORT)
    add_subdirectory(3rdparty/QtOpenCV)
endif()
add_subdirectory(3rdparty/kimageformats)

# C++ STANDARD
target_compile_features(qimgv-plus PRIVATE cxx_std_17)
set_target_properties(qimgv-plus PROPERTIES 
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../release"
)

# LINK STUFF
target_link_libraries(qimgv-plus PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg Qt${QT_VERSION_MAJOR}::PrintSupport Qt${QT_VERSION_MAJOR}::Network)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_link_libraries(qimgv-plus PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets Qt${QT_VERSION_MAJOR}::SvgWidgets)
endif()

# OPTION DEFINITIONS, LINKING
if(EXIV2)
    target_link_libraries(qimgv-plus PRIVATE Exiv2::exiv2lib)
    target_compile_definitions(qimgv-plus PRIVATE USE_EXIV2)
endif()
if(OPENCV_SUPPORT)
    target_link_libraries(qimgv-plus PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(qimgv-plus PRIVATE USE_OPENCV)
endif()
if(USE_CUDA_NPP)
    target_link_libraries(qimgv-plus PRIVATE CUDA::nppig CUDA::nppif CUDA::nppisu CUDA::nppc CUDA::cudart)
    target_compile_definitions(qimgv-plus PRIVATE USE_CUDA_NPP)
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
            COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations --no-compiler-runtime "$<TARGET_FILE:qimgv-plus>"
            COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:qimgv-plus>/vc_redist.x64.exe"
            COMMENT "Deploying Qt dependencies and cleaning up redistributable..."
        )
    endif()

    # 2. Exiv2
    if(EXIV2)
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/exiv2-0.28.8-2022msvc-AMD64/bin/exiv2.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
            COMMENT "Copying Exiv2 DLL..."
        )
    endif()

    # 3. OpenCV
    if(OPENCV_SUPPORT)
        add_custom_command(TARGET qimgv-plus POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "E:/opencv/build/x64/vc16/bin/opencv_world4130.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
            COMMENT "Copying OpenCV DLL..."
        )
    endif()

    # 4. CUDA (NPP and Runtime)
    if(USE_CUDA_NPP)
        # Find specific DLLs in CUDA bin directory (look in both bin/ and bin/x64/)
        file(GLOB _cuda_dlls 
            "E:/CUDA/bin/nppc64_*.dll" "E:/CUDA/bin/x64/nppc64_*.dll"
            "E:/CUDA/bin/nppig64_*.dll" "E:/CUDA/bin/x64/nppig64_*.dll"
            "E:/CUDA/bin/nppif64_*.dll" "E:/CUDA/bin/x64/nppif64_*.dll"
            "E:/CUDA/bin/nppisu64_*.dll" "E:/CUDA/bin/x64/nppisu64_*.dll"
            "E:/CUDA/bin/cudart64_*.dll" "E:/CUDA/bin/x64/cudart64_*.dll"
        )
        foreach(_dll ${_cuda_dlls})
            add_custom_command(TARGET qimgv-plus POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:qimgv-plus>/"
            )
        endforeach()
    endif()

    # 5. kimageformats dependencies (safe check)
    set(_kimg_plugins ani dds ff hdr iff pcx pic pfm psd pxr qoi ras rgb sct tim tga xcf exr raw avif heif jxl kra ora jxr)
    foreach(_plugin ${_kimg_plugins})
        if(TARGET kimg_${_plugin})
            add_dependencies(qimgv-plus kimg_${_plugin})
            message(STATUS "Linked plugin: kimg_${_plugin}")
        endif()
    endforeach()
    
    # 6. kimageformats plugins deployment
    add_custom_command(TARGET qimgv-plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qimgv-plus>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_ani>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_dds>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_ff>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_hdr>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_iff>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_pcx>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_pic>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_pfm>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_psd>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_pxr>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_qoi>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_ras>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_rgb>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_sct>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_tim>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
    )
    if(TARGET kimg_tga)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_tga>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    add_custom_command(TARGET qimgv-plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_xcf>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_exr>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_raw>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_kra>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_ora>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/"
        # AVIF, HEIF, JXL plugins (only if they were built)
        COMMAND ${CMAKE_COMMAND} -E echo "Copying kimageformats plugins..."
    )
    if(TARGET kimg_avif)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_avif>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    if(TARGET kimg_heif)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_heif>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    if(TARGET kimg_jxl)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_jxl>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()
    if(TARGET kimg_jxr)
        add_custom_command(TARGET qimgv-plus POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:kimg_jxr>" "$<TARGET_FILE_DIR:qimgv-plus>/imageformats/")
    endif()

    # 6. Codec DLLs for AVIF, HEIF, JXL
    add_custom_command(TARGET qimgv-plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libavif/build/Release/avif.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libheif/build/libheif/Release/heif.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/lib/Release/jxl.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/lib/Release/jxl_threads.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/lib/Release/jxl_cms.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        # Brotli (for JXL)
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/third_party/brotli/Release/brotlidec.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/third_party/brotli/Release/brotlienc.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/third_party/brotli/Release/brotlicommon.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        # OpenEXR & Imath & Zlib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/libjxl/build/third_party/zlib/Release/zlib1.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/openexr/install/bin/OpenEXR-4_0.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/openexr/install/bin/OpenEXRCore-4_0.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/openexr/install/bin/OpenEXRUtil-4_0.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/openexr/install/bin/Iex-4_0.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/openexr/install/bin/IlmThread-4_0.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/Imath/install/bin/Imath-3_2.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        # LibRaw
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../formats/LibRaw/buildfiles/release-x86_64/libraw.dll" "$<TARGET_FILE_DIR:qimgv-plus>/"
        COMMENT "Copying advanced image format codec DLLs..."
    )

    # 7. Translation files deployment
    add_custom_command(TARGET qimgv-plus POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qimgv-plus>/translations"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/qimgv/translations" "$<TARGET_FILE_DIR:qimgv-plus>/translations"
        COMMENT "Deploying translation files..."
    )
endif()

add_library(qimgv_miniz STATIC
    miniz.c
    miniz_zip.c
    miniz_tdef.c
    miniz_tinfl.c
)

target_include_directories(qimgv_miniz PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(qimgv_miniz PRIVATE
        /MD /O2 /Ob2 /Oi /Ot /DNDEBUG /arch:AVX2 /GS /guard:cf /Qspectre
    )
    set_target_properties(qimgv_miniz PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    )
endif()

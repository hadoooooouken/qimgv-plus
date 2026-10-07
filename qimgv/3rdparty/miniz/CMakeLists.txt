add_library(qimgv_miniz STATIC
    miniz.c
    miniz_zip.c
    miniz_tdef.c
    miniz_tinfl.c
)

target_include_directories(qimgv_miniz PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

qimgv_apply_msvc_profile(qimgv_miniz)
qimgv_suppress_crt_deprecation(qimgv_miniz)

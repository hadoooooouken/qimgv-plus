# MSVC build policy for qimgv-plus targets.
#
# The policy is applied explicitly per target through qimgv_apply_msvc_profile()
# instead of being injected into CMAKE_<LANG>_FLAGS / CMAKE_<TYPE>_LINKER_FLAGS,
# so third-party subprojects (ncnn, glslang) keep control of their own ISA and
# IPO strategy and only receive what is requested for them.
#
# Toolchain facts this policy relies on (MSVC 19.51, x64), verified with probe
# builds rather than taken from documentation:
#   - /O2 already implies /Ob2 /Oi /Ot, so they are not repeated.
#   - /GS is on by default.
#   - link.exe sets /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT by default for x64.
#   - /CETCOMPAT is NOT a linker default.
#   - When several /arch switches are present, the last one silently wins.
#   - Linking /guard:ehcont fails (LNK2047 / LNK1386) if any C++ object with EH
#     metadata was compiled without /guard:ehcont, including /GL objects in
#     prebuilt static libraries.

include_guard(GLOBAL)

# Release optimization baseline shared by every target in the build.
set(QIMGV_MSVC_RELEASE_BASELINE_FLAGS "/O2 /DNDEBUG")

# Application ISA baseline: AVX2 is the mandatory minimum CPU for qimgv-plus.
set(QIMGV_MSVC_ISA_OPTIONS /arch:AVX2)

# Compile-side hardening: Control Flow Guard, Spectre v1 mitigation and
# EH continuation metadata (consumed by CET shadow stacks).
set(QIMGV_MSVC_SECURITY_COMPILE_OPTIONS /guard:cf /Qspectre /guard:ehcont)

# Link-side hardening for executables and DLLs.
set(QIMGV_MSVC_SECURITY_LINK_OPTIONS /guard:cf /CETCOMPAT)
set(QIMGV_MSVC_EHCONT_LINK_OPTIONS /guard:ehcont)

# Dynamic CRT, debug flavour only in Debug.
set(QIMGV_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# qimgv_apply_msvc_profile(<target>
#     [NO_ARCH]         Target owns its ISA strategy (e.g. ncnn runtime dispatch).
#     [NO_IPO]          Leave INTERPROCEDURAL_OPTIMIZATION to the target's project.
#     [NO_EHCONT_LINK]  Target links prebuilt C++ code without EH continuation
#                       metadata; compile-side /guard:ehcont is still emitted.
# )
function(qimgv_apply_msvc_profile target)
    if(NOT MSVC)
        return()
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "qimgv_apply_msvc_profile: '${target}' is not a target")
    endif()

    cmake_parse_arguments(PROFILE "NO_ARCH;NO_IPO;NO_EHCONT_LINK" "" "" ${ARGN})
    if(PROFILE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "qimgv_apply_msvc_profile: unknown arguments '${PROFILE_UNPARSED_ARGUMENTS}'")
    endif()

    # Optimization
    if(NOT PROFILE_NO_ARCH)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C,CXX>:${QIMGV_MSVC_ISA_OPTIONS}>")
    endif()
    if(NOT PROFILE_NO_IPO)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()

    # Security
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C,CXX>:${QIMGV_MSVC_SECURITY_COMPILE_OPTIONS}>")

    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "EXECUTABLE" OR _type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "MODULE_LIBRARY")
        target_link_options(${target} PRIVATE ${QIMGV_MSVC_SECURITY_LINK_OPTIONS})
        if(NOT PROFILE_NO_EHCONT_LINK)
            target_link_options(${target} PRIVATE ${QIMGV_MSVC_EHCONT_LINK_OPTIONS})
        endif()
    endif()

    # Runtime
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "${QIMGV_MSVC_RUNTIME_LIBRARY}")
endfunction()

# Silences C4996 CRT deprecation warnings for first-party targets only; it is
# deliberately not a directory-wide definition, because glslang defines the
# same macro without a guard and a global -D turns that into C4005.
function(qimgv_suppress_crt_deprecation target)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endfunction()

enable_language(C)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" PFFFT_SYSTEM_PROCESSOR)

set(PFFFT_USE_SIMD_DEFAULT ON)
if(PFFFT_SYSTEM_PROCESSOR MATCHES "^(ppc|powerpc)")
    set(PFFFT_USE_SIMD_DEFAULT OFF)
endif()

option(PFFFT_USE_SIMD "Use SIMD optimisations in the bundled PFFFT library" ${PFFFT_USE_SIMD_DEFAULT})

add_library(
    fooyin_pffft STATIC
    src/pffft.c
    src/pffft.h
    src/pffft_common.c
    src/pffft_priv_impl.h
    src/simd/pf_altivec_float.h
    src/simd/pf_float.h
    src/simd/pf_neon_float.h
    src/simd/pf_scalar_float.h
    src/simd/pf_sse1_float.h
)
add_library(PFFFT::PFFFT ALIAS fooyin_pffft)

target_include_directories(fooyin_pffft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(
    fooyin_pffft
    PUBLIC PFFFT_STATIC_DEFINE
    PRIVATE PFFFT_SCALVEC_ENABLED=1 _USE_MATH_DEFINES
)

if(PFFFT_USE_SIMD)
    if(PFFFT_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        target_compile_definitions(fooyin_pffft PRIVATE PFFFT_ENABLE_NEON=1)
    endif()
else()
    target_compile_definitions(fooyin_pffft PRIVATE PFFFT_SIMD_DISABLE=1)
endif()

if(NOT MSVC)
    target_link_libraries(fooyin_pffft PRIVATE m)
endif()

set_target_properties(
    fooyin_pffft
    PROPERTIES C_STANDARD 99
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF
               AUTOMOC OFF
               POSITION_INDEPENDENT_CODE ON
               C_VISIBILITY_PRESET hidden
)

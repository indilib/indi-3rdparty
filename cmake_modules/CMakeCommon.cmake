include(CheckCCompilerFlag)

# C++17 Support
if(NOT ANDROID)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Position Independent Code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Ccache support
if(ANDROID OR UNIX OR APPLE)
  find_program(CCACHE_FOUND ccache)
  set(CCACHE_SUPPORT OFF CACHE BOOL "Enable ccache support")
  if((CCACHE_FOUND OR ANDROID) AND CCACHE_SUPPORT MATCHES ON)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  endif()
endif()

# Add security (hardening flags)
if(UNIX OR APPLE OR ANDROID)
  # Older compilers are predefining _FORTIFY_SOURCE, so defining it causes a
  # warning, which is then considered an error. Second issue is that for
  # these compilers, _FORTIFY_SOURCE must be used while optimizing, else
  # causes a warning, which also results in an error. And finally, CMake is
  # not using optimization when testing for libraries, hence breaking the build.
  check_c_compiler_flag("-Werror -D_FORTIFY_SOURCE=2" COMPATIBLE_FORTIFY_SOURCE)
  if(
    COMPATIBLE_FORTIFY_SOURCE
    AND NOT CMAKE_BUILD_TYPE MATCHES "Debug"
  )
    set(SEC_COMP_FLAGS "-D_FORTIFY_SOURCE=2")
  endif()
  set(SEC_COMP_FLAGS "${SEC_COMP_FLAGS} -fstack-protector-all -fPIE")
  # Make sure to add optimization flag. Some systems require this for _FORTIFY_SOURCE.
  if(
    NOT CMAKE_BUILD_TYPE MATCHES "MinSizeRel"
    AND NOT CMAKE_BUILD_TYPE MATCHES "Release"
    AND NOT CMAKE_BUILD_TYPE MATCHES "Debug"
  )
    set(SEC_COMP_FLAGS "${SEC_COMP_FLAGS} -O1")
  endif()
  if(
    NOT ANDROID
    AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
    AND NOT APPLE
    AND NOT CYGWIN
  )
    set(SEC_COMP_FLAGS "${SEC_COMP_FLAGS} -Wa,--noexecstack")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SEC_COMP_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SEC_COMP_FLAGS}")
  set(SEC_LINK_FLAGS "")
  if(NOT APPLE AND NOT CYGWIN)
    set(
      SEC_LINK_FLAGS
      "${SEC_LINK_FLAGS} -Wl,-z,nodump -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now"
    )
  endif()
  if(NOT ANDROID AND NOT APPLE)
    set(SEC_LINK_FLAGS "${SEC_LINK_FLAGS} -pie")
  endif()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SEC_LINK_FLAGS}")
  set(
    CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} ${SEC_LINK_FLAGS} "
  )
endif()

# Warning, debug and linker flags
set(
  FIX_WARNINGS
  OFF
  CACHE BOOL
  "Enable strict compilation mode to turn compiler warnings to errors"
)
if(UNIX OR APPLE)
  set(COMP_FLAGS "")
  set(LINKER_FLAGS "")
  # Verbose warnings and turns all to errors
  set(COMP_FLAGS "${COMP_FLAGS} -Wall -Wextra")
  if(FIX_WARNINGS)
    set(COMP_FLAGS "${COMP_FLAGS} -Werror")
  endif()
  # Omit problematic warnings
  if(
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.9.9
  )
    set(COMP_FLAGS "${COMP_FLAGS} -Wno-format-truncation")
  endif()
  check_c_compiler_flag(
    "-Werror=stringop-truncation"
    COMPATIBLE_STRINGOP_TRUNCATION
  )
  if(${COMPATIBLE_STRINGOP_TRUNCATION})
    set(COMP_FLAGS "${COMP_FLAGS} -Werror=stringop-truncation")
  endif()
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set(COMP_FLAGS "${COMP_FLAGS} -Wno-nonnull -Wno-deprecated-declarations")
  endif()

  check_c_compiler_flag("-Werror=unused-parameter" COMPATIBLE_UNUSED_PARAMETER)
  if(${COMPATIBLE_UNUSED_PARAMETER})
    set(COMP_FLAGS "${COMP_FLAGS} -Werror=unused-parameter")
  endif()

  check_c_compiler_flag(
    "-Werror=unused-but-set-variable"
    COMPATIBLE_UNUSED_BUT_SET_VARIABLE
  )
  if(${COMPATIBLE_UNUSED_BUT_SET_VARIABLE})
    set(COMP_FLAGS "${COMP_FLAGS} -Werror=unused-but-set-variable")
  endif()

  # Minimal debug info with Clang
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(COMP_FLAGS "${COMP_FLAGS} -gline-tables-only")
  else()
    set(COMP_FLAGS "${COMP_FLAGS} -g")
  endif()

  # Note: The following flags are problematic on older systems with gcc 4.8
  if(
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
    OR
      (
        "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
        AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9.9
      )
  )
    if(
      "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
      OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"
    )
      set(COMP_FLAGS "${COMP_FLAGS} -Wno-unused-command-line-argument")
    endif()
    find_program(LDGOLD_FOUND ld.gold)
    set(LDGOLD_SUPPORT OFF CACHE BOOL "Enable ld.gold support")
    # Optional ld.gold is 2x faster than normal ld
    if(
      LDGOLD_FOUND
      AND LDGOLD_SUPPORT MATCHES ON
      AND NOT APPLE
      AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES arm
    )
      set(LINKER_FLAGS "${LINKER_FLAGS} -fuse-ld=gold")
      # Use Identical Code Folding
      set(COMP_FLAGS "${COMP_FLAGS} -ffunction-sections")
      set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,--icf=safe")
      # Compress the debug sections
      # Note: Before valgrind 3.12.0, patch should be applied for valgrind (https://bugs.kde.org/show_bug.cgi?id=303877)
      if(
        NOT APPLE
        AND NOT ANDROID
        AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES arm
        AND NOT CMAKE_CXX_CLANG_TIDY
      )
        set(COMP_FLAGS "${COMP_FLAGS} -Wa,--compress-debug-sections")
        set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,--compress-debug-sections=zlib")
      endif()
    endif()
  endif()

  # Apply the flags
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMP_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMP_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS} -shared")
endif()

# Sanitizer support
set(CLANG_SANITIZERS OFF CACHE BOOL "Clang's sanitizer support")
if(
  CLANG_SANITIZERS
  AND
  (
    (UNIX AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    OR
    (APPLE AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  )
)
  set(
    CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer"
  )
  set(
    CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer"
  )
  set(
    CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer"
  )
  set(
    CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer"
  )
endif()

# Unity Build support
include(UnityBuild)

cmake_minimum_required(VERSION 3.0.0)

include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/mmal)
include_directories(/opt/vc/include/interface/mmal/util)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)

link_directories(/opt/vc/lib)

set (MMAL_LIBS mmal_core mmal_util mmal_vc_client)
set (EGL_LIBS brcmGLESv2 brcmEGL)

# Removed vcsm
foreach(lib ${MMAL_LIBS} ${EGL_LIBS} vcos bcm_host m dl)
    find_library(${lib}_LIBRARY
                 NAMES ${lib}
                 HINTS ${MMAL_DIR}/lib /opt/vc/lib
    )
    if (DEFINED ${lib}_LIBRARY)
        set(MMAL_LIBRARIES ${MMAL_LIBRARIES} ${${lib}_LIBRARY})
    else()
        message(FATAL_ERROR "Failed to find ${${lib}_LIBRARY} library")
    endif()
endforeach(lib)

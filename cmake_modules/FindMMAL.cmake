# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# This module can find the MMAL camera libraries.
#
cmake_minimum_required(VERSION 3.0.0)

set (MMAL_LIBS mmal_core mmal_util mmal_vc_client)
set (EGL_LIBS brcmGLESv2 brcmEGL)

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

find_path(BCM_INCLUDE_DIR NAMES bcm_host.h
    HINTS "/opt/vc/include"
)

find_path(MMAL_BASE_INCLUDE_DIR NAMES mmal.h
    HINTS "/opt/vc/include/interface/mmal"
)

find_path(MMAL_UTIL_INCLUDE_DIR NAMES mmal_util.h
    HINTS "/opt/vc/include/interface/mmal/util"
)

if (MMAL_BASE_INCLUDE_DIR AND BCM_INCLUDE_DIR AND MMAL_UTIL_INCLUDE_DIR)
  set(MMAL_INCLUDE_DIR ${MMAL_BASE_INCLUDE_DIR} ${BCM_INCLUDE_DIR} ${MMAL_UTIL_INCLUDE_DIR})
  set(MMAL_FOUND TRUE)
endif()

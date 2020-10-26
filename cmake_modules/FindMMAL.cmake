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
include(FindPackageHandleStandardArgs)

set(MMAL_ROOT /opt/vc)

set (MMAL_LIBS mmal_core mmal_util mmal_vc_client)
set (EGL_LIBS brcmGLESv2 brcmEGL)

find_path(BCM_INCLUDE_DIR NAMES bcm_host.h
    REQUIRED
)
find_package_handle_standard_args(MMAL DEFAULT_MSG BCM_INCLUDE_DIR)
list(APPEND MMAL_VARS BCM_INCLUDE_DIR)
list(APPEND MMAL_INCLUDE_DIR ${BCM_INCLUDE_DIR})

find_path(MMAL_BASE_INCLUDE_DIR NAMES mmal.h
    PATH_SUFFIXES "include/interface/mmal"
    REQUIRED
)
list(APPEND MMAL_VARS MMAL_BASE_INCLUDE_DIR)
list(APPEND MMAL_INCLUDE_DIR ${MMAL_BASE_INCLUDE_DIR})

find_path(MMAL_UTIL_INCLUDE_DIR NAMES mmal_util.h
    REQUIRED
    PATH_SUFFIXES "include/interface/mmal/util"
)
list(APPEND MMAL_VARS MMAL_UTIL_INCLUDE_DIR)
list(APPEND MMAL_INCLUDE_DIR ${MMAL_UTIL_INCLUDE_DIR})


foreach(lib ${MMAL_LIBS} ${EGL_LIBS} vcos bcm_host m dl)
    find_library(${lib}_LIBRARY
                 NAMES ${lib}
                 PATHS ${MMAL_DIR}/lib /opt/vc/lib
                 REQUIRED
    )
    list(APPEND MMAL_VARS ${lib}_LIBRARY)
    list(APPEND MMAL_LIBRARIES ${${lib}_LIBRARY})
endforeach(lib)


find_package_handle_standard_args(MMAL DEFAULT_MSG ${MMAL_VARS})

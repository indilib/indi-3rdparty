#set_property(TARGET SerialDeviceControl PROPERTY VERSION "0.815")
#set_property(TARGET SerialDeviceControl PROPERTY SOVERSION "1")

add_library(SerialDeviceControl SerialCommand.cpp SerialCommandTransceiver.cpp)

target_include_directories(SerialDeviceControl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

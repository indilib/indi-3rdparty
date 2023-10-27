This is the library that supports the Meade-branded Touptek camers

This library is allows us to use the official Touptek libraries to
support the Meade cameras. This is necessary because Meade does not provide
libraries for their cameras like every other touptak rebadger does.

The library works by using dynamic linking tricks (dlopen/dlsym) to pass
function calls to the official touptek library. The Toupcam_EnumV2 function
is the only original code that this library implements and it remaps the Meade
cameras to the corresponding Touptek camera.

This has been verified to work with the Toupcam library 54.23231 on arm64.

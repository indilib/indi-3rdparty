#pragma once
inline void OTAUpdate(){++otaCalls;for(int p:{4,7,8})if(levels[p]==HIGH)otaWhileActive=true;}

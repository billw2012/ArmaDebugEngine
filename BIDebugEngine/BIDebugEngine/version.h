#pragma once


#ifdef APPVEYOR_BUILD_VERSION
#define DBG_VERSION APPVEYOR_BUILD_VERSION
#define RESOURCE_VERSION 1,0,0,APPVEYOR_BUILD_NUMBER
#define DBG_BUILD APPVEYOR_BUILD_NUMBER
#define isCI 1
#else
#define DBG_VERSION "1.0.0.dev"
#define RESOURCE_VERSION 1,0,0,1337
#define DBG_BUILD 13371337
#endif



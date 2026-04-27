#ifndef __ARDUINO_MODULES_OTA_H__
#define __ARDUINO_MODULES_OTA_H__
#include "rcore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    namespace nota
    {
        // WiFi has to be setup before OTA can be setup!
        void setup(); 
        
        void tick();

    }  // namespace nota
}  // namespace ncore

#endif  // __ARDUINO_MODULES_OTA_H__

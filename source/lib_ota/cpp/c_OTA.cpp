#include "rcore/c_target.h"
#include "rcore/c_log.h"
#include "rcore/c_serial.h"

#ifdef TARGET_ARDUINO

#include "rOTA/c_OTA.h"

#    include "Arduino.h"
#    include "ArduinoOTA.h"

namespace ncore
{
    namespace nota
    {
        void setup()
        {
            // Port defaults to 3232
            // ArduinoOTA.setPort(3232); // Use 8266 port if you are working in Sloeber IDE, it is fixed there and not adjustable

            // No authentication by default
            // ArduinoOTA.setPassword("admin");

            // Password can be set with it's md5 value as well
            // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
            // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

            ArduinoOTA.onStart(
              []()
              {
                  // NOTE: make .detach() here for all functions called by Ticker.h library - not to interrupt transfer process in any way.
                  Serial.println("Start updating ");
                  if (ArduinoOTA.getCommand() == U_FLASH)
                      Serial.println("sketch");
                  else  // U_SPIFFS
                      Serial.println("filesystem");

                  // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
              });

            ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

            ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

            ArduinoOTA.onError(
              [](ota_error_t error)
              {
                  Serial.printf("Error[%u]: ", error);
                  if (error == OTA_AUTH_ERROR)
                      Serial.println("\nAuth Failed");
                  else if (error == OTA_BEGIN_ERROR)
                      Serial.println("\nBegin Failed");
                  else if (error == OTA_CONNECT_ERROR)
                      Serial.println("\nConnect Failed");
                  else if (error == OTA_RECEIVE_ERROR)
                      Serial.println("\nReceive Failed");
                  else if (error == OTA_END_ERROR)
                      Serial.println("\nEnd Failed");
              });

            ArduinoOTA.begin();

            Serial.println("OTA Initialized");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());

#    if defined(ESP32_RTOS) && defined(ESP32)
            xTaskCreate(ota_handle,   /* Task function. */
                        "OTA_HANDLE", /* String with name of task. */
                        10000,        /* Stack size in bytes. */
                        NULL,         /* Parameter passed as input of the task */
                        1,            /* Priority of the task. */
                        NULL);        /* Task handle. */
#    endif
        }

        void tick()
        {
#    ifdef TARGET_ESP8266
            ArduinoOTA.handle();
#    endif
        }

#    ifdef TARGET_ESP32
        void handle(void* parameter)
        {
            for (;;)
            {
                ArduinoOTA.handle();
                delay(3500);
            }
        }
#    endif

    }  // namespace nota
}  // namespace ncore
#endif

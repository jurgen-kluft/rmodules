#include "rrotary_encoder/c_rotary_encoder.h"

#if defined(TARGET_ESP32)
#    include "esp_log.h"
#    define LOG_TAG "RotaryEncoder"
#endif

#if defined(ARDUINO) && ARDUINO >= 100
#    include "Arduino.h"
#endif

namespace ncore
{
    namespace nrotary
    {
        // Rotary Encocer
        enum
        {
            ROTARYENCODER_DEFAULT_A_PIN      = 16,
            ROTARYENCODER_DEFAULT_B_PIN      = 17,
            ROTARYENCODER_DEFAULT_BUTTON_PIN = 18,
            ROTARYENCODER_DEFAULT_VCC_PIN    = -1,
            ROTARYENCODER_DEFAULT_STEPS      = 2,
        };

        config_t::config_t()
        {
            APin      = ROTARYENCODER_DEFAULT_A_PIN;
            BPin      = ROTARYENCODER_DEFAULT_B_PIN;
            ButtonPin = ROTARYENCODER_DEFAULT_BUTTON_PIN;
            VccPin    = ROTARYENCODER_DEFAULT_VCC_PIN;
            Steps     = ROTARYENCODER_DEFAULT_STEPS;
        }

        struct encoder_t
        {
            s8 m_index;  // for multiple encoders support

#if defined(TARGET_ESP8266)
#else
            portMUX_TYPE m_mux;
            portMUX_TYPE m_buttonMux;
#endif
            volatile s32 m_encoder0Pos;
            volatile s8  m_lastMovementDirection;  // 1 right; -1 left
            volatile u64 m_lastMovementAt;

            bool m_circleValues;
            bool m_isEnabled;

            config_t m_config;

            u32 m_rotaryAccelerationCoef;
            s32 m_minEncoderValue;  // -1 << 15;
            s32 m_maxEncoderValue;  // 1 << 15;

            s8   m_old_AB;
            s32  m_lastReadEncoder0Pos;
            bool m_previous_butt_state;
            bool m_wasTimeouted;

            ButtonState m_buttonState;

            void (*ISR_callback)();
            void (*ISR_button)();

            s32  m_correctionOffset;
            bool m_isButtonPulldown;
        };

#if defined(TARGET_ESP8266)
        ICACHE_RAM_ATTR void readEncoder_ISR(encoder_t* self);
        ICACHE_RAM_ATTR void readButton_ISR(encoder_t* self);
#else
        void IRAM_ATTR readEncoder_ISR(encoder_t* self);
        void IRAM_ATTR readButton_ISR(encoder_t* self);
#endif

        const s8 s_enc_states[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

#if defined(TARGET_ESP8266)
        ICACHE_RAM_ATTR void readEncoder_ISR(encoder_t* self)
#else
        void IRAM_ATTR readEncoder_ISR(encoder_t* self)
#endif
        {
            const u64 now = millis();
#if defined(TARGET_ESP8266)
#else
            portENTER_CRITICAL_ISR(&(self->m_mux));
#endif
            if (self->m_isEnabled)
            {
                // code from https://www.circuitsathome.com/mcu/reading-rotary-encoder-on-arduino/
                self->m_old_AB <<= 2;  // remember previous state

                const s8 ENC_PORT = ((digitalRead(self->m_config.BPin)) ? (1 << 1) : 0) | ((digitalRead(self->m_config.APin)) ? (1 << 0) : 0);

                self->m_old_AB |= (ENC_PORT & 0x03);  // add current state

                // self->m_encoder0Pos += ( s_enc_states[( self->m_old_AB & 0x0f )]);
                const s8 currentDirection = (s_enc_states[(self->m_old_AB & 0x0f)]);  //-1,0 or 1
                if (currentDirection != 0)
                {
                    // bool ignoreCorrection = false;
                    // if (self->m_encoder0Pos > self->m_maxEncoderValue) ignoreCorrection = true;
                    // if (self->m_encoder0Pos < self->m_minEncoderValue) ignoreCorrection = true;
                    const s32 prevRotaryPosition = self->m_encoder0Pos / self->m_config.Steps;
                    self->m_encoder0Pos += currentDirection;
                    const s32 newRotaryPosition = self->m_encoder0Pos / self->m_config.Steps;

                    if (newRotaryPosition != prevRotaryPosition && self->m_rotaryAccelerationCoef > 1)
                    {
                        // additional movements cause acceleration?
                        //  at X ms, there should be no acceleration.
                        const u64 accelerationLongCutoffMillis = 200;
                        // at Y ms, we want to have maximum acceleration
                        const u64 accelerationShortCutffMillis = 4;

                        // compute linear acceleration
                        if (currentDirection == self->m_lastMovementDirection && currentDirection != 0 && self->m_lastMovementDirection != 0)
                        {
                            // ... but only of the direction of rotation matched and there
                            // actually was a previous rotation.
                            u64 millisAfterLastMotion = now - self->m_lastMovementAt;

                            if (millisAfterLastMotion < accelerationLongCutoffMillis)
                            {
                                if (millisAfterLastMotion < accelerationShortCutffMillis)
                                {
                                    millisAfterLastMotion = accelerationShortCutffMillis;  // limit to maximum acceleration
                                }
                                if (currentDirection > 0)
                                {
                                    self->m_encoder0Pos += self->m_rotaryAccelerationCoef / millisAfterLastMotion;
                                }
                                else
                                {
                                    self->m_encoder0Pos -= self->m_rotaryAccelerationCoef / millisAfterLastMotion;
                                }
                            }
                        }
                        self->m_lastMovementAt        = now;
                        self->m_lastMovementDirection = currentDirection;
                    }

                    // https://github.com/igorantolic/ai-esp32-rotary-encoder/issues/40
                    /*
                    when circling there is an issue since encoderSteps is tipically 4
                    that means 4 changes for a single roary movement (step)
                    so if maximum is 4 that means _maxEncoderValue is 4*4=16
                    when we detact 18 we cannot go to zero since next 2 will make it wild
                    Here we changed to 18 set not to 0 but to -2; 17 to -3...
                    Now it seems better however that -3 divided with 4 will give -1 which is not regular -> also readEncoder() is changed to give allowed values
                    It is not yet perfect for cycling options but it is much better than before

                    optimistic view was that most of the time encoder0Pos values will be near to N*encodersteps
                    */
                    // respect limits
                    if ((self->m_encoder0Pos / self->m_config.Steps) > (self->m_maxEncoderValue / self->m_config.Steps))
                    {
                        // Serial.print("circle values limit HIGH");
                        // Serial.print(self->m_encoder0Pos);
                        // self->m_encoder0Pos = self->m_circleValues ? self->m_minEncoderValue : self->m_maxEncoderValue;
                        if (self->m_circleValues)
                        {
                            // if (!ignoreCorrection){
                            const s32 delta     = self->m_maxEncoderValue + self->m_config.Steps - self->m_encoder0Pos;
                            self->m_encoder0Pos = self->m_minEncoderValue - delta;
                            //}
                        }
                        else
                        {
                            self->m_encoder0Pos = self->m_maxEncoderValue;
                        }
                        // self->m_encoder0Pos = self->m_circleValues ? (self->m_minEncoderValue self->m_encoder0Pos-self->m_config.Steps) : self->m_maxEncoderValue;
                        //  Serial.print(" -> ");
                        //  Serial.println(self->m_encoder0Pos);
                    }
                    else if ((self->m_encoder0Pos / self->m_config.Steps) < (self->m_minEncoderValue / self->m_config.Steps))
                    {
                        // Serial.print("circle values limit LOW");
                        // Serial.print(self->m_encoder0Pos);
                        // self->m_encoder0Pos = self->m_circleValues ? self->m_maxEncoderValue : self->m_minEncoderValue;
                        self->m_encoder0Pos = self->m_circleValues ? self->m_maxEncoderValue : self->m_minEncoderValue;
                        if (self->m_circleValues)
                        {
                            // if (!ignoreCorrection){
                            const s32 delta     = self->m_minEncoderValue + self->m_config.Steps + self->m_encoder0Pos;
                            self->m_encoder0Pos = self->m_maxEncoderValue + delta;
                            //}
                        }
                        else
                        {
                            self->m_encoder0Pos = self->m_minEncoderValue;
                        }

                        // Serial.print(" -> ");
                        // Serial.println(self->m_encoder0Pos);
                    }
                    else
                    {
                        // Serial.print("no circle values limit ");
                        // Serial.println(self->m_encoder0Pos);
                    }
                    // Serial.println(self->m_encoder0Pos);
                }
            }
#if defined(TARGET_ESP8266)
#else
            portEXIT_CRITICAL_ISR(&(self->m_mux));
#endif
        }

#if defined(TARGET_ESP8266)
        ICACHE_RAM_ATTR void readButton_ISR(encoder_t* self)
#else
        void IRAM_ATTR readButton_ISR(encoder_t* self)
#endif
        {
#if defined(TARGET_ESP8266)
#else
            portENTER_CRITICAL_ISR(&(self->m_buttonMux));
#endif
            const u8 butt_state = !digitalRead(self->m_config.ButtonPin);
            if (!self->m_isEnabled)
            {
                self->m_buttonState = BUTTON_DISABLED;
            }
            else if (butt_state && !self->m_previous_butt_state)
            {
                self->m_previous_butt_state = true;
                // Serial.println("Button Pushed");
                self->m_buttonState = BUTTON_PUSHED;
            }
            else if (!butt_state && self->m_previous_butt_state)
            {
                self->m_previous_butt_state = false;
                // Serial.println("Button Released");
                self->m_buttonState = BUTTON_RELEASED;
            }
            else
            {
                self->m_buttonState = (butt_state ? BUTTON_DOWN : BUTTON_UP);
                // Serial.println(butt_state ? "BUTTON_DOWN" : "BUTTON_UP");
            }

#if defined(TARGET_ESP8266)
#else
            portEXIT_CRITICAL_ISR(&(self->m_buttonMux));
#endif
        }

        typedef void (*ISR_CallbackType)(void);

        void setup_interrupts(encoder_t* self, ISR_CallbackType ISR_read, ISR_CallbackType ISR_button)
        {
            attachInterrupt(digitalPinToInterrupt(self->m_config.APin), ISR_read, CHANGE);
            attachInterrupt(digitalPinToInterrupt(self->m_config.BPin), ISR_read, CHANGE);
            if (self->m_config.ButtonPin >= 0)
                attachInterrupt(digitalPinToInterrupt(self->m_config.ButtonPin), ISR_button, RISING);
        }

        // We setup functions to support N rotary encoders
        static s8       s_RegisteredEncoderCount = 0;
        static const s8 s_MaxRegisteredEncoders  = 2;

        // ISR functions for encoder 0
        static encoder_t* s_RegisteredEncoder0 = nullptr;
        void IRAM_ATTR    readEncoderISR0() { readEncoder_ISR(s_RegisteredEncoder0); }
        void IRAM_ATTR    readButtonISR0() { readButton_ISR(s_RegisteredEncoder0); }

        // ISR functions for encoder 1
        static encoder_t* s_RegisteredEncoder1 = nullptr;
        void IRAM_ATTR    readEncoderISR1() { readEncoder_ISR(s_RegisteredEncoder1); }
        void IRAM_ATTR    readButtonISR1() { readButton_ISR(s_RegisteredEncoder1); }

        static void registerEncoderInstance(encoder_t* self)
        {
            if (s_RegisteredEncoderCount < s_MaxRegisteredEncoders)
            {
                switch (s_RegisteredEncoderCount)
                {
                    case 0: s_RegisteredEncoder0 = self; break;
                    case 1: s_RegisteredEncoder1 = self; break;
                    default: break;
                }
                self->m_index = s_RegisteredEncoderCount++;
            }
        }

        static void registerEncoderInterrupts(encoder_t* self)
        {
            switch (self->m_index)
            {
                case 0: setup_interrupts(self, readEncoderISR0, readButtonISR0); break;
                case 1: setup_interrupts(self, readEncoderISR1, readButtonISR1); break;
                default: break;
            }
        }

        encoder_t* construct(config_t const& config, bool areEncoderPinsPulldownForEsp32)
        {
            if (s_RegisteredEncoderCount < s_MaxRegisteredEncoders)
            {
                encoder_t* self = new encoder_t();
                self->m_config  = config;

#if defined(TARGET_ESP8266)
#else
                self->m_mux       = portMUX_INITIALIZER_UNLOCKED;
                self->m_buttonMux = portMUX_INITIALIZER_UNLOCKED;
#endif
                self->m_encoder0Pos           = 0;
                self->m_lastMovementDirection = 0;  // 1 right; -1 left
                self->m_lastMovementAt        = 0;

                self->m_circleValues = false;
                self->m_isEnabled    = true;

                self->m_rotaryAccelerationCoef = 150;
                self->m_minEncoderValue        = -2147483648;  // -1 << 15;
                self->m_maxEncoderValue        = 2147483647;   // 1 << 15;

                self->m_old_AB              = 0;
                self->m_lastReadEncoder0Pos = 0;
                self->m_previous_butt_state = 0;
                self->m_wasTimeouted        = false;

                self->m_buttonState = BUTTON_UP;

                self->m_correctionOffset = 2;
                self->m_isButtonPulldown = false;

#if defined(TARGET_ESP8266)
                pinMode(config.APin, INPUT_PULLUP);
                pinMode(config.BPin, INPUT_PULLUP);
#else
                pinMode(config.APin, (areEncoderPinsPulldownForEsp32 ? INPUT_PULLDOWN : INPUT_PULLUP));
                pinMode(config.BPin, (areEncoderPinsPulldownForEsp32 ? INPUT_PULLDOWN : INPUT_PULLUP));
#endif
                registerEncoderInstance(self);
                return self;
            }
            return nullptr;
        }

        void begin(encoder_t* self)
        {
            self->m_lastReadEncoder0Pos = 0;
            if (self->m_config.VccPin >= 0)
            {
                pinMode(self->m_config.VccPin, OUTPUT);
                digitalWrite(self->m_config.VccPin, 1);  // Vcc for encoder
            }

            // Initialize rotary encoder reading and decoding
            self->m_previous_butt_state = 0;
            if (self->m_config.ButtonPin >= 0)
            {
#if defined(TARGET_ESP8266)
                pinMode(self->m_config.ButtonPin, INPUT_PULLUP);
#else
                pinMode(self->m_config.ButtonPin, self->m_isButtonPulldown ? INPUT_PULLDOWN : INPUT_PULLUP);
#endif
            }

            registerEncoderInterrupts(self);
        }

        void setBoundaries(encoder_t* self, s32 minValue, s32 maxValue, bool circleValues)
        {
            self->m_minEncoderValue = minValue * self->m_config.Steps;
            self->m_maxEncoderValue = maxValue * self->m_config.Steps;
            self->m_circleValues    = circleValues;
        }

        s32 readEncoder(encoder_t* self)
        {
            // return (self->m_encoder0Pos / self->m_config.Steps);
            if ((self->m_encoder0Pos / self->m_config.Steps) > (self->m_maxEncoderValue / self->m_config.Steps))
                return self->m_maxEncoderValue / self->m_config.Steps;
            if ((self->m_encoder0Pos / self->m_config.Steps) < (self->m_minEncoderValue / self->m_config.Steps))
                return self->m_minEncoderValue / self->m_config.Steps;
            return (self->m_encoder0Pos / self->m_config.Steps);
        }

        void reset(encoder_t* self, s32 newValue)
        {
            newValue                    = newValue * self->m_config.Steps;
            self->m_encoder0Pos         = newValue + self->m_correctionOffset;
            self->m_lastReadEncoder0Pos = self->m_encoder0Pos;
            if (self->m_encoder0Pos > self->m_maxEncoderValue)
                self->m_encoder0Pos = self->m_circleValues ? self->m_minEncoderValue : self->m_maxEncoderValue;
            if (self->m_encoder0Pos < self->m_minEncoderValue)
                self->m_encoder0Pos = self->m_circleValues ? self->m_maxEncoderValue : self->m_minEncoderValue;

            self->m_lastReadEncoder0Pos = readEncoder(self);
        }

        s32 hasChanged(encoder_t* self)
        {
            s32 encoder0Pos             = readEncoder(self);
            s32 encoder0Diff            = encoder0Pos - self->m_lastReadEncoder0Pos;
            self->m_lastReadEncoder0Pos = encoder0Pos;
            return encoder0Diff;
        }

        void setValue(encoder_t* self, s32 newValue) { reset(self, newValue); }
        s32  currentValue(encoder_t* self) { return readEncoder(self); }

        void enable(encoder_t* self) { self->m_isEnabled = true; }
        void disable(encoder_t* self) { self->m_isEnabled = false; }

        ButtonState currentButtonState(encoder_t* self) { return self->m_buttonState; }

        bool isButtonClicked(encoder_t* self, u64 maximumWaitMilliseconds)
        {
            s32 button = 1 - digitalRead(self->m_config.ButtonPin);
            if (!button)
            {
                if (self->m_wasTimeouted)
                {
                    self->m_wasTimeouted = false;
                    return true;
                }
                return false;
            }
            delay(30);  // debounce
            button = 1 - digitalRead(self->m_config.ButtonPin);
            if (!button)
            {
                return false;
            }

            // wait release of button but only maximumWaitMilliseconds
            self->m_wasTimeouted = false;
            const u64 waitUntil  = millis() + maximumWaitMilliseconds;
            while (1 - digitalRead(self->m_config.ButtonPin))
            {
                if (millis() > waitUntil)
                {
                    // button not released until timeout
                    self->m_wasTimeouted = true;
                    return false;
                }
            }

            return true;
        }

        bool isButtonDown(encoder_t* self) { return digitalRead(self->m_config.ButtonPin) ? false : true; }

        u32  getAcceleration(encoder_t* self) { return self->m_rotaryAccelerationCoef; }
        void setAcceleration(encoder_t* self, u32 acceleration) { self->m_rotaryAccelerationCoef = acceleration; }
        void disableAcceleration(encoder_t* self) { setAcceleration(self, 0); }

    }  // namespace nrotary
}  // namespace ncore
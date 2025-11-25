#ifndef __RDNO_MODULES_ROTARY_ENCODER_H__
#define __RDNO_MODULES_ROTARY_ENCODER_H__
#include "rdno_core/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    namespace nrotary
    {
        enum ButtonState
        {
            BUTTON_DOWN     = 0,
            BUTTON_PUSHED   = 1,
            BUTTON_UP       = 2,
            BUTTON_RELEASED = 3,
            BUTTON_DISABLED = 99,
        };

        struct encoder_t;

        struct config_t
        {
            config_t();

            u8 APin;       // CLK
            u8 BPin;       // DT
            s8 ButtonPin;  // -1 for no button
            s8 VccPin;     // -1 for no Vcc pin control
            u8 Steps;
        };

        // A maximum of 2 rotary encoders are supported simultaneously
        // Note: You can change this by modifying the code accordingly
        encoder_t* construct(config_t const& config, bool areEncoderPinsPulldownForEsp32 = false);

        // Register interrupts and begin operation
        void begin(encoder_t* self);

        // Encoder functionality
        s32         hasChanged(encoder_t* self);
        s32         currentValue(encoder_t* self);
        ButtonState currentButtonState(encoder_t* self);
        bool        isButtonClicked(encoder_t* self, u64 maximumWaitMilliseconds = 300);
        bool        isButtonDown(encoder_t* self);
        void        setValue(encoder_t* self, s32 newValue);
        void        setBoundaries(encoder_t* self, s32 minValue = -100, s32 maxValue = 100, bool circleValues = false);
        u32         getAcceleration(encoder_t* self);
        void        setAcceleration(encoder_t* self, u32 acceleration);
        void        disableAcceleration(encoder_t* self);

    }  // namespace nrotary
}  // namespace ncore

#endif  // __RDNO_MODULES_ROTARY_ENCODER_H__

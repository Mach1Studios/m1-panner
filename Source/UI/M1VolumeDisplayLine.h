#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"

using namespace murka;

class M1VolumeDisplayLine : public murka::View<M1VolumeDisplayLine>
{
public:
    void internalDraw(Murka& m)
    {
        m.setColor(DISABLED_PARAM);
        m.drawRectangle(shape.size.x / 2 - 2,
            0,
            4,
            shape.size.y);

        if (volume > maxVolume)
        {
            maxVolumeReachedTime = m.getElapsedTime();
            maxVolume = volume;
        }
        else
        {
            if ((m.getElapsedTime() - maxVolumeReachedTime) > 0.5)
            {
                maxVolume -= 0.3;
                if (maxVolume < volume)
                {
                    maxVolume = volume;
                }
            }
        }

        // Drawing peak volume meter & rms volume indicator
        // v is the input volume normalised, 0 to 1
        float v = clamp(map(volume, -48, 6, 0, 1), 0, 1);

        if (volume > 0)
        {
            clipIndicatorReachedTime = m.getElapsedTime();
            // red clip indicator
            m.setColor(METER_RED);
            m.drawRectangle(shape.size.x / 2 - 2,
                0,
                4,
                4);
        }
        else
        {
            if ((m.getElapsedTime() - clipIndicatorReachedTime) > 2)
            { // 2s clip indicator hold
            }
            else
            {
                // red clip indicator
                m.setColor(METER_RED);
                m.drawRectangle(shape.size.x / 2 - 2,
                    0,
                    4,
                    4);
            }
        }

        if (v > 0.795)
        {
            // reds - dimmed for external meters
            if (isExternalMeter)
                m.setColor(METER_RED_DIM);
            else
                m.setColor(METER_RED);
            m.drawRectangle(shape.size.x / 2 - 2,
                shape.size.y - v * shape.size.y,
                4,
                v * shape.size.y);
        }

        if (v > 0.68)
        {
            // yellows - dimmed for external meters
            float f = v;
            if (f > 0.795)
                f = 0.795;
            if (isExternalMeter)
                m.setColor(METER_YELLOW_DIM);
            else
                m.setColor(METER_YELLOW);
            m.drawRectangle(shape.size.x / 2 - 2,
                shape.size.y - f * shape.size.y,
                4,
                f * shape.size.y);
        }

        // greens - dimmed for external meters
        float g = v;
        if (g > 0.68)
            g = 0.68;
        if (isExternalMeter)
            m.setColor(METER_GREEN_DIM);
        else
            m.setColor(METER_GREEN);
        m.drawRectangle(shape.size.x / 2 - 2,
            shape.size.y - g * shape.size.y,
            4,
            g * shape.size.y);

        // volume rms indicator
        float d = 0;
        if (maxVolume < 0 && maxVolume > -100)
        {
            d = maxVolume / -144; // negative dB
            d += 0.205; // offset by red zone percentage
        }
        else
        {
            d = std::abs(maxVolume / 12); // positive dB
            d *= 0.205; // multiply by top red zone percentage
        }
        m.setColor(ENABLED_PARAM);
        m.drawRectangle(shape.size.x / 2 - 2,
            d * shape.size.y,
            4,
            4);
    }

    float map(float value, float low1, float high1, float low2, float high2)
    {
        return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
    }

    float lerp(float a, float b, float f)
    {
        return a + f * (b - a);
    }

    float clamp(float x, float a, float b)
    {
        return x < a ? a : (x > b ? b : x);
    }

    float maxVolume = 0;
    double maxVolumeReachedTime = 0;
    double clipIndicatorReachedTime = 0;
    typedef bool Results;

    MURKA_PARAMETER(M1VolumeDisplayLine, // class name
        float, // parameter type
        volume, // parameter variable name
        withVolume, // setter
        0.0 // default
    )

    MURKA_PARAMETER(M1VolumeDisplayLine, // class name
        bool, // parameter type
        isExternalMeter, // parameter variable name
        withExternalMeter, // setter
        false // default
    )
};

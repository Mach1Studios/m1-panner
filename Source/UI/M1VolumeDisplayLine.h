#pragma once

#include "MurkaView.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1VolumeDisplayLine : public murka::View<M1VolumeDisplayLine> {
public:
    void internalDraw(Murka & m) {
        MurkaContext& context = m.currentContext;

        bool inside = context.isHovered() * !areInteractiveChildrenHovered(context) * hasMouseFocus(m);
        
        m.setColor(DISABLED_PARAM);
        m.drawRectangle(shape.size.x / 2 - 2,
                         0,
                         4,
                         shape.size.y);
        
        if (volume > maxVolume) {
            maxVolumeReachedTime = m.getElapsedTime();
            maxVolume = volume;
        } else {
            if ((m.getElapsedTime() - maxVolumeReachedTime) > 0.5) {
                maxVolume -= 0.3;
                if (maxVolume < volume) {
                    maxVolume = volume;
                }
            }
        }
        
        // Drawing volume & max volume thing
        // v is a volume normalised, 0 to 1
        float v = clamp(map(volume, -48, 6, 0, 1), 0, 1);
        
        if (v > 0.9) {
            // reds
            m.setColor(METER_RED);
            m.drawRectangle(shape.size.x / 2 - 2,
                            shape.size.y - v * shape.size.y,
                            4,
                            v * shape.size.y);
        }

        if (v > 0.75) {
            // yellows
            float f = v;
            if (f > 0.9) f = 0.9;
            m.setColor(METER_YELLOW);
            m.drawRectangle(shape.size.x / 2 - 2,
                            shape.size.y - f * shape.size.y,
                            4,
                            f * shape.size.y);
        }

        // greens
        float g = v;
        if (g > 0.75) g = 0.75;
        m.setColor(METER_GREEN);
        m.drawRectangle(shape.size.x / 2 - 2,
                        shape.size.y - g * shape.size.y,
                        4,
                        g * shape.size.y);

        /*
        // White dot that follows on the top

        float d = 1 + (maxVolume) / 144;
        d -= 0.65;
        v *= 8.95;
            
        // Same magic numbers
        r->setColor(ENABLED_PARAM);
        r->drawRectangle(c.getSize().x / 2 - 2,
                         context.getSize().y - d * context.getSize().y,
                         4,
                         4);
        */
    };

    float map(float value, float low1, float high1, float low2, float high2) {
        return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
    }

    float lerp(float a, float b, float f) {
        return a + f * (b - a);
    }

    float clamp(float x, float a, float b) {
        return x < a ? a : (x > b ? b : x);
    }

    MURKA_PARAMETER(M1VolumeDisplayLine, // class name
                    float, // parameter type
                    volume, // parameter variable name
                    withVolume, // setter
                    0.0 // default
    )
   
    float maxVolume = 0;
    double maxVolumeReachedTime = 0;

    typedef bool Results;
};

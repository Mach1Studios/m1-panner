#pragma once

#include "MurkaAssets.h"
#include "MurkaBasicWidgets.h"
#include "MurkaContext.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaTypes.h"
#include "MurkaView.h"

using namespace murka;

class M1Checkbox : public murka::View<M1Checkbox>
{
public:
    void internalDraw(Murka& m)
    {
        if (didntInitialiseYet)
        {
            animatedData = *((bool*)dataToControl) ? 1.0 : 0.0;
            didntInitialiseYet = false;
        }

        float animation = A(inside() * enabled);

        m.pushStyle();
        m.enableFill();
        m.setColor(100 + 110 * enabled + 30 * animation, 220);
        m.drawCircle(getSize().y / 2, getSize().y / 2, getSize().y / 2);
        m.setColor(40 + 20 * !enabled, 255);
        m.drawCircle(getSize().y / 2, getSize().y / 2, getSize().y / 2 - 2);

        m.setColor(100 + 110 * enabled + 30 * animation, 220);

        animatedData = A(*((bool*)dataToControl));
        m.drawCircle(getSize().y / 2, getSize().y / 2, 4 * animatedData);

        m.setColor(100 + 110 * enabled + 30 * animation, 220);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        m.prepare<murka::Label>({ shape.size.y + 2, 2, 150, shape.size.y + 5 }).text(label).draw();
        m.disableFill();
        m.popStyle();

        // Action
        if ((mouseDownPressed(0)) && (inside()) && enabled)
        {
            *((bool*)dataToControl) = !*((bool*)dataToControl);
            changed = true;
        }
        else
        {
            changed = false;
        }

        checked = *((bool*)dataToControl);
    }

    float animatedData = 0;
    bool didntInitialiseYet = true;
    bool changed = false;
    bool checked = true;
    std::string label;
    bool* dataToControl = nullptr;

    M1Checkbox& controlling(bool* dataPointer)
    {
        dataToControl = dataPointer;
        return *this;
    }

    M1Checkbox& withLabel(std::string label_)
    {
        label = label_;
        return *this;
    }

    MURKA_PARAMETER(M1Checkbox, // class name
        bool, // parameter type
        enabled, // parameter variable name
        enable, // setter
        true // default
    )
};

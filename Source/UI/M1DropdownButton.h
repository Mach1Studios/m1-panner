#pragma once

#include "MurkaAssets.h"
#include "MurkaBasicWidgets.h"
#include "MurkaContext.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaTypes.h"
#include "MurkaView.h"

using namespace murka;

class M1DropdownButton : public murka::View<M1DropdownButton>
{
public:
    void internalDraw(Murka& m)
    {
        if (outlineEnabled)
        {
            // outline border
            m.setColor(ENABLED_PARAM);
            m.drawRectangle(0, 0, shape.size.x, shape.size.y);
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle(1, 1, shape.size.x - 2, shape.size.y - 2);
        }

        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, (int)fontSize);
        // centered interior text
        juceFontStash::Rectangle label_box = m.getCurrentFont()->getStringBoundingBox(label, 0, 0); // used to find size of text
        m.prepare<murka::Label>({ labelPadding_x, labelPadding_y + shape.size.y / 2 - label_box.height / 2, shape.size.x - labelPadding_x, shape.size.y }).withAlignment(textAlignment).text(label).draw();

        pressed = false;
        if ((inside()) && (mouseDownPressed(0)))
        {
            pressed = true; // Only sets to true the frame the "pressed" happened
        }
    }

    std::string label = "";
    bool pressed = false;
    float fontSize = 10;
    bool outlineEnabled = true;
    int labelPadding_x = 0;
    int labelPadding_y = 0;

    TextAlignment textAlignment = TEXT_CENTER;

    operator bool()
    {
        return pressed;
    }

    M1DropdownButton& withLabel(std::string label_)
    {
        label = label_;
        return *this;
    }

    M1DropdownButton& withOutline(bool outlineEnabled_ = false)
    {
        outlineEnabled = outlineEnabled_;
        return *this;
    }

    M1DropdownButton& withFontSize(float fontSize_)
    {
        fontSize = fontSize_;
        return *this;
    }
};

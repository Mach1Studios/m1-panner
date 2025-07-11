#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"

using namespace murka;

class M1Label : public murka::View<M1Label>
{
public:
    void internalDraw(Murka& m)
    {
        bool isInside = inside();
        //* !areInteractiveChildrenHovered(c) *
        //hasMouseFocus(m);

        auto font = m.getCurrentFont();
        if (customFont)
        {
            font = font;
        }

        MurkaColor bgColor = backgroundColor;
        m.enableFill();
        if (bgColor.getAlpha() != 0.0)
        {
            m.setColor(bgColor);
            if (alignment == TEXT_LEFT)
            {
                m.drawRectangle(0, 0, font->getStringBoundingBox(label, 0, 0).width + 10, shape.size.y);
            }
        }

        // color
        MurkaColor fgColor = customColor ? color : m.getColor();
        float anim = enabled ? 1.0 * 40 / 255 * A(highlighted) : 0.0;
        fgColor.setRed(fgColor.getRed() + anim - fgColor.getRed() * 0.5 * !enabled);
        fgColor.setGreen(fgColor.getGreen() + anim - fgColor.getGreen() * 0.5 * !enabled);
        fgColor.setBlue(fgColor.getBlue() + anim - fgColor.getBlue() * 0.5 * !enabled);
        m.setColor(fgColor);

        if (labelVerticalCentering)
        {
            // add to the labelPadding_y the text height halved subtracted from the height halved
            label_y_center = shape.size.y / 2 - font->getStringBoundingBox(label, 0, 0).height / 2;
        }

        if (alignment == TEXT_LEFT)
        {
            font->drawString(label, labelPadding_x, labelPadding_y + label_y_center);
        }
        if (alignment == TEXT_CENTER)
        {
            float textX = (shape.size.x / 2) - (font->getStringBoundingBox(label, 0, 0).width / 2);
            font->drawString(label, textX, labelPadding_y + label_y_center);
        }
        if (alignment == TEXT_RIGHT)
        {
            float textX = (shape.size.x - labelPadding_x) - font->getStringBoundingBox(label, 0, 0).width;
            font->drawString(label, textX, labelPadding_y + label_y_center);
        }
    }

    // Here go parameters and any parameter convenience constructors.

    TextAlignment alignment = TEXT_LEFT;
    int labelPadding_x = 5;
    int labelPadding_y = 0;
    float label_y_center = 0;
    bool labelVerticalCentering = false;

    M1Label& withTextAlignment(TextAlignment a)
    {
        alignment = a;
        return *this;
    }

    M1Label& withVerticalTextOffset(float offset)
    {
        labelPadding_y = offset;
        return *this;
    }

    M1Label& withVerticalTextCentering(bool withVertCenter)
    {
        labelVerticalCentering = withVertCenter;
        return *this;
    }

    M1Label& withText(std::string text)
    {
        label = text;
        return *this;
    }

    MurkaColor color = { 0.98, 0.98, 0.98 };
    MurkaColor backgroundColor = { 0., 0., 0., 0. };

    FontObject* font;

    bool customColor = false;
    bool customFont = false;
    bool enabled = true;
    bool highlighted = false;

    MURKA_PARAMETER(M1Label, // class name
        std::string, // parameter type
        label, // parameter variable name
        text, // setter
        "" // default
    )

    // The results type, you also need to define it even if it's nothing.
    bool result;

    virtual bool wantsClicks() override { return false; } // override this if you want to signal that you don't want clicks
};

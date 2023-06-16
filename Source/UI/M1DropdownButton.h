#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1DropdownButton : public murka::View<M1DropdownButton> {
public:
    void internalDraw(Murka & m) {
        if (outlineEnabled) {
            // outline border
            m.setColor(ENABLED_PARAM);
            m.drawRectangle(0, 0,
                            shape.size.x, shape.size.y);
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle(1, 1,
                            shape.size.x - 2, shape.size.y - 2);
        }

        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
        // centered interior text
        m.prepare<murka::Label>({0, 8, shape.size.x, shape.size.y}).withAlignment(TEXT_CENTER).text(label).draw();

        pressed = false;
        if ((inside()) && (mouseDownPressed(0))) {
            pressed = true; // Only sets to true the frame the "pressed" happened
        }

//        m.popStyle(); // TODO: THIS THING MAKES EVERYTHING HANG, issue with Murka - have to show assert!
    };
    
    std::string label = "";    
    bool pressed = false;
    double fontSize = 10;
    bool outlineEnabled = true;

    operator bool() {
        return pressed;
    }
        
    M1DropdownButton & withLabel(std::string label_) {
        label = label_;
        return *this;
    }
    
    M1DropdownButton & withOutline(bool outlineEnabled_ = false) {
        outlineEnabled = outlineEnabled_;
        return *this;
    }
    
    M1DropdownButton & withFontSize(double fontSize_) {
        fontSize = fontSize_;
        return *this;
    }

};

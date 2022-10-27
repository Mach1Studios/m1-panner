#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1Dropdown : public murka::View<M1Dropdown> {
public:
    void internalDraw(Murka & m) {
        int* data = dataToControl;
        
        MurkaContext& context = m.currentContext;
        bool inside = context.isHovered() * !areInteractiveChildrenHovered(m) * hasMouseFocus(m);
        
        auto& c = context;
        
        if (didntInitialiseYet) {
            didntInitialiseYet = false;
        }
                
        m.pushStyle();
        
        // outline border
        m.setColor(ENABLED_PARAM);
        m.drawRectangle(0, 0, c.getSize().x, c.getSize().y);
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(2, 2, c.getSize().x - 2, c.getSize().y - 2);

        m.setColor(LABEL_TEXT_COLOR);
        m.setFont("Proxima Nova Reg.ttf", 10);
        m.draw<murka::Label>({2, 2, shape.size.x, shape.size.y + 5}).text(label).commit();
        m.popStyle();

        // Action
        if ((context.mouseDownPressed[0]) && (inside)) {
//            *((int*)dataToControl) = !*((int*)dataToControl);
            changed = true;
        }
        else {
            changed = false;
        }
    };
    
    float animatedData = 0;
    bool didntInitialiseYet = true;
    bool changed = false;
    bool dropdown = true; // false = dropup
    std::string label;
    int* dataToControl = nullptr;
    
    M1Dropdown & controlling(int* dataPointer) {
        dataToControl = dataPointer;
        return *this;
    }
    
    M1Dropdown & withLabel(std::string label_) {
        label = label_;
        return *this;
    }
    
    MURKA_PARAMETER(M1Dropdown, // class name
                    int, // parameter type
                    rangeFrom, // parameter variable name
                    withRangeFrom, // setter
                    0 // default
    )
    MURKA_PARAMETER(M1Dropdown, // class name
                    int, // parameter type
                    rangeTo, // parameter variable name
                    withRangeTo, // setter
                    1 // default
    )
//    MURKA_PARAMETER(M1Dropdown, // class name
//                    int*, // parameter type
//                    dataToControl, // parameter variable name
//                    controlling, // setter
//                    nullptr // default
//    )
    
};

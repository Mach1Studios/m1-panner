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
        int numberOfOptions = rangeTo - rangeFrom+1;
        
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
        m.drawRectangle(1, 1, c.getSize().x - 2, c.getSize().y - 2);

        m.setColor(LABEL_TEXT_COLOR);
        m.setFont("Proxima Nova Reg.ttf", 10);
        // centered interior text
        m.draw<murka::Label>({shape.size.x/2-shape.size.x/4, shape.size.y/2-shape.size.y/4, shape.size.x, shape.size.y}).text(label).commit();
        m.popStyle();

        // Action
        if ((context.mouseDownPressed[0]) && (inside)) {
            // open menu
            open = true;
            float yStep = (float)c.getSize().y / (float)numberOfOptions;
            selectedOption = context.mousePosition.y / yStep;
            float val = (float)selectedOption / (float)(numberOfOptions-1);

            if (drawAsDropdown) {
                // draw menu downwards
                
            } else {
                // draw menu upwards
                
            }
            
//            *((int*)dataToControl) = !*((int*)dataToControl);
            changed = true;
        }
        else {
            changed = false;
        }
    };
    
    bool didntInitialiseYet = true;
    bool changed = false;
    bool open = false;
    int selectedOption = 0;
    std::string label;
    
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
    // When drawing menu orientation: True = Dropdown, False = Dropup
    MURKA_PARAMETER(M1Dropdown, // class name
                    bool, // parameter type
                    drawAsDropdown, // parameter variable name
                    shouldDrawAsDropdown, // setter
                    true // default
    )
    MURKA_PARAMETER(M1Dropdown, // class name
                    int*, // parameter type
                    dataToControl, // parameter variable name
                    controlling, // setter
                    nullptr // default
    )
    
};

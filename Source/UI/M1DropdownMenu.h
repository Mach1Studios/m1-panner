#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1DropdownMenu : public murka::View<M1DropdownMenu> {
    
enum closingModeTypes {unknown, mouseUp, mouseDown};
closingModeTypes closingMode = closingModeTypes::unknown;
    
int mouseKeptDownFrames = 0;
bool mouseUpClosingMode = false;

double openedPhase = 0;
    
public:
    void internalDraw(Murka & m) {
        MurkaContext& context = m.currentContext;
        bool inside = context.isHovered() * !areInteractiveChildrenHovered(m) * hasMouseFocus(m);
        
        auto& c = context;
        
        if (closingMode == unknown) {
            if (context.mouseDown[0]) mouseKeptDownFrames++;
            if (mouseKeptDownFrames > 20) closingMode = mouseUp;
            if (!context.mouseDown[0]) closingMode = mouseDown;
        }
        
        if (opened) {

            // outline border
            m.setColor(ENABLED_PARAM);
            m.drawRectangle(0, 0, shape.size.x, shape.size.y);
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle(1, 1, shape.size.x - 2, shape.size.y - 2);
            
            for (int i = 0; i < options.size(); i++) {
                
                bool hoveredAnOption = (context.mousePosition.y > i * optionHeight) && (context.mousePosition.y < (i + 1) * optionHeight) && inside;
                
                if (hoveredAnOption) {
                    m.setColor(ENABLED_PARAM);
                    m.drawRectangle(1, i * optionHeight, shape.size.x - 2, optionHeight);
                    m.setColor(BACKGROUND_GREY);
                    m.setFont("Proxima Nova Reg.ttf", fontSize);
                    m.draw<murka::Label>({0, optionHeight * i, shape.size.x, optionHeight}).text(options[i]).withAlignment(TEXT_CENTER).commit();
                    
                    if (closingMode == mouseDown) {
                        if (c.mouseDownPressed[0]) {
                            opened = false; // Closing the menu
                        }
                    }
                    if (closingMode == mouseUp) {
                        if (c.mouseReleased[0]) {
                            opened = false; // Closing the menu
                        }
                    }
                } else {
                    m.setColor(LABEL_TEXT_COLOR);
                    m.setFont("Proxima Nova Reg.ttf", fontSize);
                    m.draw<murka::Label>({0, optionHeight * i + 5, shape.size.x, optionHeight}).text(options[i]).withAlignment(TEXT_CENTER).commit();
                }
            }
        }
    };
    
    void open() {
        opened = true;
        mouseKeptDownFrames = 0;
        closingMode = closingModeTypes::unknown;
    }
    
    bool changed = false;
    bool opened = false;
    
    int selectedOption = 0;
    
    std::vector<std::string> options;
    
    int optionHeight = 30;
    int fontSize = 10;
    bool enabled = true;
    std::string label;
    MurkaShape triggerButtonShape;
    
    M1DropdownMenu & withLabel(std::string label_) {
        label = label_;
        return *this;
    }
    
    M1DropdownMenu & withOptions(std::vector<std::string> options_) {
        options = options_;
        return *this;
    }
    
    M1DropdownMenu & withTriggerButtonPlacedAt(MurkaShape shape) {
        triggerButtonShape = shape;
        return *this;
    }
    
};

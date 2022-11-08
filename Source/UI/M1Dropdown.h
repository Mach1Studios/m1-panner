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
        m.setFont("Proxima Nova Reg.ttf", fontSize);
        // centered interior text
        m.draw<murka::Label>({(shape.size.x/2)-(shape.size.x/4), (shape.size.y/2)-(shape.size.y/4), shape.size.x, shape.size.y}).text(label).commit();

        // Action
        if ((context.mouseDownPressed[0]) && (inside)) {
            // open menu
            open = true;
            
            if (drawAsDropdown) {
                // draw menu downwards
                for (int i = 0; i < numberOfOptions; i++) {
                    m.setColor(ENABLED_PARAM);
                    m.drawRectangle(0, i * optionHeight, c.getSize().x, optionHeight);
                    m.setColor(BACKGROUND_GREY);
                    m.drawRectangle(1, 1 + i * optionHeight, c.getSize().x - 2, optionHeight - 2);
                    m.setColor(LABEL_TEXT_COLOR);
                    m.setFont("Proxima Nova Reg.ttf", fontSize);
                    m.draw<murka::Label>({shape.size.x/2-shape.size.x/4, i * optionHeight, shape.size.x, optionHeight}).text(label).commit();
                }
            } else {
                // draw menu upwards
                for (int i = 0; i < numberOfOptions; i++) {
                    m.setColor(ENABLED_PARAM);
                    m.drawRectangle(0, i * -optionHeight, c.getSize().x, optionHeight);
                    m.setColor(BACKGROUND_GREY);
                    m.drawRectangle(1, 1 + i * -optionHeight, c.getSize().x - 2, optionHeight - 2);
                    m.setColor(LABEL_TEXT_COLOR);
                    m.setFont("Proxima Nova Reg.ttf", fontSize);
                    m.draw<murka::Label>({shape.size.x/2-shape.size.x/4, i * -optionHeight, shape.size.x, optionHeight}).text(label).commit();
                }
            }
            
            // TODO: find where mouse clicks while open
            // TODO: find where mouse highlights and change background color of each option
//            float yStep = (float)c.getSize().y / (float)numberOfOptions;
//            selectedOption = context.mousePosition.y / yStep;
//            float val = (float)selectedOption / (float)(numberOfOptions-1);
//            *((int*)dataToControl) = val;
            changed = true;
        } else {
            open = false;
            changed = false;
        }
        m.popStyle();
    };
    
    bool didntInitialiseYet = true;
    bool changed = false;
    bool open = false;
    int rangeFrom = 0;
    int rangeTo = 0;
    int selectedOption = 0;
    int* dataToControl = nullptr;
    bool drawAsDropdown = false;
    int optionHeight = 20;
    int fontSize = 10;
    bool enabled = true;
    std::string label;
    
    M1Dropdown & withLabel(std::string label_) {
        label = label_;
        return *this;
    }
    
    M1Dropdown & withParameters(float rangeFrom_,
                            float rangeTo_,
                            int selectedOption_ = 0,
                            bool drawAsDropdown_ = true,
                            int optionHeight_ = 0,
                            int fontSize_ = 10,
                            bool enabled_ = true) {
        rangeFrom = rangeFrom_;
        rangeTo = rangeTo_;
        selectedOption = selectedOption_;
        drawAsDropdown = drawAsDropdown_;
        optionHeight = optionHeight_;
        fontSize = fontSize_;
        enabled = enabled_;
        
        return *this;
    }
    
    M1Dropdown & controlling(int* dataPointer) {
        dataToControl = dataPointer;
        return *this;
    }
    
};

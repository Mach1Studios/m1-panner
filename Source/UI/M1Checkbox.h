#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1Checkbox : public murka::View<M1Checkbox> {
public:
    void internalDraw(Murka & m) {
        bool* data = dataToControl;
        
        MurkaContext& context = m.currentContext;
        bool inside = context.isHovered() * !areInteractiveChildrenHovered(m) * hasMouseFocus(m);
        auto& c = context;
        
        if (didntInitialiseYet) {
            animatedData = *((bool*)dataToControl) ? 1.0 : 0.0;
            didntInitialiseYet = false;
        }
        
        float animation = A(inside * enabled);
        
		m.pushStyle();
        m.setColor(100 + 110 * enabled + 30 * animation, 220);
        m.drawCircle(c.getSize().y / 2, c.getSize().y / 2, c.getSize().y / 2);
        m.setColor(40 + 20 * !enabled, 255);
        m.drawCircle(c.getSize().y / 2, c.getSize().y / 2, c.getSize().y / 2 - 2);
        
        m.setColor(100 + 110 * enabled + 30 * animation, 220);
        
        animatedData = A(*((bool*)dataToControl));
        m.drawCircle(c.getSize().y / 2, c.getSize().y / 2,
                          4 * animatedData);

        m.setColor(100 + 110 * enabled + 30 * animation, 220);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);
        m.prepare<murka::Label>({shape.size.y + 2, 2, 150, shape.size.y + 5}).text(label).draw();
        m.popStyle();

        // Action
        if ((context.mouseDownPressed[0]) && (inside) && enabled) {
            *((bool*)dataToControl) = !*((bool*)dataToControl);
            changed = true;
		}
		else {
            changed = false;
		}
	}
    
    float animatedData = 0;
    bool didntInitialiseYet = true;
    bool changed = false;
    bool checked = true;
    std::string label;
    bool* dataToControl = nullptr;
    
    M1Checkbox & controlling(bool* dataPointer) {
        dataToControl = dataPointer;
        return *this;
    }
    
    M1Checkbox & withLabel(std::string label_) {
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

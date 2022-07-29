#pragma once

#include "MurkaView.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1PitchWheel : public murka::View<M1PitchWheel> {
public:
    void internalDraw(Murka & m) {
        MurkaContext& ctx = m.currentContext;

        bool inside = ctx.isHovered() * !areInteractiveChildrenHovered(ctx) * hasMouseFocus(m);
        
        hovered = inside + draggingNow;
        bool hoveredLocal = hovered + externalHovered; // this variable is not used outside the widget to avoid feedback loop

        changed = false; // false unless the user changed a value using this knob
        
        auto& c = ctx;
        
//        r->setColor(255, 0, 0, 255);
//        r->drawLine(0, 0, c.getSize().x, c.getSize().y);
//        r->drawLine(c.getSize().x, 0, 0, c.getSize().y);
		
		// Reticle calculation

		float reticlePositionNorm = (*((float*)dataToControl) - rangeFrom) / (rangeTo - rangeFrom);

		MurkaShape reticlePosition = { c.getSize().x / 2 - 6,
									  offset + (shape.size.y - offset * 2) * reticlePositionNorm - 6,
									  12,
									  12 };
		bool reticleHover = false + draggingNow;
		if (reticlePosition.inside(ctx.mousePosition)) {
			reticleHover = true;
		}

		m.pushStyle();
       
        int minilineStep = (shape.size.y - offset * 2) / 4;
        int minilineLength = shape.size.x / 6;
        int labelValue = 90;
        
        // Drawing labels
        
		m.setFont("Proxima Nova Reg.ttf", 8);
		for (int i = 0; i <= 4; i++) {
            float lineLength = minilineLength / 2;
            if (i == 2) lineLength *= 2;
			m.setColor(57 + 20 * A(reticleHover + externalHovered));
            m.drawLine(c.getSize().x / 2 - lineLength,
                        i * minilineStep + offset,
                        c.getSize().x / 2 + lineLength,
                        i * minilineStep + offset);
            
            MurkaShape labelShape = {55, (i * minilineStep + 2), 150, 50};
            m.setColor(115, 115, 115, 255);
            m.draw<murka::Label>(labelShape).text(std::to_string(labelValue)).commit();
            labelValue -= 45;
        }
   
		m.setColor(122 + 20 * A(reticleHover + externalHovered));
        m.drawLine(c.getSize().x / 2, offset,
			shape.size.x / 2, shape.size.y - offset);

        // Reticle
        m.setColor(255, 198, 31);
        m.drawCircle(shape.size.x / 2, offset + (shape.size.y - offset * 2) * reticlePositionNorm, (6 + 3 * A(reticleHover + externalHovered)));
        
		/*
        // MIXER - MONITOR DISPLAY
        r->setColor(200);
        //TODO: fix these crazy fixes for monitor->pitch display
        r->drawLine(c.getSize().x / 2 - c.getSize().x/6,
                    (std::max)((float)params->offset,(float)((-params->mixerPitch+90)/180.) * (std::min)(c.getSize().y+params->offset, c.getSize().y-params->offset)),
                    c.getSize().x / 2 + c.getSize().x /6,
					(std::max)((float)params->offset,(float)((-params->mixerPitch+90)/180.) * (std::min)(c.getSize().y+params->offset, c.getSize().y-params->offset))
                    );
        */

        // Action
        
        if ((ctx.mouseDownPressed[0]) && (inside) && (!draggingNow)) {
            draggingNow = true;
            cursorHide();
        }
        
        if (draggingNow) {
            if (abs(ctx.mouseDelta.y) >= 1) {
                *((float*)dataToControl) += ctx.mouseDelta.y / 2;
            }
            // TODO: why is the order inversed here?
            if (*((float*)dataToControl) < rangeTo) {
                *((float*)dataToControl) = rangeTo;
            }
            if (*((float*)dataToControl) > rangeFrom) {
                *((float*)dataToControl) = rangeFrom;
            }
        }

        if (draggingNow) { // drawing tooltip
            dataCache = *((float*)dataToControl);
            hintPosition = {c.currentViewShape.position.x - 5,
                            c.currentViewShape.position.y + (reticlePositionNorm * (c.currentViewShape.size.y - offset * 2))};
            ctx.addOverlay([&]() {
                m.setFont("Proxima Nova Reg.ttf", 8);
                m.draw<murka::Label>({hintPosition.x,
                    hintPosition.y,
                   75 /* */,
                   20 /* */}).text(
                                   std::to_string(int(dataCache))).commit();
            }, this);
            
            changed = true;
        }

        if ((draggingNow) && (!ctx.mouseDown[0])) {
            draggingNow = false;
            cursorShow();
        }

		m.popStyle();
    };
    
    float dataCache = 0.0; // for hint drawing
    MurkaPoint hintPosition = {0, 0};
    bool draggingNow = false;

    std::function<void()> cursorHide, cursorShow;
    float mixerPitch = 0;

//    Parameters() {}
//    Parameters(std::function<void()> cursrorHide_,
//               std::function<void()> cursorShow_,
//               float offset_,
//               float range_from,
//               float range_to,
//               bool externalHovered_,
//               float mixerPitch_ = 0) {
//        cursorHide = cursrorHide_;
//        cursorShow = cursorShow_;
//        offset = offset_;
//        rangeFrom = range_from;
//        rangeTo = range_to;
//        externalHovered = externalHovered_;
//        mixerPitch = mixerPitch_;
//    }
    
    
    
    bool hovered = false;
    
    bool changed = false;

    MURKA_PARAMETER(M1PitchWheel, // class name
                    float, // parameter type
                    rangeFrom, // parameter variable name
                    withRangeFrom, // setter
                    0 // default
    )
    MURKA_PARAMETER(M1PitchWheel, // class name
                    float, // parameter type
                    rangeTo, // parameter variable name
                    withRangeTo, // setter
                    1 // default
    )
    MURKA_PARAMETER(M1PitchWheel, // class name
                    float, // parameter type
                    offset, // parameter variable name
                    withOffset, // setter
                    0 // default
    )
    MURKA_PARAMETER(M1PitchWheel, // class name
                    bool, // parameter type
                    externalHovered, // parameter variable name
                    externallyHovered, // setter
                    false // default
    )
    MURKA_PARAMETER(M1PitchWheel, // class name
                    float*, // parameter type
                    dataToControl, // parameter variable name
                    controlling, // setter
                    nullptr // default
    )

};

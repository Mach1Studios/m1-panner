#pragma once

#include "MurkaView.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1PitchWheel : public murka::View<M1PitchWheel> {
public:
    void internalDraw(Murka & m) {

        bool isInside = inside();
        //* !areInteractiveChildrenHovered(c) *
        //hasMouseFocus(m);

        hovered = isInside + draggingNow;
        bool hoveredLocal = hovered + externalHovered; // this variable is not used outside the widget to avoid feedback loop
        changed = false; // false unless the user changed a value using this knob
		
		// Reticle calculation
		float reticlePositionNorm = (*((float*)dataToControl) - rangeFrom) / (rangeTo - rangeFrom);

		MurkaShape reticlePosition = { getSize().x / 2 - 6,
									  offset + (shape.size.y - offset * 2) * reticlePositionNorm - 6,
									  12,
									  12 };
		bool reticleHover = false + draggingNow;
		if (reticlePosition.inside(mousePosition())) {
			reticleHover = true;
		}

		m.pushStyle();
       
        int minilineStep = (shape.size.y - offset * 2) / 4;
        int minilineLength = shape.size.x / 6;
        int labelValue = 90;
        
        // Drawing labels
        
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-2);
		for (int i = 0; i <= 4; i++) {
            float lineLength = minilineLength / 2;
            if (i == 2) lineLength *= 2;
			m.setColor(57 + 20 * A(reticleHover + externalHovered));
            m.drawLine(getSize().x / 2 - lineLength,
                        i * minilineStep + offset,
                        getSize().x / 2 + lineLength,
                        i * minilineStep + offset);
            
            MurkaShape labelShape = {55, (i * minilineStep + 2), 150, 50};
            m.setColor(REF_LABEL_TEXT_COLOR);
            m.prepare<murka::Label>(labelShape).text(std::to_string(labelValue)).draw();
            labelValue -= 45;
        }
        
		m.setColor(133 + 20 * A(reticleHover + externalHovered));
        m.drawLine(getSize().x / 2, offset,
			shape.size.x / 2, shape.size.y - offset);

        // Reticle
        m.setColor(M1_ACTION_YELLOW);
        m.drawCircle(shape.size.x / 2, offset + (shape.size.y - offset * 2) * reticlePositionNorm, (6 + 3 * A(reticleHover + externalHovered)));
        
		/*
        // MIXER - MONITOR DISPLAY
        r->setColor(ENABLED_PARAM);
        //TODO: fix these crazy fixes for monitor->pitch display
        r->drawLine(c.getSize().x / 2 - c.getSize().x/6,
                    (std::max)((float)params->offset,(float)((-params->mixerPitch+90)/180.) * (std::min)(c.getSize().y+params->offset, c.getSize().y-params->offset)),
                    c.getSize().x / 2 + c.getSize().x /6,
					(std::max)((float)params->offset,(float)((-params->mixerPitch+90)/180.) * (std::min)(c.getSize().y+params->offset, c.getSize().y-params->offset))
                    );
        */

        // Action
        
        if ((mouseDownPressed(0)) && (inside()) && (!draggingNow)) {
            draggingNow = true;
            cursorHide();
        }
        
        if (draggingNow && mouseDelta().lengthSquared() > 0.001) {
            if (abs(mouseDelta().y) >= 1) {
                *((float*)dataToControl) += mouseDelta().y / 2;
            }
            // TODO: why is the order inversed here?
            if (*((float*)dataToControl) < rangeTo) {
                *((float*)dataToControl) = rangeTo;
            }
            if (*((float*)dataToControl) > rangeFrom) {
                *((float*)dataToControl) = rangeFrom;
            }
            // drawing tooltip
            dataCache = *((float*)dataToControl);
            hintPosition = {shape.position.x - 1,
                shape.position.y + (reticlePositionNorm * (shape.size.y - offset * 2) + 4)};
            addOverlay([&]() {
                m.setColor(LABEL_TEXT_COLOR);
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-2);
                m.prepare<murka::Label>({hintPosition.x,
                    hintPosition.y,
                   75 /* */,
                   20 /* */}).text(
                                   std::to_string(int(dataCache))).draw();
            }, this);
            
            changed = true;
        }

        if ((draggingNow) && (!mouseDown(0))) {
            draggingNow = false;
            cursorShow();
        }

		m.popStyle();
    }
    
    float dataCache = 0.0; // for hint drawing
    MurkaPoint hintPosition = {0, 0};
    bool draggingNow = false;

    std::function<void()> cursorHide, cursorShow;
    float mixerPitch = 0;
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

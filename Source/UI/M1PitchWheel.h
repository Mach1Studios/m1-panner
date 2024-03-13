#pragma once

#include "MurkaView.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1PitchWheel : public murka::View<M1PitchWheel> {
public:
    void internalDraw(Murka & m) {

        bool isInside = inside();
        changed = false; // false unless the user changed a value using this knob
        hovered = isInside + draggingNow;
        bool hoveredLocal = hovered + externalHovered; // this variable is not used outside the widget to avoid feedback loop
		
		// Reticle calculation
		float reticlePositionNorm = (*((float*)dataToControl) - rangeFrom) / (rangeTo - rangeFrom);

		MurkaShape reticlePosition = { getSize().x / 2 - 6,
									  offset + (shape.size.y - offset * 2) * reticlePositionNorm - 6,
									  12,
									  12 };
        

        bool reticleHover = false + draggingNow;

        if (!enabled) {
            hoveredLocal = false;
            reticleHover = false;
        } else {
            if (reticlePosition.inside(mousePosition())) {
                reticleHover = true;
            }
        }

		m.pushStyle();
        m.setLineWidth(1);
       
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
        
		m.setColor(100 + 90 * A(reticleHover + externalHovered) * enabled);
        m.drawLine(getSize().x / 2, offset, shape.size.x / 2, shape.size.y - offset);

        // Reticle
        if (enabled) {
            m.setColor(M1_ACTION_YELLOW);
        } else {
            m.setColor(DISABLED_PARAM);
        }
        m.drawCircle(shape.size.x / 2, offset + (shape.size.y - offset * 2) * reticlePositionNorm, (6 + 3 * A(reticleHover + externalHovered)));
        
        // MIXER - MONITOR DISPLAY
        if (isConnected && monitorState->monitor_mode != 1) {
            m.setColor(ENABLED_PARAM);
            float pitchNorm = normalize(monitorState->pitch, 90, -90); // inversed for UI
            pitchNorm = (pitchNorm > 0.975f) ? 0.975f : pitchNorm; // limit to the top bound of UI slider
            float topBottomPadding = 10; // to clamp the monitor preview within the slider
            m.drawLine(getSize().x/2 - getSize().x/6,
                       (std::max)(topBottomPadding, pitchNorm * getSize().y),
                       getSize().x/2 + getSize().x/6,
                       (std::max)(topBottomPadding, pitchNorm * getSize().y));
        }

        // Action
        
        if ((mouseDownPressed(0)) && (inside()) && (!draggingNow)) {
            draggingNow = true;
            cursorHide();
        }
        
        if (draggingNow) {
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
            hintPosition = {getAbsoluteViewPosition().x - 1,
                getAbsoluteViewPosition().y + (reticlePositionNorm * (getSize().y - offset * 2) + 4)};
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
    
    float normalize(float input, float min, float max){
        float normalized_x = (input-min)/(max-min);
        return normalized_x;
    }
    
    float dataCache = 0.0; // for hint drawing
    MurkaPoint hintPosition = {0, 0};
    bool draggingNow = false;

    std::function<void()> cursorHide, cursorShow;
    MixerSettings* monitorState = nullptr;
    bool isConnected = false;
    bool enabled = true;
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

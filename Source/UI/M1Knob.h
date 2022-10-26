#pragma once

#include "MurkaView.h"
#include "MurkaBasicWidgets.h"
#include "TextField.h"
#include "../Config.h"

using namespace murka;

class M1Knob : public murka::View<M1Knob> {
public:
    void internalDraw(Murka & m) {
        float* data = dataToControl;
        
        MurkaContext& ctx = m.currentContext;

        bool inside = ctx.isHovered() *
//        Had to temporary remove the areInteractiveChildrenHovered because of the bug in Murka with the non-deleting children widgets. TODO: fix this
//        !areInteractiveChildrenHovered(ctx) *
            hasMouseFocus(m)
            * (!editingTextNow);
        
        changed = false;

        hovered = inside + draggingNow; // for external views to be highlighted too if needed
        bool hoveredLocal = hovered + externalHover; // shouldn't propel hoveredLocal outside so it doesn't feedback

        
        if (!enabled) {
            hoveredLocal = false;
        }
        
        m.setColor(100 + 110 * enabled,
                    100 + 110 * enabled,
                    100 + 110 * enabled, 200);
        m.pushStyle();
        m.setLineWidth(4);
        m.enableFill();
        
        float width = 1.0;
        
        // "Outer" circle
        
        
        m.drawCircle(shape.size.x / 2,
                     shape.size.y * 0.35,
                     shape.size.x * (0.25 + A(0.01 * hoveredLocal)));
        
        // "Inner" circle
         
        m.setColor(BACKGROUND_GREY);
        m.drawCircle(shape.size.x / 2,
                     shape.size.y * 0.35,
                     shape.size.x * 0.25 - 2 * (width + A(0.5 * hoveredLocal)));
        m.setColor(BACKGROUND_GREY);
       // A grey colored rectangle that rotates
        
        m.pushMatrix();
        m.translate(shape.size.x / 2,
                    shape.size.y * 0.35,
                     0);
        
        float inputValueNormalised = ((*data - rangeFrom) / (rangeTo - rangeFrom));
        float inputValueAngleInDegrees = inputValueNormalised * 360;
        
        if (!isEndlessRotary) {
            inputValueAngleInDegrees = inputValueNormalised * 300 + 30;
        }
        
        m.rotateZRad(inputValueAngleInDegrees * (juce::MathConstants<float>::pi / 180));
        
        m.drawRectangle(-width * (4 + A(1 * inside)), 0, width * (8 + A(2 * inside)), shape.size.x * (0.25 + A(0.02 * inside)));
        
        // A white rectangle inside a grey colored one
        
        m.setColor(100 + 110 * enabled + A(30 * hoveredLocal) * enabled, 255);
        float w = A(width * (1 + 1 * hoveredLocal));
        m.drawRectangle(-w, 0, w * 2, shape.size.x * 0.26);
        m.popMatrix();
        
        m.popStyle();
        
        m.setColor(100 + 110 * enabled + A(30 * hoveredLocal), 255);
        
        auto labelPositionY = shape.size.x * 0.8 / width + 10;

        std::string displayString = float_to_string(*data, floatingPointPrecision);
        
        
        std::function<void()> deleteTheTextField = [&]() {
            // Temporary solution to delete the TextField:
            // Searching for an id to delete the text field widget.
            // To be redone after the UI library refactoring.
            
            imIdentifier idToDelete;
            for (auto childTuple: imChildren) {
                auto childIdTuple = childTuple.first;
                if (std::get<1>(childIdTuple) == typeid(TextField).name()) {
                    idToDelete = childIdTuple;
                }
            }
            imChildren.erase(idToDelete);
        };
         
        
        std::string valueText = prefix + displayString + postfix;
 
        auto font = m.getCurrentFont();
        auto valueTextBbox = font->getStringBoundingBox(valueText, 0, 0);

        MurkaShape valueTextShape = { shape.size.x / 2 - valueTextBbox.width / 2 - 5,
                                     shape.size.x * 0.8 / width + 10,
                                     valueTextBbox.width + 10,
                                     valueTextBbox.height };
        
        if (editingTextNow) {
            auto& textFieldObject =
                m.draw<TextField>({ valueTextShape.x() - 5, valueTextShape.y() - 5, 
                    valueTextShape.width() + 10, valueTextShape.height() + 10 })
                .controlling(data)
                .withPrecision(2)
                .forcingEditorToSelectAll(shouldForceEditorToSelectAll)
                .onlyAllowNumbers(true)
                .commit();
            
            
            auto textFieldResults = textFieldObject.results;
            
            if (shouldForceEditorToSelectAll) {
                // We force selection by sending the value to text editor field
                shouldForceEditorToSelectAll = false;
            }
            
            if (!textFieldResults) {
                textFieldObject.activated = true;
                ctx.claimKeyboardFocus(&textFieldObject);
            }
            
            if (textFieldResults) {
                editingTextNow = false;

                deleteTheTextField();
            }
        } else {
            m.draw<murka::Label>({0, shape.size.x * 0.8 / width + 10,
                shape.size.x / width, shape.size.y * 0.5})
                .withAlignment(TEXT_CENTER).text(valueText)
                .commit();
        }
        

        bool hoveredValueText = false;
        if (valueTextShape.inside(m.currentContext.mousePosition) && !editingTextNow) {
            m.drawRectangle(valueTextShape.x() - 2,
                             valueTextShape.y(),
                             2,
                             2);
            m.drawRectangle(valueTextShape.x() + valueTextShape.width() + 2,
                             valueTextShape.y(),
                             2,
                             2);
            m.drawRectangle(valueTextShape.x() - 2,
                             valueTextShape.y() + valueTextShape.height(),
                             2,
                             2);
            m.drawRectangle(valueTextShape.x() + valueTextShape.width() + 2,
                             valueTextShape.y() + valueTextShape.height(),
                             2,
                             2);
            hoveredValueText = true;
        }
        
        
        // Action

        if ((m.currentContext.mouseDownPressed[0]) && (!m.currentContext.isHovered()) && (editingTextNow)) {
            // Pressed outside the knob widget while editing text. Aborting the text edit
            editingTextNow = false;
            deleteTheTextField();
        }
        
        std::string debugStr = "";
        if ((hoveredValueText) && (m.currentContext.doubleClick)) {
            editingTextNow = true;
            shouldForceEditorToSelectAll = true;
        }
        
        
        if ((m.currentContext.mouseDownPressed[0]) && (inside) && (m.currentContext.mousePosition.y < labelPositionY) &&
            (!draggingNow) && (enabled)) {
            draggingNow = true;
            cursorHide();
        }

        if ((draggingNow) && (!m.currentContext.mouseDown[0])) {
            draggingNow = false;
            cursorShow();
        }
        
        // Setting knob value to default if double clicked or pressed alt while clicking
        
        bool shouldSetDefault = (m.currentContext.doubleClick ||
                                false // (context.isKeyPressed(ofKey::OF_KEY_ALT) && context.mouseDownPressed[0])
								);
        
        // Don't set default by doubleclick if the mouse is in the Label/Text editor zone
        if (m.currentContext.mousePosition.y >= labelPositionY) shouldSetDefault = false;

        if (shouldSetDefault && inside) {
            draggingNow = false;
            *((float*)dataToControl) = defaultValue;
            cursorShow();
            changed = true;
        }
        
        if (draggingNow) {
            if (abs(m.currentContext.mouseDelta.y) >= 1) {
                
                float s = speed;  // TODO: check if this speed constant should be dependent on UIScale
                /*
                if (context.isKeyPressed(OF_KEY_SHIFT) ||
                    context.isKeyPressed(OF_KEY_LEFT_SHIFT) ||
                    context.isKeyPressed(OF_KEY_RIGHT_SHIFT)) {
                    speed *= 4;
                }
                */
                *((float*)dataToControl) += (m.currentContext.mouseDelta.y / s) * (rangeTo - rangeFrom);
            }
            if (*((float*)dataToControl) > rangeTo) {
                if (isEndlessRotary) {
                    *((float*)dataToControl) -= (rangeTo - rangeFrom);
                } else {
                    *((float*)dataToControl) = rangeTo;
                }
            }
            if (*((float*)dataToControl) < rangeFrom) {
                if (isEndlessRotary) {
                    *((float*)dataToControl) += (rangeTo - rangeFrom);
                } else {
                    *((float*)dataToControl) = rangeFrom;
                }
            }
            
            changed = true;
        }

    };
    
    std::stringstream converterStringStream;
    
    std::string float_to_string(float input, int precision) {
        converterStringStream.str(std::string());
        converterStringStream << std::fixed << std::setprecision(precision) << input;
        return (converterStringStream.str());
    }
    
    bool draggingNow = false;
    bool editingTextNow = false;
    bool shouldForceEditorToSelectAll = false;

    float rangeFrom = 0;
    float rangeTo = 1;

    std::string postfix = "";
    std::string prefix = "";
    
    float speed = 250.;
    
    bool enabled = true;
    bool externalHover = false; // for Pitch wheel to control the knob
    
    int floatingPointPrecision = 0;
    
    std::function<void()> cursorHide, cursorShow;
    
    bool isEndlessRotary = false;
    
    float defaultValue = 0;
    
    float* dataToControl = nullptr;
    
    bool changed = false;
    
    M1Knob & withParameters(float rangeFrom_,
                            float rangeTo_,
                            std::string postfix_ = "",
                            std::string prefix_ = "",
                            int floatingPointPrecision_ = 0,
                            float speed_ = 250.,
                            float defaultValue_ = 0,
                            bool isEndlessRotary_ = false,
                            bool enabled_ = true,
                            bool externalHover_ = false,
                            std::function<void()> cursrorHide_ = [](){},
                            std::function<void()> cursorShow_ = [](){}) {
//        parameterName = parameterName_;
        rangeFrom = rangeFrom_;
        rangeTo = rangeTo_;
        postfix = postfix_;
        prefix = prefix_;
        speed = speed_;
        defaultValue = defaultValue_;
        isEndlessRotary = isEndlessRotary_;
        enabled = enabled_;
        externalHover = externalHover_;
        cursorHide = cursrorHide_;
        cursorShow = cursorShow_;
        
        return *this;
    }
    
    M1Knob & controlling(float* dataPointer) {
        dataToControl = dataPointer;
        
        return *this;
    }
    
    bool hovered = false;
    
};

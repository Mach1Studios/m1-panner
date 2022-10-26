#pragma once

#include "Murka.h"

using namespace murka;

typedef std::tuple<float&, float&, float&, float&> XYRZ;

class OverlayReticleField : public murka::View<OverlayReticleField> {
public:
    void internalDraw(Murka & m) {
            MurkaContext& context = m.currentContext;
            bool inside = context.isHovered() * !areInteractiveChildrenHovered(context) * hasMouseFocus(m);
            XYRZ *xyrz = (XYRZ*)dataToControl;

            m.enableFill();

            MurkaPoint reticlePositionInWidgetSpace = {
				context.getSize().x / 2 + (std::get<2>(*xyrz) / 180.) * context.getSize().x / 2,
                context.getSize().y / 2 + (-std::get<3>(*xyrz) / 90.) * context.getSize().y / 2
			};

            auto center = MurkaPoint(context.getSize().x / 2, context.getSize().y / 2);

             if ((draggingNow) || (shouldDrawDivergeLine)) {
                m.setColor(GRID_LINES_1_RGBA);
                m.disableFill();
				auto x = context.getSize().x;
                m.drawLine(reticlePositionInWidgetSpace.x, 0, reticlePositionInWidgetSpace.x, context.getSize().y);
				m.drawLine(0, reticlePositionInWidgetSpace.y, context.getSize().x, reticlePositionInWidgetSpace.y);
			 }
        
            bool reticleHovered = MurkaShape(reticlePositionInWidgetSpace.x - 10,
                                             reticlePositionInWidgetSpace.y - 10,
                                             20,
                                             20).inside(context.mousePosition) + draggingNow;
        
            // Drawing central reticle
            m.enableFill();
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (10 + 3 * A(reticleHovered)));
            m.setColor(BACKGROUND_GREY); // background color for internal circle
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (8 + 3 * A(reticleHovered)));
            
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, 6);

            // Draw additional reticles for each input channel
            std::vector<std::string> pointsNames = m1Encode->getPointsNames();
            std::vector<Mach1Point3D> points = m1Encode->getPoints();
            for (int i = 0; i < m1Encode->getPointsCount(); i++) {
                drawAdditionalReticle((points[i].z) * shape.size.x, (1.0-points[i].x) * shape.size.y, pointsNames[i], reticleHovered, m, context);
            }

            // MIXER - MONITOR DISPLAY
            if (monitorState->monitor_mode != 2){
                drawMonitorYaw(monitorState->yaw - 180., monitorState->pitch, m); //TODO: Why do we need -180?
            }
        
            reticleHoveredLastFrame = reticleHovered;
        
            // Action
        
            if ((reticleHovered) && (inside) && (context.mouseDownPressed[0])) {
                draggingNow = true;
                cursorHide();
            }
        
            if ((draggingNow) && (!context.mouseDown[0])) {
                draggingNow = false;
                cursorShow();
                teleportCursor(context.currentViewShape.position.x + reticlePositionInWidgetSpace.x, context.currentViewShape.position.x + reticlePositionInWidgetSpace.y);
            }
        
            if (draggingNow) {
                MurkaPoint normalisedDelta = {context.mouseDelta.x / context.getSize().x,
                                              context.mouseDelta.y / context.getSize().y};
                std::get<2>(*xyrz) -= normalisedDelta.x * 180 * 2;
                std::get<3>(*xyrz) += normalisedDelta.y * 90 * 2;

                changed = true;
            }
        
            if (context.doubleClick) {
                MurkaPoint normalisedMouse = {context.mousePosition.x / context.getSize().x,
                                              context.mousePosition.y / context.getSize().y};
                std::get<2>(*xyrz) = 180 * 2 * (normalisedMouse.x  - 0.5);
                std::get<3>(*xyrz) = 90 * 2 * (-normalisedMouse.y + 0.5);
                
                changed = true;
            }
	}
    
    void clampPoint(MurkaPoint& input, float min, float max) {
        if (input.x < min) input.x = min;
        if (input.x > max) input.x = max;
        if (input.y < min) input.y = min;
        if (input.y > max) input.y = max;
    };
    
    float normalize(float input, float min, float max){
        float normalized_x = (input-min)/(max-min);
        return normalized_x;
    };
    
    void drawAdditionalReticle(float x, float y, std::string label, bool reticleHovered, Murka& m, MurkaContext& context) {
        float realx = x;
        float realy = y;
        
        m.setColor(M1_ACTION_YELLOW);
        m.disableFill();
        m.drawCircle(realx-1, realy-1, (10 + 3 * A(reticleHovered)));
        
        if (realx+14 > context.getSize().x){
            //draw rollover shape on left side
            float left_rollover = (realx+14)-context.getSize().x;
            m.drawCircle(left_rollover-14, realy+5, (10 + 3 * A(reticleHovered)));
        }
        if (realx-14 < 0){
            //draw rollover shape on right side
            float right_rollover = abs(realx - 14);
            float width = context.getSize().x;
            m.drawCircle(width-right_rollover+14, realy+5, (10 + 3 * A(reticleHovered)));
        }
        
        m.setFont("Proxima Nova Reg.ttf", (10 + 2 * A(reticleHovered)));
        m.setColor(M1_ACTION_YELLOW);
        m.disableFill();
        M1Label& l = m.draw<M1Label>(MurkaShape(realx-9, realy-7 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).commit();
        
        if (realx + 20 > context.getSize().x){
            //draw rollover shape on left side
            float left_rollover = (realx+8)-context.getSize().x;
            m.draw<M1Label>(MurkaShape(left_rollover-16, realy-2 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).commit();
        }
        if (realx-20 < 0){
            //draw rollover shape on right side
            float right_rollover = abs(realx-8);
            float width = context.getSize().x;
            m.draw<M1Label>(MurkaShape(width-right_rollover, realy-2 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).commit();
        }
    };
    
    void drawMonitorYaw(float yawAngle, float pitchAngle, Murka& m){
        MurkaContext& context = m.currentContext;
        float yaw = normalize(yawAngle, -180., 180.); //TODO: fix this
        float pitch = pitchAngle + 90.; //TODO: fix this
        float divider = 4.; // Diameter/width of drawn object
        m.setColor(REF_LABEL_TEXT_COLOR);
        float halfWidth = context.getSize().x/(divider*2);
        float halfHeight = context.getSize().y/(divider*2);
        float centerPos = fmod(yaw+0.5, 1.) * context.getSize().x;
        m.drawRectangle( centerPos - halfWidth, ((pitch / 180.) * context.getSize().y) - halfHeight, context.getSize().x/divider, context.getSize().y/divider );
        if (centerPos+halfWidth > context.getSize().x){
            //draw rollover shape on left side
            float left_rollover = (centerPos+halfWidth)-context.getSize().x;
            m.drawRectangle(0., ((pitch / 180.) * context.getSize().y) - halfHeight, left_rollover, context.getSize().y/divider);
        }
        if (centerPos-halfWidth < 0){
            //draw rollover shape on right side
            float right_rollover = abs(centerPos - halfWidth);
            float width = context.getSize().x;
            m.drawRectangle(width-right_rollover,
                            ((pitch / 180.) * context.getSize().y) - halfHeight,
                            right_rollover,
                            context.getSize().y/divider);
        }
    };
    
    bool draggingNow = false;
    bool reticleHoveredLastFrame = false;
    bool changed = false;
    
    XYRZ* dataToControl = nullptr;
    
    OverlayReticleField & controlling (XYRZ* dataToControl_) {
        dataToControl = dataToControl_;
        return *this;
    }
    
    float somevalue = 0.0;
    std::function<void()> cursorHide;
    std::function<void()> cursorShow;
    std::function<void(int, int)> teleportCursor;
    bool shouldDrawDivergeLine = false;
    bool shouldDrawRotateLine = false;
    float sRotate = 0, sSpread = 50;
    Mach1Encode* m1Encode = nullptr;
    PannerSettings* pannerState = nullptr;
    MixerSettings* monitorState = nullptr;
};

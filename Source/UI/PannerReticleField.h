#pragma once

#include "MurkaView.h"
#include "MurkaBasicWidgets.h"
#include "Mach1Encode.h"

// TODO: Click & Drag should hide cursor and continue dragging from last reticle position
// TODO: Smoother curved lines

using namespace murka;

typedef std::tuple<float&, float&, float&, float&> XYRD;

class PannerReticleField : public murka::View<PannerReticleField> {
public:
    PannerReticleField() {

    }
    
    void internalDraw(Murka & m) {
            MurkaContext& context = m.currentContext;
            bool inside = context.isHovered() * !areInteractiveChildrenHovered(context) * hasMouseFocus(m);
            XYRD *xyrd = (XYRD*)dataToControl;

            m.setColor(GRID_LINES_1_RGBA);
            auto linestep = context.getSize().x / (96);
            for (int i = 0; i < (96); i++) {
                if (monitorState->monitor_mode != 2){
                    m.drawLine(linestep * i, 0, linestep * i, shape.size.y);
                    m.drawLine(0, linestep * i, shape.size.x, linestep * i);
                }
            } 

            m.setColor(GRID_LINES_2);
            linestep = shape.size.x / 4;
            for (int i = 1; i < 4; i++) {
                m.drawLine(linestep * i, 0, linestep * i, shape.size.y);
                m.drawLine(0, linestep * i, shape.size.x, linestep * i);
            }
        
            m.setColor(GRID_LINES_4_RGB);
            m.drawLine(0, 0, context.getSize().x, context.getSize().y);
            m.drawLine(context.getSize().x, 0, 0, context.getSize().y);
        
            // GUIDE CIRCLES
            if (monitorState->monitor_mode != 2){
                //inside circle
                m.disableFill();
                m.drawCircle(context.getSize().x / 2, context.getSize().y / 2, context.getSize().y / 4);
                //outside circle
                m.drawCircle(shape.size.x / 2, shape.size.y / 2, shape.size.y / 2);
                //middle circle
                //r->drawCircle(context.getSize().x / 2, context.getSize().y / 2, sqrt(pow(context.getSize().y / 4, 2) + pow(context.getSize().y / 4, 2)));
            }
            m.enableFill();

            // GRID FILLS
            if (monitorState->monitor_mode != 2){
                m.setColor(BACKGROUND_GREY);
                m.drawRectangle((context.getSize().x/2) - 10, 0, 20, 10);
                m.drawRectangle((context.getSize().x/2) - 10, context.getSize().y - 10, 20, 10);
            }
        
            // LARGE GRID LINES
            m.setColor(GRID_LINES_4_RGB);
            linestep = context.getSize().x / 4;
            for (int i = 1; i < 4; i++) {
                for (int j = 1; j < 4; j++) {
                    m.drawLine(linestep * i, 0, linestep * i, context.getSize().y);
                    m.drawLine(0, linestep * i, context.getSize().x, linestep * i);
                }
            }
        
            // GRID LABELS
            if (monitorState->monitor_mode != 2){
                m.setColor(BACKGROUND_GREY);
                m.drawRectangle((context.getSize().x/2) - 10, 0, 20, 12);
                m.drawRectangle((context.getSize().x/2) - 12, context.getSize().y - 15, 25, 20);
                m.setFont("Proxima Nova Reg.ttf", 8.5);
                m.setColor(REF_LABEL_TEXT_COLOR);
                MurkaShape zeroLabelShape = {(context.getSize().x/2) - 7.5, -2, 25, 15};
                m.draw<murka::Label>(zeroLabelShape).text({"0"}).commit();
                MurkaShape oneEightyLabelShape = {(context.getSize().x/2) - 13, context.getSize().y - 12, 30, 15};
                m.draw<murka::Label>(oneEightyLabelShape).text("180").commit();
            }
 
			// MIXER - MONITOR DISPLAY
			/*
			{
				MurkaPoint center = { c.getSize().x / 2,
									   c.getSize().x / 2 };

				std::vector< MurkaPoint> vects;
				// arc
				{
					float centerX = 0;
					float centerY = 0;
					float radiusX = center.x - 1;
					float radiusY = center.y - 1;
					for (int i = 45; i <= 90 + 45; i += 5) {
						float x = centerX + (radiusX * cosf(degreesToRadians(1.0 * i)));
						float y = centerY + (radiusY * sinf(degreesToRadians(1.0 * i)));
						vects.push_back(MurkaPoint(x, y));
					}
				}

				r->pushStyle();
				r->pushMatrix();
				r->translate(center.x, center.y, 0);
				r->rotateZRad(degreesToRadians(params->mixerYaw - 180)); //TODO: Why do we need -180?
				r->setColor(ENABLED_PARAM);

				// temp
				for (size_t i = 1; i < vects.size(); i++) {
					r->drawLine(vects[i-1].x, vects[i - 1].y, vects[i].x, vects[i].y);
				} 
				//r->drawPath(*(vector<MurkaPoint>*)&vects);

				r->drawLine(0, 0, 0, center.y - 10);
				r->popMatrix();
				r->popStyle();
			}
			*/
        
            MurkaPoint reticlePositionInWidgetSpace = {context.getSize().x / 2 + (std::get<0>(*xyrd) / 100.) * context.getSize().x / 2,
                context.getSize().y / 2 + (-std::get<1>(*xyrd) / 100.) * context.getSize().y / 2};
            auto center = MurkaPoint(context.getSize().x / 2,
                                 context.getSize().y / 2);

            if ((draggingNow) || (shouldDrawRotateGuideLine)) {
                m.setColor(M1_ACTION_YELLOW);
                m.disableFill();
                m.drawCircle(center.x,
                             center.y, (std::get<3>(*xyrd) / 100.) * context.getSize().x / sqrt(2));
            } 
        
            if ((draggingNow) || (shouldDrawDivergeGuideLine)) {
                m.setColor(M1_ACTION_YELLOW);
                m.disableFill();
                auto center = MurkaPoint(shape.size.x / 2,
                                         shape.size.y / 2);
                m.drawLine((reticlePositionInWidgetSpace.x - center.x) * 30 + center.x,
                            (reticlePositionInWidgetSpace.y - center.y) * 30 + center.y,
                            -(reticlePositionInWidgetSpace.x - center.x) * 30 + center.x,
                            -(reticlePositionInWidgetSpace.y - center.y) * 30 + center.y);
            }
        
            bool reticleHovered = MurkaShape(reticlePositionInWidgetSpace.x - 10,
                                             reticlePositionInWidgetSpace.y - 10,
                                             20,
                                             20).inside(context.mousePosition) + draggingNow;
        
            // Drawing central reticle
            m.enableFill();
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (10 + 3 * A(reticleHovered) + (2 * (elevation/90))));
            m.setColor(BACKGROUND_GREY); // background color for internal circle
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (8 + 3 * A(reticleHovered) + (2 * (elevation/90))));
            
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, 6);
        
            // Reticles
			float angle = autoOrbit ? -atan2((reticlePositionInWidgetSpace - center).x, (reticlePositionInWidgetSpace - center).y) : juce::degreesToRadians(sRotate);
			//float widgetSpaceSpread = float(shape.size.x) / 200.;
			std::vector<std::string> pointsNames = m1Encode->getPointsNames();
			std::vector<Mach1Point3D> points = m1Encode->getPoints();
			for (int i = 0; i < m1Encode->getPointsCount(); i++) {
				drawAdditionalReticle((points[i].z) * shape.size.x, (1.0-points[i].x) * shape.size.y, pointsNames[i], reticleHovered, false, m);
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
                teleportCursor(shape.position.x +
                                    reticlePositionInWidgetSpace.x,
                               shape.position.x +
                                    reticlePositionInWidgetSpace.y);
            }
        
            if (draggingNow) {
                MurkaPoint normalisedDelta = {context.mouseDelta.x /
                                                shape.size.x,
                                              context.mouseDelta.y /
                                                shape.size.y};
                std::get<0>(*xyrd) -= normalisedDelta.x * 200;
                std::get<1>(*xyrd) += normalisedDelta.y * 200;
                
                results = true;
            }
        
            //TODO: make double click only within the bounds of the reticlegrid
            if ((context.doubleClick) && (inside)) {
                MurkaPoint normalisedMouse = {context.mousePosition.x / shape.size.x,
                                              context.mousePosition.y / shape.size.y};
                std::get<0>(*xyrd) = normalisedMouse.x * 200 - 100;
                std::get<1>(*xyrd) = -normalisedMouse.y * 200 + 100;
                
                draggingNow = true;
                cursorHide();
                
                results = true;
            }
    }
    
    void clampPoint(MurkaPoint& input, float min, float max) {
        if (input.x < min) input.x = min;
        if (input.x > max) input.x = max;
        if (input.y < min) input.y = min;
        if (input.y > max) input.y = max;
    };
    
    void drawAdditionalReticle(float x, float y, std::string label, bool reticleHovered, bool largeReticles, Murka& m) {
        m.setFont("Proxima Nova Reg.ttf", (10 + 2 * A(reticleHovered) + (2 * (elevation/90))));
        m.setColor(M1_ACTION_YELLOW);
        m.disableFill();
        
        if (largeReticles){
            m.drawCircle(x, y, (20 + 3 * A(reticleHovered) + (2 * (elevation/90))));
        } else {
            m.drawCircle(x, y, (10 + 3 * A(reticleHovered) + (2 * (elevation/90))));
        }
        m.draw<murka::Label>(MurkaShape(x - 8 - (2 * (elevation/90)), y - 8 - 2 * A(reticleHovered) - (2 * (elevation/90)), 50, 50)).text(label.c_str()).commit();
    };
    
    PannerReticleField & controlling (XYRD *dataToControl_) {
        dataToControl = dataToControl_;
        return *this;
    }
    
    XYRD *dataToControl;
    
    bool draggingNow = false;
    bool reticleHoveredLastFrame = false;
    bool results = false;
    
    float somevalue = 0.0;
    std::function<void()> cursorHide;
    std::function<void()> cursorShow;
    std::function<void(int, int)> teleportCursor;
    bool shouldDrawDivergeGuideLine = false;
    bool shouldDrawRotateGuideLine = false;
    Mach1Encode* m1Encode = nullptr;
    PannerSettings* pannerState = nullptr;
    MixerSettings* monitorState = nullptr;
    bool autoOrbit = false;
    float azimuth = 0, elevation = 0, diverge = 0, sRotate = 0, sSpread = 50;

    // The results type, you also need to define it even if it's nothing.
    typedef bool Results;
};

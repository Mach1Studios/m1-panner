#pragma once

#include "Murka.h"

using namespace murka;

typedef std::tuple<float&, float&, float&, float&> XYRZ;

class OverlayReticleField : public murka::View<OverlayReticleField>
{
public:
    void internalDraw(Murka& m)
    {
        bool isInside = inside() * hasMouseFocus(m);
        XYRZ* xyrz = (XYRZ*)dataToControl;

        m.enableFill();

        MurkaPoint reticlePositionInWidgetSpace = {
            getSize().x / 2 + (std::get<2>(*xyrz) / 180.) * getSize().x / 2,
            getSize().y / 2 + (-std::get<3>(*xyrz) / 90.) * getSize().y / 2
        };

        auto center = MurkaPoint(getSize().x / 2, getSize().y / 2);

        if ((draggingNow) || (shouldDrawDivergeLine))
        {
            m.setColor(GRID_LINES_1_RGBA);
            m.disableFill();
            m.drawLine(reticlePositionInWidgetSpace.x, 0, reticlePositionInWidgetSpace.x, getSize().y);
            m.drawLine(0, reticlePositionInWidgetSpace.y, getSize().x, reticlePositionInWidgetSpace.y);
        }

        bool reticleHovered = MurkaShape(reticlePositionInWidgetSpace.x - 10,
                                  reticlePositionInWidgetSpace.y - 10,
                                  20,
                                  20)
                                  .inside(mousePosition())
                              + draggingNow;

        // Drawing central reticle
        m.enableFill();
        m.setColor(M1_ACTION_YELLOW);
        m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (10 + 3 * A(reticleHovered)));
        m.setColor(BACKGROUND_GREY); // background color for internal circle
        m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (8 + 3 * A(reticleHovered)));
        m.setColor(M1_ACTION_YELLOW);
        m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, 6);

        // rollover central reticle draw logic
        if (reticlePositionInWidgetSpace.x > getSize().x)
        {
            //draw rollover shape on left side
            float left_rollover = (reticlePositionInWidgetSpace.x + 14) - getSize().x;
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(left_rollover - 14, reticlePositionInWidgetSpace.y, (10 + 3 * A(reticleHovered)));
            m.setColor(BACKGROUND_GREY); // background color for internal circle
            m.drawCircle(left_rollover - 14, reticlePositionInWidgetSpace.y, (8 + 3 * A(reticleHovered)));
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(left_rollover - 14, reticlePositionInWidgetSpace.y, 6);
        }
        if (reticlePositionInWidgetSpace.x < 0)
        {
            //draw rollover shape on right side
            float right_rollover = abs(reticlePositionInWidgetSpace.x - 14);
            float left_rollover = (reticlePositionInWidgetSpace.x + 14) - getSize().x;
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(getSize().x - right_rollover + 14, reticlePositionInWidgetSpace.y, (10 + 3 * A(reticleHovered)));
            m.setColor(BACKGROUND_GREY); // background color for internal circle
            m.drawCircle(getSize().x - right_rollover + 14, reticlePositionInWidgetSpace.y, (8 + 3 * A(reticleHovered)));
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(getSize().x - right_rollover + 14, reticlePositionInWidgetSpace.y, 6);
        }

        // Draw additional reticles for each input channel
        std::vector<std::string> pointsNames = m1Encode->getPointsNames();
        std::vector<Mach1Point3D> points = m1Encode->getPoints();
        if (m1Encode->getPointsCount() > 1)
        { // do not draw additional points if only mono input
            for (int i = 0; i < m1Encode->getPointsCount(); i++)
            {
                float r, d;
                float x = points[i].z;
                float y = points[i].x;

                if (pointsNames[i] == "LFE") {
                    continue;
                }

                if (x == 0 && y == 0)
                {
                    r = 0;
                    d = 0;
                }
                else
                {
                    d = sqrtf(x * x + y * y) / sqrt(2.0);
                    float rotation_radian = atan2(x, y); //acos(x/d);
                    float rotation_degree = juce::radiansToDegrees(rotation_radian);
                    r = (rotation_degree / 360.) + 0.5; // normalize 0->1
                }
                drawAdditionalReticle(r * shape.size.x, (-points[i].y + 1.0) / 2 * shape.size.y, pointsNames[i], reticleHovered, m);
            }
        }

        // MIXER - MONITOR DISPLAY
        if (isConnected && monitorState->monitor_mode != 1)
        {
            drawMonitorYaw(monitorState->yaw - 180., -monitorState->pitch, m);
        }

        reticleHoveredLastFrame = reticleHovered;

        // Action

        if ((reticleHovered) && (inside()) && (mouseDownPressed(0)))
        {
            draggingNow = true;
            cursorHide();
        }

        if ((draggingNow) && (!mouseDown(0)))
        {
            draggingNow = false;
            cursorShow();
        }

        if (draggingNow)
        {
            MurkaPoint normalisedDelta = { mouseDelta().x / getSize().x,
                mouseDelta().y / getSize().y };
            if (mouseDelta().lengthSquared() > 0.001)
            {
                std::get<2>(*xyrz) -= normalisedDelta.x * 180 * 2;
                std::get<3>(*xyrz) += normalisedDelta.y * 90 * 2;

                changed = true;
            }
        }

        if (doubleClick())
        {
            MurkaPoint normalisedMouse = { mousePosition().x / getSize().x,
                mousePosition().y / getSize().y };
            std::get<2>(*xyrz) = 180 * 2 * (normalisedMouse.x - 0.5);
            std::get<3>(*xyrz) = 90 * 2 * (-normalisedMouse.y + 0.5);

            changed = true;
        }
    }

    void clampPoint(MurkaPoint& input, float min, float max)
    {
        if (input.x < min)
            input.x = min;
        if (input.x > max)
            input.x = max;
        if (input.y < min)
            input.y = min;
        if (input.y > max)
            input.y = max;
    }

    float normalize(float input, float min, float max)
    {
        float normalized_x = (input - min) / (max - min);
        return normalized_x;
    }

    void drawAdditionalReticle(float x, float y, std::string label, bool reticleHovered, Murka& m)
    {
        float realx = x;
        float realy = y;

        if (isConnected) {
            m.setColor(MurkaColor(track_color.red, track_color.green, track_color.blue, track_color.alpha));
        } else {
            m.setColor(M1_ACTION_YELLOW);
        }
        m.disableFill();
        m.drawCircle(realx - 1, realy - 1, (10 + 3 * A(reticleHovered)));

        if (realx + 14 > getSize().x)
        {
            //draw rollover shape on left side
            float left_rollover = (realx + 14) - getSize().x;
            m.drawCircle(left_rollover - 14, realy + 5, (10 + 3 * A(reticleHovered)));
        }
        if (realx - 14 < 0)
        {
            //draw rollover shape on right side
            float right_rollover = abs(realx - 14);
            m.drawCircle(getSize().x - right_rollover + 14, realy + 5, (10 + 3 * A(reticleHovered)));
        }

        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, (DEFAULT_FONT_SIZE + 2 * A(reticleHovered)));
        M1Label& l = m.prepare<M1Label>(MurkaShape(realx - 9, realy - 7 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).draw();

        if (realx + 20 > getSize().x)
        {
            //draw rollover shape on left side
            float left_rollover = (realx + 8) - getSize().x;
            m.prepare<M1Label>(MurkaShape(left_rollover - 16, realy - 2 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).draw();
        }
        if (realx - 20 < 0)
        {
            //draw rollover shape on right side
            float right_rollover = abs(realx - 8);
            m.prepare<M1Label>(MurkaShape(getSize().x - right_rollover, realy - 2 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).draw();
        }
    }

    void drawMonitorYaw(float yawAngle, float pitchAngle, Murka& m)
    {
        float yaw = normalize(yawAngle, -180., 180.); //TODO: fix this
        float pitch = pitchAngle + 90.; //TODO: fix this
        float divider = 4.; // Diameter/width of drawn object
        m.setColor(OVERLAY_YAW_REF_RGBA);
        float halfWidth = getSize().x / (divider * 2);
        float halfHeight = getSize().y / (divider * 2);
        float centerPos = fmod(yaw + 0.5, 1.) * getSize().x;
        m.drawRectangle(centerPos - halfWidth, ((pitch / 180.) * getSize().y) - halfHeight, getSize().x / divider, getSize().y / divider);
        if (centerPos + halfWidth > getSize().x)
        {
            //draw rollover shape on left side
            float left_rollover = (centerPos + halfWidth) - getSize().x;
            m.drawRectangle(0., ((pitch / 180.) * getSize().y) - halfHeight, left_rollover, getSize().y / divider);
        }
        if (centerPos - halfWidth < 0)
        {
            //draw rollover shape on right side
            float right_rollover = abs(centerPos - halfWidth);
            m.drawRectangle(getSize().x - right_rollover,
                ((pitch / 180.) * getSize().y) - halfHeight,
                right_rollover,
                getSize().y / divider);
        }
    }

    bool draggingNow = false;
    bool reticleHoveredLastFrame = false;
    bool changed = false;

    XYRZ* dataToControl = nullptr;

    OverlayReticleField& controlling(XYRZ* dataToControl_)
    {
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
    Mach1Encode<float>* m1Encode = nullptr;
    PannerSettings* pannerState = nullptr;
    MixerSettings* monitorState = nullptr;
    bool isConnected = false;
    juce::OSCColour track_color;
};

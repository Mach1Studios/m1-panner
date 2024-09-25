#pragma once

#include "Mach1Encode.h"
#include "MurkaBasicWidgets.h"
#include "MurkaView.h"

using namespace murka;

typedef std::tuple<float&, float&, float&, float&> XYRD;

class PannerReticleField : public murka::View<PannerReticleField>
{
public:
    PannerReticleField()
    {
    }

    void internalDraw(Murka& m)
    {
        results = false;

        m1encodeUpdate();

        XYRD* xyrd = (XYRD*)dataToControl;

        // Increase circle resolution
        m.setCircleResolution(64);

        m.setLineWidth(1);

        m.setColor(GRID_LINES_1_RGBA);
        auto linestep = getSize().x / (96);
        for (int i = 0; i < (96); i++)
        {
            if ((monitorState->monitor_mode >= 0 && monitorState->monitor_mode < 3) && monitorState->monitor_mode != 1)
            {
                m.drawLine(linestep * i, 0, linestep * i, shape.size.y);
                m.drawLine(0, linestep * i, shape.size.x, linestep * i);
            }
        }

        m.setColor(GRID_LINES_2);
        linestep = shape.size.x / 4;
        for (int i = 1; i < 4; i++)
        {
            m.drawLine(linestep * i, 0, linestep * i, shape.size.y);
            m.drawLine(0, linestep * i, shape.size.x, linestep * i);
        }

        m.setColor(GRID_LINES_3_RGBA);
        m.drawLine(0, 0, getSize().x, getSize().y);
        m.drawLine(getSize().x, 0, 0, getSize().y);

        m.setLineWidth(2);

        // GUIDE CIRCLES
        if ((monitorState->monitor_mode >= 0 && monitorState->monitor_mode < 3) && monitorState->monitor_mode != 1)
        {
            //inside circle
            m.disableFill();
            m.drawCircle(getSize().x / 2, getSize().y / 2, getSize().y / 4);
            //outside circle
            m.drawCircle(shape.size.x / 2, shape.size.y / 2, shape.size.y / 2);
            //middle circle
            //r->drawCircle(getSize().x / 2, getSize().y / 2, sqrt(pow(getSize().y / 4, 2) + pow(getSize().y / 4, 2)));
        }

        m.enableFill();
        m.setLineWidth(1);

        // GRID FILLS
        if ((monitorState->monitor_mode >= 0 && monitorState->monitor_mode < 3) && monitorState->monitor_mode != 1)
        {
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle((getSize().x / 2) - 10, 0, 20, 10);
            m.drawRectangle((getSize().x / 2) - 10, getSize().y - 10, 20, 10);
        }

        // LARGE GRID LINES
        m.setColor(GRID_LINES_3_RGBA);
        linestep = getSize().x / 4;
        for (int i = 1; i < 4; i++)
        {
            for (int j = 1; j < 4; j++)
            {
                m.drawLine(linestep * i, 0, linestep * i, getSize().y);
                m.drawLine(0, linestep * i, getSize().x, linestep * i);
            }
        }

        // GRID LABELS
        if ((monitorState->monitor_mode >= 0 && monitorState->monitor_mode < 3) && monitorState->monitor_mode != 1)
        {
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle((getSize().x / 2) - 10, 0, 20, 12);
            m.drawRectangle((getSize().x / 2) - 12, getSize().y - 15, 25, 20);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 5);
            m.setColor(REF_LABEL_TEXT_COLOR);
            MurkaShape zeroLabelShape = { (getSize().x / 2) - 7.5, -2, 25, 15 };
            m.prepare<murka::Label>(zeroLabelShape).text({ "0" }).draw();
            MurkaShape oneEightyLabelShape = { (getSize().x / 2) - 13, getSize().y - 13, 30, 15 };
            m.prepare<murka::Label>(oneEightyLabelShape).text("180").draw();
        }

        // reset font size
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 3);

        // MIXER - MONITOR DISPLAY
        if (isConnected && (monitorState->monitor_mode >= 0 && monitorState->monitor_mode < 3) && monitorState->monitor_mode != 1)
        {
            if (monitorState->yaw >= -360. && monitorState->yaw <= 360.)
            { // block values that are outside range
                drawMonitorYaw(monitorState->yaw - 180, m);
            }
        }

        if (monitorState->monitor_mode == 1)
        {
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle((getSize().x / 2) - 110, getSize().y - 20, 220, 20);
            m.setColor(ENABLED_PARAM);
            auto& stereoSafeLabel = m.prepare<M1Label>(MurkaShape(getSize().x / 2 - 100, getSize().y - 20, 200, 20));
            stereoSafeLabel.label = "STEREO SAFE MODE";
            stereoSafeLabel.alignment = TEXT_CENTER;
            stereoSafeLabel.enabled = false;
            stereoSafeLabel.highlighted = false;
            stereoSafeLabel.draw();
        }

        MurkaPoint reticlePositionInWidgetSpace = { getSize().x / 2 + (std::get<0>(*xyrd) / 100.) * getSize().x / 2,
            getSize().y / 2 + (-std::get<1>(*xyrd) / 100.) * getSize().y / 2 };
        auto center = MurkaPoint(getSize().x / 2,
            getSize().y / 2);

        if ((draggingNow) || (shouldDrawRotateGuideLine))
        {
            m.setColor(M1_ACTION_YELLOW);
            m.disableFill();
            m.drawCircle(center.x,
                center.y,
                (std::get<3>(*xyrd) / 100.) * getSize().x / sqrt(2));
        }

        if ((draggingNow) || (shouldDrawDivergeGuideLine))
        {
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
                                  20)
                                  .inside(mousePosition())
                              + draggingNow;

        // Drawing central reticle
        if ((monitorState->monitor_mode >= 0 && monitorState->monitor_mode < 3) && monitorState->monitor_mode != 1)
        {
            m.enableFill();
            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (10 + 3 * A(reticleHovered) + (2 * (pannerState->elevation / 90))));
            m.setColor(BACKGROUND_GREY); // background color for internal circle
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, (8 + 3 * A(reticleHovered) + (2 * (pannerState->elevation / 90))));

            m.setColor(M1_ACTION_YELLOW);
            m.drawCircle(reticlePositionInWidgetSpace.x, reticlePositionInWidgetSpace.y, 6);

            // Reticles
            std::vector<Mach1Point3D> points = pannerState->m1Encode.getPoints();
            std::vector<std::string> pointsNames = pannerState->m1Encode.getPointsNames();
            if (pannerState->m1Encode.getInputChannelsCount() > 1)
            {
                for (int i = 0; i < pannerState->m1Encode.getPointsCount(); i++)
                {
                    MurkaPoint point((points[i].z + 1.0) * shape.size.x / 2, (-points[i].x + 1.0) * shape.size.y / 2);
                    clamp(point.x, 0, shape.size.x);
                    clamp(point.y, 0, shape.size.y);
                    if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputMode::Stereo)
                    {
                        drawAdditionalReticle(point.x, point.y, pointsNames[i], reticleHovered, 1, m);
                    }
                    else if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputMode::AFormat)
                    {
                        drawAdditionalReticle(point.x, point.y, pointsNames[i], reticleHovered, 2, m);
                    }
                    else
                    {
                        drawAdditionalReticle(point.x, point.y, pointsNames[i], reticleHovered, 1.5, m);
                    }
                }
            }
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
            teleportCursor(shape.position.x + reticlePositionInWidgetSpace.x,
                shape.position.x + reticlePositionInWidgetSpace.y);
        }

        if (draggingNow)
        {
            MurkaPoint normalisedDelta = { mouseDelta().x / shape.size.x,
                mouseDelta().y / shape.size.y };
            if (mouseDelta().lengthSquared() > 0.001)
            {
                std::get<0>(*xyrd) -= normalisedDelta.x * 200;
                std::get<1>(*xyrd) += normalisedDelta.y * 200;

                results = true;
            }
        }

        if ((doubleClick()) && (inside()))
        {
            MurkaPoint normalisedMouse = { mousePosition().x / shape.size.x,
                mousePosition().y / shape.size.y };
            std::get<0>(*xyrd) = normalisedMouse.x * 200 - 100;
            std::get<1>(*xyrd) = -normalisedMouse.y * 200 + 100;

            draggingNow = true;
            cursorHide();

            results = true;
        }

        clamp(std::get<0>(*xyrd), -100, 100);
        clamp(std::get<1>(*xyrd), -100, 100);
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

    void clamp(float& input, float min, float max)
    {
        if (input < min)
            input = min;
        if (input > max)
            input = max;
    }

    void drawAdditionalReticle(float x, float y, std::string label, bool reticleHovered, float reticleSizeMultiplier, Murka& m)
    {
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, (DEFAULT_FONT_SIZE + 2 * A(reticleHovered) + (2 * (pannerState->elevation / 90))));
        m.setColor(M1_ACTION_YELLOW);
        m.disableFill();

        m.drawCircle(x, y, (10 * reticleSizeMultiplier) + 3 * A(reticleHovered) + (2 * (pannerState->elevation / 90)));
        // re-adjust label x offset for size of string
        m.prepare<murka::Label>(MurkaShape(x - (4 + label.length() * 4.75) - (2 * (pannerState->elevation / 90)), y - 8 - 2 * A(reticleHovered) - (2 * (pannerState->elevation / 90)), 50, 50)).text(label.c_str()).draw();
    }

    void drawMonitorYaw(float yawAngle, Murka& m)
    {
        MurkaPoint center = { getSize().x / 2, getSize().y / 2 };
        float yawRad = juce::degreesToRadians(yawAngle + 90.0f);
        float yawX = cos(yawRad);
        float yawY = sin(yawRad);
        float diameter = getSize().y / 2;
        m.setColor(ENABLED_PARAM);
        m.drawLine(center.x, center.y, yawX * diameter + center.x, yawY * diameter + center.y);

        // reference arc
        float angleSize = juce::MathConstants<float>::pi / 2;

        int numsteps = 60;
        for (int i = 0; i < numsteps; i++)
        {
            float phase0 = (float)i / (float)numsteps;
            float phase1 = ((float)i + 1) / (float)numsteps;

            float angle0 = phase0 * angleSize + yawRad - angleSize * 0.5;
            float angle1 = phase1 * angleSize + yawRad - angleSize * 0.5;

            MurkaPoint lineStart = center + MurkaPoint(cos(angle0 - 0.01) * (center.x - 1), sin(angle0 - 0.01) * (center.y - 1));
            MurkaPoint lineEnd = center + MurkaPoint(cos(angle1 + 0.01) * (center.x - 1), sin(angle1 + 0.01) * (center.y - 1));
            m.drawLine(lineStart.x, lineStart.y, lineEnd.x, lineEnd.y);
        }
    }

    PannerReticleField& controlling(XYRD* dataToControl_)
    {
        dataToControl = dataToControl_;
        return *this;
    }

    std::function<void()> m1encodeUpdate = []() {};

    XYRD* dataToControl;

    bool draggingNow = false;
    bool reticleHoveredLastFrame = false;
    bool results = false;

    float somevalue = 0.0;
    std::function<void()> cursorHide;
    std::function<void()> cursorShow;
    std::function<void(int, int)> teleportCursor;
    bool shouldDrawDivergeGuideLine = false;
    bool shouldDrawRotateGuideLine = false;
    PannerSettings* pannerState = nullptr;
    MixerSettings* monitorState = nullptr;
    bool isConnected = false;

    // The results type, you also need to define it even if it's nothing.
    typedef bool Results;
};

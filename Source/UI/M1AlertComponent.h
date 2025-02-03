#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../AlertData.h"

// TODO: Resize button and message components based on size of the string
// TODO: Add buttons for response actions to specific errors or alerts
// TODO: Escape key to quickly dismiss alert

using namespace murka;

class M1AlertComponent : public murka::View<M1AlertComponent>
{
public:

    void internalDraw(Murka& m)
    {
        if (!alertActive) return;

        // Dark overlay
        m.setColor(40, 40, 40, 200); // BACKGROUND_GREY
        m.drawRectangle(0, 0, shape.size.x, shape.size.y);

        // Alert background
        const auto centerX = shape.size.x * 0.5f;
        const auto centerY = shape.size.y * 0.5f;
        // outline
        m.setColor(ENABLED_PARAM);
        m.drawRectangle(centerX - alertWidth * 0.5f,
                        centerY - alertHeight * 0.5f,
                        alertWidth,
                        alertHeight);
        // background box
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(centerX - alertWidth * 0.5f + 1,
                        centerY - alertHeight * 0.5f + 1,
                        alertWidth - 2,
                        alertHeight - 2);

        // Title
        m.setColor(LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 22);
        juceFontStash::Rectangle title_label_box = m.getCurrentFont()->getStringBoundingBox(alert.title, 0, 0); // used to find size of text
        m.prepare<murka::Label>({
            centerX - alertWidth * 0.5f + 20,
            centerY - alertHeight * 0.5f + 20,
            alertWidth - 40,
            30
        }).withAlignment(TEXT_LEFT).text(alert.title).draw();

        // Message
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 18);
        juceFontStash::Rectangle message_label_box = m.getCurrentFont()->getStringBoundingBox(alert.message, 0, 0); // used to find size of text
        m.prepare<murka::Label>({
            centerX - alertWidth * 0.5f + 20,
            centerY - alertHeight * 0.5f + 60,
            alertWidth - 40,
            alertHeight - 100
        }).withAlignment(TEXT_LEFT).text(alert.message).draw();

        // OK Button
        const float buttonY = centerY + alertHeight * 0.5f - 50;
        juceFontStash::Rectangle button_label_box = m.getCurrentFont()->getStringBoundingBox(alert.buttonText, 0, 0); // used to find size of text
        m.setColor(DISABLED_PARAM);
        if (isHovered())
        {
            m.setColor(ENABLED_PARAM);
        }
        m.drawRectangle(centerX - 40, buttonY, 80, 30);

        m.setColor(LABEL_TEXT_COLOR);
        if (isHovered())
        {
            m.setColor(BACKGROUND_GREY);
        }
        m.prepare<murka::Label>({
            centerX - 40, buttonY + button_label_box.height / 3, 80, 30
        }).withAlignment(TEXT_CENTER).text(alert.buttonText).draw();

        // Dismiss if user clicks
        if (mouseDownPressed(0) && isHovered()) {
            alertActive = false;
            if (onDismiss) onDismiss();
        }
    }

    float alertWidth = 400;
    float alertHeight = 200;
    AlertData alert;
    bool alertActive = false;
    std::function<void()> onDismiss;
};

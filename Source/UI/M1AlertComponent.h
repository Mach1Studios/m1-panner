#pragma once

#include "MurkaBasicWidgets.h"
#include "MurkaView.h"
#include "../Config.h"
#include "../AlertData.h"

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

        // Calculate text bounds first to determine alert size
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 18);
        juceFontStash::Rectangle message_label_box = m.getCurrentFont()->getStringBoundingBox(alert.message, 0, 0);

        // Adjust alert height based on message length
        const float minAlertHeight = 200;  // minimum height
        const float messageMargin = 80;     // space for title + button
        const float messageWidth = alertWidth - 40;  // width minus margins

        // Calculate how many lines the text will take
        float numLines = std::ceil(message_label_box.width / messageWidth);
        float messageHeight = message_label_box.height * numLines;

        // Set alert height based on content (with minimum)
        alertHeight = std::max(minAlertHeight, messageHeight + messageMargin);

        // Alert background
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
        m.prepare<murka::Label>({
            centerX - alertWidth * 0.5f + 20,
            centerY - alertHeight * 0.5f + 20,
            alertWidth - 40,
            30
        }).withAlignment(TEXT_LEFT).text(alert.title).draw();

        // Message - with word wrap
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 18);
        const float textX = centerX - alertWidth * 0.5f + 20;
        const float textY = centerY - alertHeight * 0.5f + 60;
        const float maxWidth = alertWidth - 40;
        float currentY = textY;

        // Split text into words
        juce::String message(alert.message);
        juce::StringArray words;
        words.addTokens(message, " \t\r\n", "\"'");

        juce::String currentLine;
        float lineHeight = m.getCurrentFont()->getLineHeight();

        for (auto& word : words) {
            juce::String testLine = currentLine.isEmpty() ? word : currentLine + " " + word;
            float testWidth = m.getCurrentFont()->stringWidth(testLine.toStdString());

            if (testWidth > maxWidth && !currentLine.isEmpty()) {
                // Draw current line
                m.prepare<murka::Label>({textX, currentY, maxWidth, lineHeight})
                   .withAlignment(TEXT_LEFT)
                   .text(currentLine.toStdString())
                   .draw();
                currentY += lineHeight;
                currentLine = word;
            } else {
                currentLine = testLine;
            }
        }

        // Draw remaining text
        if (!currentLine.isEmpty()) {
            m.prepare<murka::Label>({textX, currentY, maxWidth, lineHeight})
               .withAlignment(TEXT_LEFT)
               .text(currentLine.toStdString())
               .draw();
        }

        // OK Button
        const float buttonY = centerY + alertHeight * 0.5f - 50;
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
            centerX - 40,
            buttonY + 5,  // Slight vertical adjustment for centering
            80,
            20
        }).withAlignment(TEXT_CENTER).text(alert.buttonText).draw();

        // Dismiss if user clicks
        if (mouseDownPressed(0) && isHovered()) {
            alertActive = false;
            if (onDismiss) onDismiss();
        }
    }

    float alertWidth = 400;
    float alertHeight = 200;  // This will be adjusted based on content
    Mach1::AlertData alert;
    bool alertActive = false;
    std::function<void()> onDismiss;
};

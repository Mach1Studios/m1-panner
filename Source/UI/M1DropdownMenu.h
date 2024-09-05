#pragma once

#include "MurkaAssets.h"
#include "MurkaBasicWidgets.h"
#include "MurkaContext.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaTypes.h"
#include "MurkaView.h"

using namespace murka;

class M1DropdownMenu : public murka::View<M1DropdownMenu>
{
    enum closingModeTypes { modeUnknown,
        modeMouseUp,
        modeMouseDown };
    closingModeTypes closingMode = closingModeTypes::modeUnknown;

    int mouseKeptDownFrames = 0;

public:
    void internalDraw(Murka& m)
    {
        if (closingMode == modeUnknown)
        {
            if (mouseDown(0))
                mouseKeptDownFrames++;
            if (mouseKeptDownFrames > 20)
                closingMode = modeMouseUp;
            if (!mouseDown(0))
                closingMode = modeMouseDown;
        }

        if (opened)
        {
            // outline border
            m.setColor(ENABLED_PARAM);
            m.drawRectangle(0, 0, shape.size.x, shape.size.y);
            m.setColor(BACKGROUND_GREY);
            m.drawRectangle(1, 1, shape.size.x - 2, shape.size.y - 2);

            bool hoveredAnything = false;

            for (int i = 0; i < options.size(); i++)
            {
                bool hoveredAnOption = (mousePosition().y > i * optionHeight) && (mousePosition().y < (i + 1) * optionHeight) && inside();

                hoveredAnything += hoveredAnOption; // Setting to true if any of those are true

                if (hoveredAnOption)
                {
                    m.setColor(ENABLED_PARAM);
                    m.drawRectangle(1, i * optionHeight, shape.size.x - 2, optionHeight);
                    m.setColor(BACKGROUND_GREY);
                    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
                    juceFontStash::Rectangle label_box = m.getCurrentFont()->getStringBoundingBox(options[i], 0, 0); // used to find size of text
                    m.prepare<murka::Label>({ labelPadding_x, (optionHeight * i) + optionHeight / 2 - label_box.height / 2, shape.size.x - labelPadding_x, optionHeight }).text(options[i]).withAlignment(textAlignment).draw();

                    if (closingMode == modeMouseDown)
                    {
                        if (mouseDownPressed(0))
                        {
                            opened = false; // Closing the menu
                            if (selectedOption != i)
                            {
                                changed = true;
                            }
                            selectedOption = i;
                        }
                    }
                    if (closingMode == modeMouseUp)
                    {
                        if (mouseReleased(0))
                        {
                            opened = false; // Closing the menu
                            if (selectedOption != i)
                            {
                                changed = true;
                            }
                            selectedOption = i;
                        }
                    }
                }
                else
                {
                    m.setColor(LABEL_TEXT_COLOR);
                    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
                    juceFontStash::Rectangle label_box = m.getCurrentFont()->getStringBoundingBox(options[i], 0, 0); // used to find size of text
                    m.prepare<murka::Label>({ labelPadding_x, (optionHeight * i) + optionHeight / 2 - label_box.height / 2, shape.size.x - labelPadding_x, optionHeight }).text(options[i]).withAlignment(textAlignment).draw();
                }

                // Closing if pressed/released outside of the menu
                if (!inside() && !hoveredAnything)
                {
                    if ((closingMode == modeMouseDown) && (mouseDownPressed(0)))
                    {
                        opened = false;
                        changed = false;
                    }
                    if ((closingMode == modeMouseUp) && (mouseReleased(0)))
                    {
                        opened = false;
                        changed = false;
                    }
                }
            }
        }
        else
        {
            changed = false;
        }
    }

    void close()
    {
        opened = false;
        mouseKeptDownFrames = 0;
        closingMode = closingModeTypes::modeUnknown;
        changed = false;
    }

    void open()
    {
        opened = true;
        mouseKeptDownFrames = 0;
        closingMode = closingModeTypes::modeUnknown;
    }

    bool changed = false;
    bool opened = false;
    int selectedOption = 0;
    std::vector<std::string> options;
    Mach1EncodeInputModeType* dataToControl = nullptr;

    int optionHeight = 30;
    int fontSize = 10;
    bool enabled = true;
    std::string label;
    MurkaShape triggerButtonShape;
    TextAlignment textAlignment = TEXT_CENTER;
    int labelPadding_x = 0;
    int labelPadding_y = 0;

    M1DropdownMenu& controlling(Mach1EncodeInputModeType* dataPointer)
    {
        dataToControl = dataPointer;
        return *this;
    }

    M1DropdownMenu& withLabel(std::string label_)
    {
        label = label_;
        return *this;
    }

    M1DropdownMenu& withOptions(std::vector<std::string> options_)
    {
        options = options_;
        return *this;
    }

    M1DropdownMenu& withTriggerButtonPlacedAt(MurkaShape shape)
    {
        triggerButtonShape = shape;
        return *this;
    }
};

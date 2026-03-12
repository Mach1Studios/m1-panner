#pragma once

#include <JuceHeader.h>
#include "../Config.h"

namespace NativePannerColours {
inline juce::Colour rgb(unsigned char r, unsigned char g, unsigned char b) { return juce::Colour::fromRGB(r, g, b); }
inline juce::Colour rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { return juce::Colour::fromRGBA(r, g, b, a); }
}

class NativePannerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NativePannerLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, NativePannerColours::rgb(BACKGROUND_GREY));
        setColour(juce::ComboBox::outlineColourId, NativePannerColours::rgb(ENABLED_PARAM));
        setColour(juce::ComboBox::textColourId, NativePannerColours::rgb(LABEL_TEXT_COLOR));
        setColour(juce::PopupMenu::backgroundColourId, NativePannerColours::rgb(BACKGROUND_GREY));
        setColour(juce::PopupMenu::textColourId, NativePannerColours::rgb(LABEL_TEXT_COLOR));
        setColour(juce::Label::textColourId, NativePannerColours::rgb(LABEL_TEXT_COLOR));
        setColour(juce::Slider::textBoxTextColourId, NativePannerColours::rgb(LABEL_TEXT_COLOR));
        setColour(juce::Slider::textBoxBackgroundColourId, NativePannerColours::rgb(BACKGROUND_GREY));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, NativePannerColours::rgb(ENABLED_PARAM));
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override
    {
        static auto typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf,
                                                                       BinaryData::InterRegular_ttfSize);
        return typeface;
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          const float rotaryStartAngle,
                          const float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>(x, y, width, height);
        const auto enabled = slider.isEnabled();
        const auto outerColour = enabled ? NativePannerColours::rgb(ENABLED_PARAM) : NativePannerColours::rgb(DISABLED_PARAM);
        const auto innerColour = NativePannerColours::rgb(BACKGROUND_GREY);
        const auto accent = enabled ? juce::Colour(210, 210, 210) : juce::Colour(120, 120, 120);

        const auto dialCenter = juce::Point<float>(bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.32f);
        const float radius = bounds.getWidth() * 0.22f;

        g.setColour(outerColour);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.2f, radius * 2.2f).withCentre(dialCenter));

        g.setColour(innerColour);
        g.fillEllipse(juce::Rectangle<float>(radius * 1.8f, radius * 1.8f).withCentre(dialCenter));

        const auto angle = juce::jmap(sliderPos, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        juce::Path pointer;
        pointer.addRoundedRectangle(-2.0f, -radius, 4.0f, radius * 0.95f, 1.5f);

        g.setColour(accent);
        g.fillPath(pointer,
                   juce::AffineTransform::rotation(angle).translated(dialCenter.x, dialCenter.y));
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float,
                          float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical) {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<float>(x, y, width, height);
        const auto enabled = slider.isEnabled();
        const auto trackColour = enabled ? NativePannerColours::rgb(ENABLED_PARAM) : NativePannerColours::rgb(DISABLED_PARAM);

        g.setColour(juce::Colour(57, 57, 57));
        g.drawLine(bounds.getCentreX(), bounds.getY() + 12.0f, bounds.getCentreX(), bounds.getBottom() - 12.0f, 1.0f);

        for (int i = 0; i <= 4; ++i) {
            const auto yPos = juce::jmap(static_cast<float>(i), 0.0f, 4.0f, bounds.getY() + 12.0f, bounds.getBottom() - 12.0f);
            const auto halfWidth = i == 2 ? bounds.getWidth() * 0.22f : bounds.getWidth() * 0.12f;
            g.setColour(juce::Colour(77, 77, 77));
            g.drawLine(bounds.getCentreX() - halfWidth, yPos, bounds.getCentreX() + halfWidth, yPos, 1.0f);
        }

        g.setColour(trackColour);
        const auto handleBounds = juce::Rectangle<float>(12.0f, 12.0f).withCentre({ bounds.getCentreX(), sliderPos });
        g.fillEllipse(handleBounds);
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const auto enabled = button.isEnabled();
        const auto accent = enabled ? NativePannerColours::rgb(ENABLED_PARAM) : NativePannerColours::rgb(DISABLED_PARAM);
        const float radius = 8.0f;
        const auto circleBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f)
                                      .withCentre({ bounds.getX() + radius + 2.0f, bounds.getCentreY() });

        g.setColour(accent.withAlpha(shouldDrawButtonAsHighlighted ? 0.95f : 0.8f));
        g.fillEllipse(circleBounds);
        g.setColour(NativePannerColours::rgb(BACKGROUND_GREY));
        g.fillEllipse(circleBounds.reduced(2.0f));

        if (button.getToggleState()) {
            g.setColour(accent);
            g.fillEllipse(circleBounds.reduced(5.0f));
        }

        g.setColour(NativePannerColours::rgb(LABEL_TEXT_COLOR));
        g.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 4)));
        g.drawText(button.getButtonText(),
                   bounds.withTrimmedLeft(circleBounds.getRight() + 6.0f).toNearestInt(),
                   juce::Justification::centredLeft,
                   false);
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        g.setColour(NativePannerColours::rgb(BACKGROUND_GREY));
        g.fillRect(bounds);

        g.setColour(NativePannerColours::rgb(ENABLED_PARAM));
        g.drawRect(bounds, 1.0f);

        juce::Path triangle;
        triangle.addTriangle(width - 16.0f, 9.0f, width - 6.0f, 9.0f, width - 11.0f, 15.0f);
        g.fillPath(triangle);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 5));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(box.getLocalBounds().reduced(4, 1));
        label.setFont(getComboBoxFont(box));
        label.setJustificationType(juce::Justification::centred);
    }
};

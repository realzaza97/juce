#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
const juce::Colour bg        { 0xff0d0e13 };
const juce::Colour panel     { 0xff161824 };
const juce::Colour panelHi   { 0xff1f2233 };
const juce::Colour line      { 0xff2a2e45 };
const juce::Colour text      { 0xffe6e8f2 };
const juce::Colour textDim   { 0xff8b90a8 };
const juce::Colour purple    { 0xff8b5cf6 };
const juce::Colour blue      { 0xff38bdf8 };
const juce::Colour green     { 0xff34d399 };
const juce::Colour warn      { 0xfffbbf24 };

inline juce::String noteName (int midi)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}
inline bool isBlackKey (int midi) { const int k = ((midi % 12) + 12) % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

struct LookAndFeel : juce::LookAndFeel_V4
{
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::TextButton::buttonColourId, panelHi);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::Slider::thumbColourId, purple);
        setColour (juce::Slider::trackColourId, purple);
        setColour (juce::Slider::backgroundColourId, line);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, textDim);
        setColour (juce::ComboBox::backgroundColourId, panelHi);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, purple);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = base;
        if (! b.isEnabled()) c = c.withAlpha (0.35f);
        else if (down) c = c.brighter (0.2f);
        else if (over) c = c.brighter (0.1f);
        g.setColour (c); g.fillRoundedRectangle (r, 6.f);
        g.setColour (line); g.drawRoundedRectangle (r, 6.f, 1.f);
    }
};
} // namespace theme

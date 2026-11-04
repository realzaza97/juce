#include "PianoRollView.h"
#include "../PluginProcessor.h"
#include "../Theme.h"

double PianoRollView::gridStep() const { return v2i::gridBeats (proc.currentConvertParams().gridDiv); }
double PianoRollView::snap (double b) const { const double g = gridStep(); return g > 0 ? std::round (b / g) * g : b; }

void PianoRollView::fitView()
{
    auto& notes = proc.editableNotes();
    if (notes.empty()) { scrollBeat = 0; return; }
    int lo = 127, hi = 0; double end = 0;
    for (auto& n : notes) { lo = juce::jmin (lo, n.note); hi = juce::jmax (hi, n.note); end = juce::jmax (end, n.startBeat + n.lengthBeat); }
    const float h = (float) getHeight() - rulerH;
    const int rows = juce::jmax (12, (int) (h / rowH));
    topNote = juce::jlimit (rows - 1, 127, hi + (rows - (hi - lo + 1)) / 2 + 1);
    pxPerBeat = juce::jlimit (12.0, 600.0, (getWidth() - keyW - 24.0) / juce::jmax (1.0, end));
    scrollBeat = 0.0;
    repaint();
}

int PianoRollView::hitTest (juce::Point<float> p) const
{
    auto& notes = proc.editableNotes();
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        auto& n = notes[(size_t) i];
        juce::Rectangle<float> r (beatToX (n.startBeat), noteToY (n.note), (float) (n.lengthBeat * pxPerBeat), rowH);
        if (r.expanded (0.f, 0.f).contains (p)) return i;
    }
    return -1;
}

void PianoRollView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (theme::panel); g.fillRoundedRectangle (b, 8.f);
    g.saveState();
    g.reduceClipRegion (getLocalBounds());

    const int rows = (int) std::ceil ((b.getHeight() - rulerH) / rowH);
    const float gx0 = keyW;

    // rows
    for (int r = 0; r < rows; ++r)
    {
        const int n = topNote - r;
        if (n < 0 || n > 127) continue;
        const float y = noteToY (n);
        g.setColour (theme::isBlackKey (n) ? theme::bg.withAlpha (0.55f) : theme::panel.brighter (0.03f));
        g.fillRect (gx0, y, b.getWidth() - gx0, rowH);
        g.setColour (theme::line.withAlpha (n % 12 == 0 ? 0.9f : 0.35f));
        g.drawHorizontalLine ((int) (y + rowH), gx0, b.getRight());
    }

    // grid (subdivision, beat, bar)
    const double step = gridStep();
    const double firstBeat = std::floor (scrollBeat), lastBeat = xToBeat (b.getRight());
    if (step > 0 && step * pxPerBeat >= 7.0)
        for (double t = std::floor (scrollBeat / step) * step; t <= lastBeat; t += step)
        { g.setColour (theme::line.withAlpha (0.35f)); g.drawVerticalLine ((int) beatToX (t), rulerH, b.getBottom()); }
    for (double t = firstBeat; t <= lastBeat; t += 1.0)
    {
        const bool bar = ((long) std::llround (t)) % 4 == 0;
        const float x = beatToX (t);
        if (x < gx0) continue;
        g.setColour (bar ? theme::textDim.withAlpha (0.55f) : theme::line);
        g.drawVerticalLine ((int) x, rulerH, b.getBottom());
        if (bar) { g.setColour (theme::textDim); g.setFont (11.f); g.drawText (juce::String ((long) std::llround (t) / 4 + 1), (int) x + 3, 3, 40, 16, juce::Justification::left); }
    }

    // raw pitch curve
    {
        const auto cp = proc.currentConvertParams();
        const double toBeat = cp.bpm / 60.0 / juce::jmax (0.05, cp.speed);
        juce::Path path; bool pen = false;
        for (auto& f : proc.getFrames())
        {
            if (! f.voiced) { pen = false; continue; }
            const float x = beatToX (f.timeSec * toBeat), y = noteToY (0) - (f.midi + 0.5f) * rowH;
            if (x < gx0 - 10 || x > b.getRight() + 10) { pen = false; continue; }
            if (! pen) { path.startNewSubPath (x, y); pen = true; } else path.lineTo (x, y);
        }
        g.setColour (theme::blue.withAlpha (0.45f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    // notes
    auto& notes = proc.editableNotes();
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        auto& n = notes[(size_t) i];
        juce::Rectangle<float> r (beatToX (n.startBeat), noteToY (n.note), juce::jmax (3.f, (float) (n.lengthBeat * pxPerBeat)), rowH);
        if (r.getRight() < gx0 || r.getX() > b.getRight()) continue;
        const float v = (float) n.velocity / 127.f;
        g.setColour (theme::purple.interpolatedWith (theme::blue, v * 0.6f).withAlpha (0.55f + 0.45f * v));
        g.fillRoundedRectangle (r.reduced (0.f, 1.f), 3.f);
        g.setColour (i == selected ? theme::green : theme::purple.brighter (0.4f));
        g.drawRoundedRectangle (r.reduced (0.f, 1.f), 3.f, i == selected ? 2.f : 1.f);
        if (r.getWidth() > 26.f) { g.setColour (theme::text); g.setFont (10.f); g.drawText (theme::noteName (n.note), r.reduced (4.f, 0.f), juce::Justification::centredLeft); }
    }

    // keyboard strip + ruler (drawn last so notes never cover them)
    g.setColour (theme::panelHi); g.fillRect (0.f, rulerH, keyW, b.getHeight());
    for (int r = 0; r < rows; ++r)
    {
        const int n = topNote - r; if (n < 0 || n > 127) continue;
        const float y = noteToY (n);
        g.setColour (theme::isBlackKey (n) ? juce::Colour (0xff0b0c10) : juce::Colour (0xffd9dbe6));
        g.fillRect (0.f, y + 0.5f, theme::isBlackKey (n) ? keyW * 0.62f : keyW - 1.f, rowH - 1.f);
        if (n % 12 == 0) { g.setColour (juce::Colour (0xff20222e)); g.setFont (10.f); g.drawText (theme::noteName (n), 0, (int) y, (int) keyW - 4, (int) rowH, juce::Justification::centredRight); }
    }
    g.setColour (theme::panelHi); g.fillRect (0.f, 0.f, b.getWidth(), rulerH);
    g.setColour (theme::line); g.drawHorizontalLine ((int) rulerH, 0.f, b.getRight());
    for (double t = firstBeat; t <= lastBeat; t += 1.0)
    {
        const float x = beatToX (t); if (x < gx0) continue;
        if (((long) std::llround (t)) % 4 == 0) { g.setColour (theme::textDim); g.setFont (11.f); g.drawText (juce::String ((long) std::llround (t) / 4 + 1), (int) x + 3, 3, 40, 16, juce::Justification::left); }
        g.setColour (theme::line); g.drawVerticalLine ((int) x, rulerH - 6.f, rulerH);
    }
    if (notes.empty())
    {
        g.setColour (theme::textDim); g.setFont (14.f);
        g.drawText ("Load audio and press ANALYZE. Double-click here to add notes by hand.", getLocalBounds().withTrimmedLeft ((int) keyW), juce::Justification::centred);
    }
    g.restoreState();
    g.setColour (theme::line); g.drawRoundedRectangle (b.reduced (0.5f), 8.f, 1.f);
}

void PianoRollView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    drag = Drag::none;
    if (e.x < keyW || e.y < rulerH) return;
    const int hit = hitTest (e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit >= 0) { proc.editableNotes().erase (proc.editableNotes().begin() + hit); selected = -1; proc.notesEdited(); repaint(); }
        return;
    }
    selected = hit;
    if (hit >= 0)
    {
        auto& n = proc.editableNotes()[(size_t) hit];
        origStart = n.startBeat; origLen = n.lengthBeat; origNote = n.note;
        const float endX = beatToX (n.startBeat + n.lengthBeat);
        drag = (endX - e.position.x <= 7.f && n.lengthBeat * pxPerBeat > 14.0) ? Drag::resize : Drag::move;
    }
    repaint();
}

void PianoRollView::mouseDrag (const juce::MouseEvent& e)
{
    auto& notes = proc.editableNotes();
    if (selected < 0 || selected >= (int) notes.size() || drag == Drag::none) return;
    auto& n = notes[(size_t) selected];
    const double dBeat = e.getDistanceFromDragStartX() / pxPerBeat;
    if (drag == Drag::move)
    {
        n.startBeat = juce::jmax (0.0, snap (origStart + dBeat));
        n.note = juce::jlimit (0, 127, origNote - (int) std::lround (e.getDistanceFromDragStartY() / (double) rowH));
    }
    else
    {
        const double minLen = gridStep() > 0 ? gridStep() : 0.05;
        n.lengthBeat = juce::jmax (minLen, snap (origStart + origLen + dBeat) - origStart);
    }
    proc.notesEdited();
    repaint();
}

void PianoRollView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.x < keyW || e.y < rulerH || hitTest (e.position) >= 0) return;
    const double len = gridStep() > 0 ? gridStep() : 0.25;
    const int note = juce::jlimit (0, 127, yToNote (e.position.y));
    auto& notes = proc.editableNotes();
    notes.push_back ({ juce::jmax (0.0, snap (xToBeat (e.position.x))), len, note, 100 });
    selected = (int) notes.size() - 1;
    proc.notesEdited();
    repaint();
}

void PianoRollView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = xToBeat (e.position.x);
        pxPerBeat = juce::jlimit (8.0, 800.0, pxPerBeat * std::exp ((double) w.deltaY * 1.5));
        scrollBeat = juce::jmax (0.0, anchor - (e.position.x - keyW) / pxPerBeat);
    }
    else if (e.mods.isShiftDown())
        topNote = juce::jlimit (12, 127, topNote + (w.deltaY > 0 ? 3 : -3));
    else
    {
        const double visible = (getWidth() - keyW) / pxPerBeat;
        scrollBeat = juce::jmax (0.0, scrollBeat - (double) (w.deltaY != 0.f ? w.deltaY : w.deltaX) * visible * 0.5);
    }
    repaint();
}

bool PianoRollView::keyPressed (const juce::KeyPress& k)
{
    auto& notes = proc.editableNotes();
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && selected >= 0 && selected < (int) notes.size())
    {
        notes.erase (notes.begin() + selected); selected = -1; proc.notesEdited(); repaint(); return true;
    }
    return false;
}

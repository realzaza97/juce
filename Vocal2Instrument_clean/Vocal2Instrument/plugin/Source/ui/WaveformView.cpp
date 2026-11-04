#include "WaveformView.h"
#include "../Theme.h"

void WaveformView::rebuildPeaks()
{
    peaks.clear();
    if (audio == nullptr || audio->samples.empty() || getWidth() <= 0) return;
    const int w = getWidth();
    const size_t n = audio->samples.size();
    peaks.resize ((size_t) w);
    for (int x = 0; x < w; ++x)
    {
        const size_t a = n * (size_t) x / (size_t) w, b = juce::jmax (a + 1, n * (size_t) (x + 1) / (size_t) w);
        float lo = 0.f, hi = 0.f;
        for (size_t i = a; i < b && i < n; ++i) { lo = juce::jmin (lo, audio->samples[i]); hi = juce::jmax (hi, audio->samples[i]); }
        peaks[(size_t) x] = { lo, hi };
    }
}

double WaveformView::xToTime (float x) const
{
    if (audio == nullptr || getWidth() <= 0) return 0.0;
    return juce::jlimit (0.0, audio->durationSec(), (double) x / getWidth() * audio->durationSec());
}

bool WaveformView::getSelection (double& t0, double& t1) const
{
    if (std::abs (selEnd - selStart) < 0.05) return false;
    t0 = juce::jmin (selStart, selEnd); t1 = juce::jmax (selStart, selEnd);
    return true;
}

void WaveformView::mouseDown (const juce::MouseEvent& e) { selStart = selEnd = xToTime ((float) e.x); repaint(); }
void WaveformView::mouseDrag (const juce::MouseEvent& e) { selEnd = xToTime ((float) e.x); repaint(); }

void WaveformView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel); g.fillRoundedRectangle (r, 8.f);
    g.setColour (theme::line);  g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);

    if (audio == nullptr || audio->samples.empty())
    {
        g.setColour (theme::textDim); g.setFont (14.f);
        g.drawText (placeholder, getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float mid = r.getCentreY(), half = r.getHeight() * 0.45f;
    g.setColour (theme::line); g.drawHorizontalLine ((int) mid, 0.f, r.getRight());

    juce::ColourGradient grad (theme::blue, 0, r.getY(), theme::purple, 0, r.getBottom(), false);
    g.setGradientFill (grad);
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const float y0 = mid - peaks[x].second * half, y1 = mid - peaks[x].first * half;
        g.drawVerticalLine ((int) x, y0, juce::jmax (y1, y0 + 1.f));
    }
    double a, b;
    if (getSelection (a, b))
    {
        const float xa = (float) (a / audio->durationSec() * getWidth()), xb = (float) (b / audio->durationSec() * getWidth());
        g.setColour (theme::green.withAlpha (0.18f)); g.fillRect (xa, r.getY(), xb - xa, r.getHeight());
        g.setColour (theme::green); g.drawVerticalLine ((int) xa, r.getY(), r.getBottom()); g.drawVerticalLine ((int) xb, r.getY(), r.getBottom());
    }
    g.setColour (theme::textDim); g.setFont (11.f);
    g.drawText (juce::String (audio->durationSec(), 1) + " s", getLocalBounds().reduced (8, 4), juce::Justification::topRight);
}

#include "ScanProgressPanel.h"

ScanProgressPanel::ScanProgressPanel()
{
    setOpaque (true);
}

void ScanProgressPanel::startScan()
{
    state = State::Scanning;
    progress = 0.0f;
    currentPluginName = {};
    failures.clear();
    stopTimer();
    repaint();
}

void ScanProgressPanel::updateProgress (float newProgress, const juce::String& pluginName)
{
    bool needsRepaint = (pluginName != currentPluginName)
                     || (std::abs (newProgress - progress) > 0.005f);

    progress = newProgress;
    currentPluginName = pluginName;

    if (needsRepaint)
        repaint();
}

void ScanProgressPanel::showSummary (int numFound, const juce::StringArray& failedFiles)
{
    state = State::Summary;
    pluginsFound = numFound;
    failures = failedFiles;
    repaint();
    startTimer (5000); // auto-dismiss after 5 seconds
}

void ScanProgressPanel::timerCallback()
{
    stopTimer();
    if (onDismissed)
        onDismissed();
}

void ScanProgressPanel::mouseDown (const juce::MouseEvent& e)
{
    if (state == State::Summary && failures.size() > 0)
    {
        // Check if click is in the "failed" text region (right half)
        if (e.position.x > getWidth() * 0.5f)
        {
            juce::PopupMenu menu;
            menu.addSectionHeader ("Failed plugins");
            for (auto& f : failures)
                menu.addItem (juce::PopupMenu::Item (f).setEnabled (false));
            menu.showMenuAsync ({});
            return;
        }
    }

    // Clicking anywhere else dismisses the summary
    if (state == State::Summary)
    {
        stopTimer();
        if (onDismissed)
            onDismissed();
    }
}

void ScanProgressPanel::paint (juce::Graphics& g)
{
    g.fillAll (Colors::surface);

    // Bottom border
    g.setColour (Colors::border);
    g.drawLine (0.0f, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f, 1.0f);

    auto bounds = getLocalBounds().reduced (14, 0);

    if (state == State::Scanning)
    {
        // -- Top row: label + plugin name --
        auto textArea = bounds.removeFromTop (22).withTrimmedTop (4);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (Colors::textMid);
        g.drawText ("Scanning...", textArea.removeFromLeft (80), juce::Justification::centredLeft);

        g.setColour (Colors::textDim);
        auto nameStr = currentPluginName.isEmpty() ? juce::String ("Preparing...") : currentPluginName;
        g.drawText (nameStr, textArea, juce::Justification::centredLeft, true);

        // -- Percentage on right --
        auto pctStr = juce::String (juce::roundToInt (progress * 100.0f)) + "%";
        g.setColour (Colors::textMid);
        g.drawText (pctStr, bounds.removeFromRight (40).withY (textArea.getY()).withHeight (textArea.getHeight()),
                    juce::Justification::centredRight);

        // -- Bottom row: progress bar --
        auto barArea = bounds.withHeight (4).withY (getHeight() - 10);
        g.setColour (Colors::bgDeep);
        g.fillRoundedRectangle (barArea.toFloat(), 2.0f);

        auto filledWidth = barArea.toFloat().withWidth (barArea.getWidth() * progress);
        g.setColour (Colors::accent);
        g.fillRoundedRectangle (filledWidth, 2.0f);
    }
    else // Summary
    {
        auto textArea = bounds.withTrimmedTop (10);
        g.setFont (juce::FontOptions (12.0f));
        g.setColour (Colors::text);

        juce::String summary = "Scan complete: " + juce::String (pluginsFound) + " plugins found";
        auto summaryWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), summary);
        g.drawText (summary, textArea, juce::Justification::centredLeft);

        if (failures.size() > 0)
        {
            auto failArea = textArea.withTrimmedLeft (juce::roundToInt (summaryWidth) + 6);
            auto failStr = juce::String (failures.size()) + " failed (click to view)";
            g.setColour (Colors::remove);
            g.drawText (failStr, failArea, juce::Justification::centredLeft);
        }

        // Dismiss hint on right
        g.setColour (Colors::textDim);
        g.drawText ("click to dismiss", textArea, juce::Justification::centredRight);
    }
}

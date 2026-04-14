#pragma once
// ScanProgressPanel — inline bar showing scan progress, current plugin name, and post-scan summary
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"

class ScanProgressPanel : public juce::Component, private juce::Timer
{
public:
    ScanProgressPanel();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void startScan();
    void updateProgress (float progress, const juce::String& pluginName);
    void showSummary (int numFound, const juce::StringArray& failedFiles);

    std::function<void()> onDismissed;

    static constexpr int kPanelHeight = 40;

private:
    void timerCallback() override;

    enum class State { Scanning, Summary };
    State state = State::Scanning;

    float progress = 0.0f;
    juce::String currentPluginName;
    int pluginsFound = 0;
    juce::StringArray failures;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScanProgressPanel)
};

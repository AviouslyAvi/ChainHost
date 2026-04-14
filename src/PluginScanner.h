#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PluginScanner : private juce::Timer
{
public:
    PluginScanner();
    ~PluginScanner() override;

    void scan();
    bool isScanning() const { return scanner != nullptr; }

    juce::KnownPluginList& getPluginList() { return knownPlugins; }
    const juce::KnownPluginList& getPluginList() const { return knownPlugins; }

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }

    void saveToCache();
    void loadFromCache();

    std::function<void()> onScanComplete;
    std::function<void (float progress, const juce::String& pluginName)> onScanProgress;
    std::function<void (int numFound, const juce::StringArray& failedFiles)> onScanFinished;

private:
    void timerCallback() override;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    int currentFormatIndex = 0;
    juce::StringArray failedFiles;

    juce::File getCacheFile() const;
    void startNextFormat();
};

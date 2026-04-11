#pragma once
// PresetBrowser — list/save/load/delete presets for ChainHost
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colors.h"

class ChainHostProcessor;

class PresetBrowser : public juce::Component, public juce::ListBoxModel
{
public:
    PresetBrowser (ChainHostProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    std::function<void()> onPresetLoaded;
private:
    ChainHostProcessor& proc;
    juce::ListBox listBox { "Presets", this };
    juce::TextEditor nameEditor;
    juce::TextButton saveButton { "SAVE" }, deleteButton { "DEL" };
    juce::StringArray presetNames;
    juce::Array<juce::File> presetFiles;
    int selectedRow = -1;
};

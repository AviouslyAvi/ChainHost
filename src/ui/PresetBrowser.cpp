#include "PresetBrowser.h"
#include "../PluginProcessor.h"

PresetBrowser::PresetBrowser (ChainHostProcessor& p) : proc (p)
{
    listBox.setColour (juce::ListBox::backgroundColourId, Colors::bgDeep);
    listBox.setColour (juce::ListBox::outlineColourId, Colors::border);
    listBox.setRowHeight (22);
    addAndMakeVisible (listBox);

    nameEditor.setColour (juce::TextEditor::backgroundColourId, Colors::surfaceRaised);
    nameEditor.setColour (juce::TextEditor::textColourId, Colors::text);
    nameEditor.setColour (juce::TextEditor::outlineColourId, Colors::border);
    nameEditor.setTextToShowWhenEmpty ("Preset name...", Colors::textDim);
    nameEditor.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (nameEditor);

    saveButton.setColour (juce::TextButton::buttonColourId, Colors::accent.withAlpha (0.5f));
    saveButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    saveButton.onClick = [this]() {
        auto name = nameEditor.getText().trim();
        if (name.isEmpty()) return;
        auto file = ChainHostProcessor::getPresetsDirectory().getChildFile (name + ".chainhost");
        juce::MemoryBlock data;
        proc.getStateInformation (data);
        file.replaceWithData (data.getData(), data.getSize());
        nameEditor.clear();
        refresh();
    };
    addAndMakeVisible (saveButton);

    deleteButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.4f));
    deleteButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    deleteButton.onClick = [this]() {
        if (selectedRow >= 0 && selectedRow < presetFiles.size()) {
            presetFiles[selectedRow].deleteFile();
            selectedRow = -1;
            refresh();
        }
    };
    addAndMakeVisible (deleteButton);
    refresh();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bgDeep);
    g.setColour (Colors::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("PRESET BROWSER", 10, 4, 120, 14, juce::Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (18);
    auto bottom = r.removeFromBottom (28);
    nameEditor.setBounds (bottom.removeFromLeft (bottom.getWidth() - 110));
    bottom.removeFromLeft (4);
    saveButton.setBounds (bottom.removeFromLeft (50));
    bottom.removeFromLeft (4);
    deleteButton.setBounds (bottom);
    r.removeFromBottom (4);
    listBox.setBounds (r);
}

void PresetBrowser::refresh()
{
    presetNames.clear(); presetFiles.clear();
    for (auto& f : ChainHostProcessor::getPresetsDirectory().findChildFiles (juce::File::findFiles, false, "*.chainhost"))
    {
        presetFiles.add (f);
        presetNames.add (f.getFileNameWithoutExtension());
    }
    listBox.updateContent(); listBox.repaint();
}

int PresetBrowser::getNumRows() { return presetNames.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (selected) { g.setColour (Colors::accent.withAlpha (0.15f)); g.fillRect (0, 0, w, h); }
    g.setColour (selected ? Colors::accent : Colors::text);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (presetNames[row], 8, 0, w - 16, h, juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    selectedRow = row;
    if (row >= 0 && row < presetFiles.size())
    {
        juce::MemoryBlock data;
        if (presetFiles[row].loadFileAsData (data))
        {
            proc.setStateInformation (data.getData(), (int)data.getSize());
            if (onPresetLoaded) onPresetLoaded();
        }
    }
}

#pragma once
// ChainHostEditor — main plugin editor window, top-level UI orchestrator
#include <juce_gui_extra/juce_gui_extra.h>
#include <melatonin_blur/melatonin_blur.h>
#include <melatonin_inspector/melatonin_inspector.h>
#include "Colors.h"
#include "ChainHostLookAndFeel.h"
#include "FabKnob.h"
#include "PluginWindow.h"
#include "PluginSlotComponent.h"
#include "PresetBrowser.h"
#include "MappingPanel.h"
#include "LfoPanel.h"
#include "../PluginProcessor.h"

class ChainHostEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit ChainHostEditor (ChainHostProcessor&);
    ~ChainHostEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
private:
    void timerCallback() override;
    ChainHostProcessor& proc;
    ChainHostLookAndFeel laf;

    melatonin::DropShadow headerShadow { { juce::Colours::black.withAlpha (0.4f), 12, { 0, 4 } } };

    juce::TextButton scanButton { "SCAN" }, addPluginButton { "+ PLUGIN" },
                     addChainButton { "+ CHAIN" }, presetsToggle { "PRESETS" };
    PresetBrowser presetBrowser;
    bool presetBrowserOpen = false;

    struct ChainViewRow {
        juce::OwnedArray<PluginSlotComponent> slotComponents;
        FabKnob volumeKnob { "Vol", Colors::chainAccent };
        juce::TextButton removeChainButton { juce::CharPointer_UTF8 ("\xc3\x97") },
                         addToChainButton { "+" };
    };

    class ChainContainer : public juce::Component {
    public:
        ChainContainer (ChainHostProcessor& p) : proc (p) {}
        void paint (juce::Graphics& g) override;
        void resized() override;
        juce::OwnedArray<ChainViewRow> chainRows;
    private:
        ChainHostProcessor& proc;
    };

    juce::Viewport chainViewport;
    ChainContainer chainContainer;

    int scrollIdleCounter = 0;
    static constexpr int kScrollHideTicks = 40; // ~2 seconds at 50ms timer
    float scrollAlpha = 0.0f;

    FabKnob macroKnobs[MacroManager::numMacros];
    juce::Label macroLabels[MacroManager::numMacros];
    juce::TextButton learnButtons[MacroManager::numMacros];

    juce::TextButton tabMappings { "MAPPINGS" }, tabLfo { "LFO" };
    int activeTab = 0;
    MappingPanel mappingPanel;
    LfoPanel lfoPanel;

    juce::OwnedArray<PluginWindow> pluginWindows;
    std::map<juce::uint32, juce::uint32> windowCloseTimestamps;
    static constexpr juce::uint32 kReopenCooldownMs = 600;

    void refreshChainView();
    void refreshMacroLabels();
    void showPluginMenu (int chainIndex);
    void openPluginWindow (juce::AudioProcessorGraph::NodeID nodeId);
    void closePluginWindow (juce::AudioProcessorGraph::NodeID nodeId);
    void cleanupClosedWindows();
    void setActiveTab (int tab);

   #if DEBUG || JUCE_DEBUG
    melatonin::Inspector inspector { *this, false };
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainHostEditor)
};

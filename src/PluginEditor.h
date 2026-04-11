#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <gin_plugin/gin_plugin.h>
#include <melatonin_blur/melatonin_blur.h>
#include <melatonin_inspector/melatonin_inspector.h>
#include "PluginProcessor.h"

//==============================================================================
// Colour palette — "Studio Obsidian": warm amber on deep volcanic charcoal
namespace Colors
{
    // Backgrounds — deep obsidian with subtle blue-black undertone
    const juce::Colour bg           (0xff101017);
    const juce::Colour bgDeep       (0xff0b0b12);
    const juce::Colour surface      (0xff181824);
    const juce::Colour surfaceRaised(0xff222234);
    const juce::Colour surfaceHover (0xff2c2c42);

    // Borders — cool slate with structure
    const juce::Colour border       (0xff2e2e44);
    const juce::Colour borderSubtle (0xff232338);
    const juce::Colour borderGlow   (0xff3a3a58);

    // Text — high contrast with warm highlights
    const juce::Colour text         (0xfff0eff6);
    const juce::Colour textMid      (0xff9898b0);
    const juce::Colour textDim      (0xff555570);

    // Accent — molten amber/gold
    const juce::Colour accent       (0xfff0982e);
    const juce::Colour accentBright (0xffffc04a);
    const juce::Colour accentDeep   (0xffc87820);

    // Semantic
    const juce::Colour remove       (0xffe04458);
    const juce::Colour bypass       (0xff4cc88a);
    const juce::Colour wet          (0xff38b8d8);
    const juce::Colour chainAccent  (0xff6e88d0);
    const juce::Colour learn        (0xfff06828);

    // Slot
    const juce::Colour slotBg       (0xff14141f);
    const juce::Colour slotBorder   (0xff2a2a40);

    // LFO — electric cyan
    const juce::Colour lfoBlue      (0xff2ea8f0);
    const juce::Colour lfoBlueDim   (0xff1a6090);
}

//==============================================================================
// Custom LookAndFeel: extends Gin's CopperLookAndFeel with our colour scheme
class ChainHostLookAndFeel : public gin::CopperLookAndFeel
{
public:
    ChainHostLookAndFeel();
};

//==============================================================================
// Arc knob using juce::Slider + CopperLookAndFeel + value overlay
class FabKnob : public juce::Component, public juce::DragAndDropTarget
{
public:
    FabKnob (const juce::String& label = {}, juce::Colour arcCol = Colors::accent);

    void resized() override;
    void paint (juce::Graphics&) override;

    void setValue (float v, bool notify = true);
    float getValue() const;

    void setRange (float mn, float mx);
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setArcColour (juce::Colour c);
    void setSuffix (const juce::String& s) { suffix = s; repaint(); }
    void setDefaultValue (float v) { defaultValue = v; }
    void setShowPercentage (bool b) { showPercentage = b; repaint(); }

    juce::Slider& getSlider() { return slider; }
    std::function<void()> onValueChange;

    // Drag-and-drop target support
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }
    std::function<void (const juce::var&)> onDrop;

private:
    juce::Slider slider;
    juce::String label, suffix = "%";
    juce::Colour arcColour;
    float defaultValue = 0.0f;
    bool showPercentage = true;
    bool dropHover = false;
};

//==============================================================================
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (const juce::String& name, juce::AudioProcessorGraph::NodeID nid,
                  juce::AudioProcessorEditor* editor,
                  std::function<void (juce::AudioProcessorGraph::NodeID)> onClose);
    void closeButtonPressed() override;
    juce::AudioProcessorGraph::NodeID getNodeId() const { return nodeId; }
private:
    juce::AudioProcessorGraph::NodeID nodeId;
    std::function<void (juce::AudioProcessorGraph::NodeID)> closeCallback;
};

//==============================================================================
class PluginSlotComponent : public juce::Component, public juce::DragAndDropTarget
{
public:
    PluginSlotComponent (ChainHostProcessor& p, juce::AudioProcessorGraph::NodeID nid, int ci, int si);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override { return true; }
    void itemDropped (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragHover = false; repaint(); }

    std::function<void (juce::AudioProcessorGraph::NodeID)> onOpenEditor, onRemove;
    std::function<void (int, int, int, int)> onMove;
    juce::AudioProcessorGraph::NodeID getNodeId() const { return nodeId; }

private:
    ChainHostProcessor& proc;
    juce::AudioProcessorGraph::NodeID nodeId;
    int chainIndex, slotIndex;
    bool dragHover = false;
    juce::TextButton bypassButton { "ON" };
    juce::Label nameLabel;
    FabKnob dryWetKnob;
    juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
    melatonin::DropShadow cardShadow { { juce::Colours::black.withAlpha (0.5f), 8, { 0, 3 } } };
    melatonin::DropShadow accentGlow;
};

//==============================================================================
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

//==============================================================================
class MappingPanel : public juce::Component
{
public:
    MappingPanel (ChainHostProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSelectedMacro (int i);
    void refresh();
    void startLearn (int macroIndex);
    void stopLearn();
    bool isLearning() const { return learningMacroIndex >= 0; }
    int getLearningMacro() const { return learningMacroIndex; }
    void checkLearn();
    std::function<void()> onMappingChanged;
private:
    ChainHostProcessor& proc;
    int selectedMacro = 0, learningMacroIndex = -1;
    struct ParamSnapshot { juce::String slotUid; int paramIndex; float value; };
    std::vector<ParamSnapshot> learnSnapshot;
    juce::TextButton macroSelectButtons[MacroManager::numMacros];
    juce::TextButton mapButton { "+ MAP" }, learnButton { "LEARN" };
    struct MappingRow {
        juce::Label label;
        FabKnob minKnob { "Min", Colors::textMid }, maxKnob { "Max", Colors::textMid };
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
        juce::String slotUid; int paramIndex;
    };
    juce::OwnedArray<MappingRow> mappingRows;
    void showParameterPicker();
    void takeParamSnapshot();
};

//==============================================================================
class LfoPanel : public juce::Component
{
public:
    LfoPanel (ChainHostProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    ChainHostProcessor& proc;
    int activeLfo = 0;

    // LFO tab buttons
    juce::TextButton lfoTabs[LfoEngine::numLfos];

    // Controls for the active LFO
    juce::TextButton enableButton { "OFF" };
    juce::TextButton shapeButtons[LfoEngine::NumShapes];
    FabKnob rateKnob { "Rate", Colors::lfoBlue }, depthKnob { "Depth", Colors::lfoBlue };
    juce::TextButton syncToggle { "Hz" };       // toggles between Hz and BPM
    juce::TextButton divButton { "1/4" };       // shows current note division (BPM mode)
    juce::TextButton addTargetButton { "+ TARGET" };

    // Drag handle — drag onto macro knobs to assign LFO target
    struct LfoDragHandle : public juce::Component
    {
        int* activeLfoPtr = nullptr;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
    };
    LfoDragHandle dragHandle;

    struct TargetRow {
        juce::Label label;
        FabKnob minKnob { "Min", Colors::lfoBlue }, maxKnob { "Max", Colors::lfoBlue },
                centerKnob { "Start", Colors::lfoBlue };
        juce::TextButton removeButton { juce::CharPointer_UTF8 ("\xc3\x97") };
    };
    juce::OwnedArray<TargetRow> targetRows;

    void setActiveLfo (int index);
    void showTargetMenu (int lfoIndex);
    void refreshTargetRows();
};

//==============================================================================
class ChainHostEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::Timer
{
public:
    explicit ChainHostEditor (ChainHostProcessor&);
    ~ChainHostEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    ChainHostProcessor& proc;
    ChainHostLookAndFeel laf;

    // Header shadows
    melatonin::DropShadow headerShadow { { juce::Colours::black.withAlpha (0.4f), 12, { 0, 4 } } };

    juce::TextButton scanButton { "SCAN" }, addPluginButton { "+ PLUGIN" },
                     addChainButton { "+ CHAIN" }, presetsToggle { "PRESETS" };
    PresetBrowser presetBrowser;
    bool presetBrowserOpen = false;

    struct ChainViewRow {
        juce::OwnedArray<PluginSlotComponent> slotComponents;
        FabKnob volumeKnob { "Vol", Colors::chainAccent };
        juce::TextButton muteButton;  // chain number, toggles mute
        juce::TextButton removeChainButton { juce::CharPointer_UTF8 ("\xc3\x97") },
                         addToChainButton { "+" };
    };
    juce::OwnedArray<ChainViewRow> chainRows;

    FabKnob macroKnobs[MacroManager::numMacros];
    juce::Label macroLabels[MacroManager::numMacros];
    juce::TextButton learnButtons[MacroManager::numMacros];
    juce::TextButton clearButtons[MacroManager::numMacros];

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

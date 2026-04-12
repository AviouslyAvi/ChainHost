#include "LfoPanel.h"
#include "../PluginProcessor.h"

LfoPanel::LfoPanel (ChainHostProcessor& p) : proc (p)
{
    for (int i = 0; i < LfoEngine::numLfos; ++i)
    {
        lfoTabs[i].setButtonText ("LFO " + juce::String (i + 1));
        lfoTabs[i].setColour (juce::TextButton::textColourOffId, Colors::text);
        lfoTabs[i].onClick = [this, i]() { setActiveLfo (i); };
        addAndMakeVisible (lfoTabs[i]);
    }

    enableButton.setClickingTogglesState (true);
    enableButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    enableButton.setColour (juce::TextButton::textColourOnId, Colors::text);
    enableButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.enabled = enableButton.getToggleState();
        syncControlsToLfo();
    };
    addAndMakeVisible (enableButton);

    for (int s = 0; s < LfoEngine::NumShapes; ++s)
        shapeBox.addItem (LfoEngine::shapeName ((LfoEngine::Shape) s), s + 1);
    shapeBox.setColour (juce::ComboBox::backgroundColourId, Colors::surfaceRaised);
    shapeBox.setColour (juce::ComboBox::textColourId, Colors::text);
    shapeBox.setColour (juce::ComboBox::outlineColourId, Colors::border);
    shapeBox.onChange = [this]() {
        int s = shapeBox.getSelectedId() - 1;
        if (s >= 0)
        {
            auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
            lfo.shape = (LfoEngine::Shape) s;
            lfo.breakpoints = LfoEngine::generateBreakpointsForShape ((LfoEngine::Shape) s);
            syncControlsToLfo();
        }
    };
    addAndMakeVisible (shapeBox);

    rateKnob.setRange (LfoEngine::kMinRate, LfoEngine::kMaxRate);
    rateKnob.setDefaultValue (1.0f);
    rateKnob.setSuffix (" Hz");
    rateKnob.setShowPercentage (true);
    rateKnob.onValueChange = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        if (lfo.tempoSync)
        {
            int div = juce::jlimit (0, (int) NumSyncDivs - 1, juce::roundToInt (rateKnob.getValue()));
            lfo.syncDiv = (SyncDiv) div;
            rateKnob.setSuffix ("");
            // Update displayed text to show division name
        }
        else
            lfo.rate = rateKnob.getValue();
    };
    addAndMakeVisible (rateKnob);

    depthKnob.setDefaultValue (0.5f);
    depthKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).depth = depthKnob.getValue(); };
    addAndMakeVisible (depthKnob);

    riseKnob.setRange (0.0f, 5.0f);
    riseKnob.setDefaultValue (0.0f);
    riseKnob.setSuffix (" s");
    riseKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).riseTime = riseKnob.getValue(); };
    addAndMakeVisible (riseKnob);

    smoothKnob.setDefaultValue (0.0f);
    smoothKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).smooth = smoothKnob.getValue(); };
    addAndMakeVisible (smoothKnob);

    delayKnob.setRange (0.0f, 5.0f);
    delayKnob.setDefaultValue (0.0f);
    delayKnob.setSuffix (" s");
    delayKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).delayTime = delayKnob.getValue(); };
    addAndMakeVisible (delayKnob);

    phaseKnob.setDefaultValue (0.0f);
    phaseKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).startPhase = phaseKnob.getValue(); };
    addAndMakeVisible (phaseKnob);

    auto styleToggle = [] (juce::TextButton& b) {
        b.setColour (juce::TextButton::textColourOffId, Colors::text);
        b.setColour (juce::TextButton::textColourOnId, Colors::text);
    };

    styleToggle (syncButton);
    syncButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.tempoSync = ! lfo.tempoSync;
        syncControlsToLfo();
    };
    addAndMakeVisible (syncButton);

    styleToggle (retrigButton);
    retrigButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.retrigger = ! lfo.retrigger;
        syncControlsToLfo();
    };
    addAndMakeVisible (retrigButton);

    styleToggle (polarButton);
    polarButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.unipolar = ! lfo.unipolar;
        syncControlsToLfo();
    };
    addAndMakeVisible (polarButton);

    styleToggle (envButton);
    envButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.envelopeMode = ! lfo.envelopeMode;
        lfo.envelopeDone = false;
        syncControlsToLfo();
    };
    addAndMakeVisible (envButton);

    styleToggle (directionButton);
    directionButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        int d = ((int) lfo.direction + 1) % 3;
        lfo.direction = (LfoEngine::Direction) d;
        syncControlsToLfo();
    };
    addAndMakeVisible (directionButton);

    for (int d = 0; d < NumSyncDivs; ++d)
        syncDivBox.addItem (syncDivName ((SyncDiv) d), d + 1);
    syncDivBox.setColour (juce::ComboBox::backgroundColourId, Colors::surfaceRaised);
    syncDivBox.setColour (juce::ComboBox::textColourId, Colors::text);
    syncDivBox.setColour (juce::ComboBox::outlineColourId, Colors::border);
    syncDivBox.onChange = [this]() {
        proc.getLfoEngine().getLfo (activeLfo).syncDiv = (SyncDiv) (syncDivBox.getSelectedId() - 1);
    };
    addAndMakeVisible (syncDivBox);

    waveformEditor.onChanged = [this]() { repaint(); };
    waveformEditor.isEnvelopeMode = [this]() { return proc.getLfoEngine().getLfo (activeLfo).envelopeMode; };
    addAndMakeVisible (waveformEditor);

    auto styleToolBtn = [this] (juce::TextButton& btn, LfoWaveformEditor::Tool tool) {
        btn.setColour (juce::TextButton::textColourOffId, Colors::text);
        btn.setColour (juce::TextButton::textColourOnId, Colors::text);
        btn.onClick = [this, tool, &btn]() {
            waveformEditor.setTool (tool);
            toolPointer.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            toolFlat.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            toolRampUp.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            toolRampDown.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            btn.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.4f));
        };
        addAndMakeVisible (btn);
    };
    styleToolBtn (toolPointer, LfoWaveformEditor::PointerTool);
    styleToolBtn (toolFlat, LfoWaveformEditor::FlatTool);
    styleToolBtn (toolRampUp, LfoWaveformEditor::RampUpTool);
    styleToolBtn (toolRampDown, LfoWaveformEditor::RampDownTool);
    toolPointer.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.4f));

    // Grid size controls
    auto setupGridBox = [] (juce::ComboBox& box, const int sizes[], int count, int defaultVal) {
        for (int i = 0; i < count; ++i)
            box.addItem (juce::String (sizes[i]), i + 1);
        // Select default
        for (int i = 0; i < count; ++i)
            if (sizes[i] == defaultVal) { box.setSelectedId (i + 1, juce::dontSendNotification); break; }
    };
    static const int gridXSizes[] = { 4, 8, 12, 16, 24, 32 };
    static const int gridYSizes[] = { 2, 4, 6, 8, 12, 16 };
    setupGridBox (gridXBox, gridXSizes, 6, waveformEditor.gridX);
    setupGridBox (gridYBox, gridYSizes, 6, waveformEditor.gridY);

    gridXBox.onChange = [this]() {
        static const int sizes[] = { 4, 8, 12, 16, 24, 32 };
        int idx = gridXBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6) waveformEditor.setGridX (sizes[idx]);
    };
    gridYBox.onChange = [this]() {
        static const int sizes[] = { 2, 4, 6, 8, 12, 16 };
        int idx = gridYBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6) waveformEditor.setGridY (sizes[idx]);
    };
    addAndMakeVisible (gridXBox);
    addAndMakeVisible (gridYBox);

    addTargetButton.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.3f));
    addTargetButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    addTargetButton.onClick = [this]() { showTargetMenu (activeLfo); };
    addAndMakeVisible (addTargetButton);

    refresh();
}

void LfoPanel::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bgDeep);

    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);

    int wfRight = getWidth() / 2 - 10;
    int toolY = 46;
    int wfBottom = toolY + 16 + 94;
    int gridRowY = wfBottom + 2;

    // --- Section boxes ---
    // Controls row (SYNC, RETRIG, etc.)
    g.setColour (Colors::border.withAlpha (0.25f));
    g.drawRoundedRectangle (6.0f, 24.0f, (float) wfRight - 2.0f, 20.0f, 3.0f, 1.0f);

    // Tool bar box
    g.drawRoundedRectangle (6.0f, (float) toolY - 2.0f, (float) wfRight - 2.0f, 16.0f, 3.0f, 1.0f);

    // Waveform box (drawn by the editor itself, add subtle outer glow)
    g.setColour (Colors::border.withAlpha (0.15f));
    g.drawRoundedRectangle (8.0f, (float) toolY + 14.0f, (float) wfRight - 12.0f, 98.0f, 2.0f, 1.0f);

    // Grid controls labels
    g.setColour (Colors::textDim.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (8.0f)));
    g.drawText ("X", wfRight - 114, gridRowY, 12, 16, juce::Justification::centredRight);
    g.drawText ("Y", wfRight - 64, gridRowY, 12, 16, juce::Justification::centredRight);

    // Knob section box
    int ky = gridRowY + 20;
    g.setColour (Colors::border.withAlpha (0.25f));
    g.drawRoundedRectangle (6.0f, (float) ky - 4.0f, 274.0f, 66.0f, 3.0f, 1.0f);

    // --- Targets section ---
    int targetsX = getWidth() / 2 + 10;

    // Targets box
    g.setColour (Colors::border.withAlpha (0.25f));
    g.drawRoundedRectangle ((float) targetsX - 6.0f, 24.0f,
        (float) (getWidth() - targetsX + 2), (float) (getHeight() - 28), 3.0f, 1.0f);

    g.setColour (Colors::textDim.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    g.drawText ("TARGETS", targetsX, 28, 80, 12, juce::Justification::centredLeft);

    if (lfo.targets.empty())
    {
        g.setColour (Colors::textDim.withAlpha (0.3f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("No targets assigned", targetsX, 44, 200, 16, juce::Justification::centredLeft);
    }
}

void LfoPanel::resized()
{
    int tabW = 48;
    for (int i = 0; i < LfoEngine::numLfos; ++i)
        lfoTabs[i].setBounds (10 + i * (tabW + 3), 4, tabW, 18);

    int ctrlX = 10 + LfoEngine::numLfos * (tabW + 3) + 6;
    enableButton.setBounds (ctrlX, 4, 32, 18);

    shapeBox.setBounds (ctrlX + 38, 4, 110, 18);
    int my = 26;
    syncButton.setBounds (10, my, 38, 16);
    syncDivBox.setBounds (52, my, 68, 16);
    retrigButton.setBounds (126, my, 36, 16);
    polarButton.setBounds (166, my, 32, 16);
    envButton.setBounds (202, my, 32, 16);
    directionButton.setBounds (238, my, 32, 16);

    int wfRight = getWidth() / 2 - 10;
    // Tool buttons above waveform
    int toolY = 46;
    toolPointer.setBounds (10, toolY, 30, 14);
    toolFlat.setBounds (43, toolY, 36, 14);
    toolRampUp.setBounds (82, toolY, 24, 14);
    toolRampDown.setBounds (109, toolY, 24, 14);
    waveformEditor.setBounds (10, toolY + 16, wfRight - 10, 94);

    // Grid size controls below waveform
    int gridY2 = toolY + 16 + 96;
    gridXBox.setBounds (wfRight - 100, gridY2, 40, 16);
    gridYBox.setBounds (wfRight - 50, gridY2, 40, 16);

    int ky = gridY2 + 20;
    int knobW = 44;
    rateKnob.setBounds (10, ky, knobW, 60);
    depthKnob.setBounds (10 + knobW, ky, knobW, 60);
    delayKnob.setBounds (10 + knobW * 2, ky, knobW, 60);
    phaseKnob.setBounds (10 + knobW * 3, ky, knobW, 60);
    riseKnob.setBounds (10 + knobW * 4, ky, knobW, 60);
    smoothKnob.setBounds (10 + knobW * 5, ky, knobW, 60);

    int targetsX = getWidth() / 2 + 10;
    addTargetButton.setBounds (targetsX + 90, 26, 72, 18);

    int ty = 44;
    for (auto* tr : targetRows)
    {
        tr->label.setBounds (targetsX, ty, getWidth() / 2 - 50, 16);
        tr->removeButton.setBounds (getWidth() - 30, ty, 16, 16);
        ty += 19;
    }
}

void LfoPanel::syncControlsToLfo()
{
    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);

    enableButton.setToggleState (lfo.enabled, juce::dontSendNotification);
    enableButton.setButtonText (lfo.enabled ? "ON" : "OFF");
    enableButton.setColour (juce::TextButton::buttonColourId,
        lfo.enabled ? Colors::lfoBlue.withAlpha (0.5f) : Colors::surfaceRaised);
    enableButton.setColour (juce::TextButton::buttonOnColourId, Colors::lfoBlue.withAlpha (0.5f));

    shapeBox.setSelectedId ((int) lfo.shape + 1, juce::dontSendNotification);

    syncButton.setButtonText (lfo.tempoSync ? "SYNC" : "FREE");
    syncDivBox.setVisible (lfo.tempoSync);
    syncDivBox.setSelectedId ((int) lfo.syncDiv + 1, juce::dontSendNotification);

    // Reconfigure rate knob for sync vs free mode
    if (lfo.tempoSync)
    {
        rateKnob.setRange (0.0f, (float) (NumSyncDivs - 1));
        rateKnob.getSlider().setNumDecimalPlacesToDisplay (0);
        rateKnob.setValue ((float) lfo.syncDiv, false);
        rateKnob.setSuffix ("");
        rateKnob.setLabel (syncDivName (lfo.syncDiv));
    }
    else
    {
        rateKnob.setRange (LfoEngine::kMinRate, LfoEngine::kMaxRate);
        rateKnob.getSlider().setNumDecimalPlacesToDisplay (2);
        rateKnob.setSuffix (" Hz");
        rateKnob.setLabel ("Rate");
    }

    {
        auto styleActive = [] (juce::TextButton& btn, bool active) {
            auto bg = active ? Colors::lfoBlue.withAlpha (0.6f) : Colors::surfaceRaised;
            auto fg = active ? juce::Colours::white : Colors::text;
            btn.setColour (juce::TextButton::buttonColourId, bg);
            btn.setColour (juce::TextButton::buttonOnColourId, bg);
            btn.setColour (juce::TextButton::textColourOffId, fg);
            btn.setColour (juce::TextButton::textColourOnId, fg);
        };
        styleActive (retrigButton, lfo.retrigger);
        styleActive (envButton, lfo.envelopeMode);
        styleActive (syncButton, lfo.tempoSync);

        polarButton.setButtonText (lfo.unipolar ? "UNI" : "BI");
        styleActive (polarButton, ! lfo.unipolar);
    }

    if (lfo.breakpoints.empty())
        lfo.breakpoints = LfoEngine::generateBreakpointsForShape (lfo.shape);
    waveformEditor.setBreakpoints (&lfo.breakpoints);
    waveformEditor.setEnabled (lfo.enabled);
    waveformEditor.setPhase (lfo.phase);

    if (! lfo.tempoSync)
        rateKnob.setValue (lfo.rate, false);
    depthKnob.setValue (lfo.depth, false);
    riseKnob.setValue (lfo.riseTime, false);
    smoothKnob.setValue (lfo.smooth, false);
    delayKnob.setValue (lfo.delayTime, false);
    phaseKnob.setValue (lfo.startPhase, false);

    // Direction button
    {
        static const char* dirLabels[] = { "FWD", "REV", "P.P." };
        directionButton.setButtonText (dirLabels[(int) lfo.direction]);
        auto styleActive = [] (juce::TextButton& btn, bool active) {
            auto bg = active ? Colors::lfoBlue.withAlpha (0.6f) : Colors::surfaceRaised;
            auto fg = active ? juce::Colours::white : Colors::text;
            btn.setColour (juce::TextButton::buttonColourId, bg);
            btn.setColour (juce::TextButton::buttonOnColourId, bg);
            btn.setColour (juce::TextButton::textColourOffId, fg);
            btn.setColour (juce::TextButton::textColourOnId, fg);
        };
        styleActive (directionButton, lfo.direction != LfoEngine::Forward);
    }

    repaint();
}

void LfoPanel::refresh()
{
    for (int i = 0; i < LfoEngine::numLfos; ++i)
        lfoTabs[i].setColour (juce::TextButton::buttonColourId,
            i == activeLfo ? Colors::lfoBlue.withAlpha (0.45f) : Colors::surfaceRaised);

    syncControlsToLfo();
    refreshTargetRows();
    resized();
}

void LfoPanel::updatePhase()
{
    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
    waveformEditor.setPhase (lfo.phase);
    waveformEditor.repaint();

    // Keep knobs in sync when macros drive LFO params
    if (! lfo.tempoSync)
        rateKnob.setValue (lfo.rate, false);
    depthKnob.setValue (lfo.depth, false);
    riseKnob.setValue (lfo.riseTime, false);
    smoothKnob.setValue (lfo.smooth, false);
    delayKnob.setValue (lfo.delayTime, false);
    phaseKnob.setValue (lfo.startPhase, false);
}

void LfoPanel::setActiveLfo (int index)
{
    activeLfo = juce::jlimit (0, LfoEngine::numLfos - 1, index);
    refresh();
}

void LfoPanel::refreshTargetRows()
{
    targetRows.clear();
    auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
    for (int ti = 0; ti < (int) lfo.targets.size(); ++ti)
    {
        auto* tr = targetRows.add (new TargetRow());
        auto& t = lfo.targets[(size_t) ti];
        juce::String lbl;
        if (t.type == LfoTarget::Macro)
            lbl = "Macro " + juce::String (t.macroIndex + 1);
        else if (auto* node = proc.getGraph().getNodeForId (t.nodeId))
        {
            auto& params = node->getProcessor()->getParameters();
            lbl = node->getProcessor()->getName() + " > "
                + (t.paramIndex < params.size() ? params[t.paramIndex]->getName (24) : juce::String ("?"));
        }
        tr->label.setText (lbl, juce::dontSendNotification);
        tr->label.setColour (juce::Label::textColourId, Colors::text);
        tr->label.setFont (juce::Font (juce::FontOptions (9.5f)));
        addAndMakeVisible (tr->label);
        tr->removeButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        tr->removeButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        tr->removeButton.onClick = [this, ti]() {
            proc.getLfoEngine().removeTarget (activeLfo, ti);
            refresh();
        };
        addAndMakeVisible (tr->removeButton);
    }
}

void LfoPanel::showTargetMenu (int lfoIndex)
{
    juce::PopupMenu menu;
    int itemId = 1;
    juce::PopupMenu macroSub;
    for (int i = 0; i < MacroManager::numMacros; ++i)
        macroSub.addItem (itemId++, "Macro " + juce::String (i + 1));
    menu.addSubMenu ("Macros", macroSub);

    struct PI { LfoTarget t; };
    std::vector<PI> lookup;
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        LfoTarget t;
        t.type = LfoTarget::Macro;
        t.macroIndex = i;
        lookup.push_back ({ t });
    }

    auto& cg = proc.getChainGraph();
    for (int ci = 0; ci < cg.getNumChains(); ++ci)
    {
        for (auto& slot : cg.getChain (ci).slots)
        {
            if (auto* node = proc.getGraph().getNodeForId (slot.nodeId))
            {
                auto& params = node->getProcessor()->getParameters();
                if (params.isEmpty()) continue;
                juce::PopupMenu sub;
                for (int pi = 0; pi < params.size(); ++pi)
                {
                    LfoTarget t;
                    t.type = LfoTarget::Parameter;
                    t.nodeId = slot.nodeId;
                    t.paramIndex = pi;
                    lookup.push_back ({ t });
                    sub.addItem (itemId++, params[pi]->getName (40));
                }
                menu.addSubMenu (node->getProcessor()->getName(), sub);
            }
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addTargetButton),
        [this, lfoIndex, lookup] (int r) {
            if (r > 0 && r <= (int) lookup.size())
            {
                proc.getLfoEngine().addTarget (lfoIndex, lookup[(size_t) (r - 1)].t);
                refresh();
            }
        });
}

void LfoPanel::setupMacroDropHandlers()
{
    // Map LFO knobs to InternalParams indices for macro drag-drop
    // Each LFO has 6 params: Rate(0), Depth(1), Rise(2), Smooth(3), Delay(4), Phase(5)
    auto makeHandler = [this] (int paramOffset) {
        return [this, paramOffset] (int macroIdx) {
            int internalIdx = activeLfo * InternalParams::paramsPerLfo + paramOffset;
            proc.getMacroManager().addMapping (macroIdx, InternalParams::uid, internalIdx, 0.0f, 1.0f);
        };
    };
    rateKnob.onMacroDropped   = makeHandler (0);
    depthKnob.onMacroDropped  = makeHandler (1);
    riseKnob.onMacroDropped   = makeHandler (2);
    smoothKnob.onMacroDropped = makeHandler (3);
    delayKnob.onMacroDropped  = makeHandler (4);
    phaseKnob.onMacroDropped  = makeHandler (5);
}

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
    addAndMakeVisible (waveformEditor);

    auto styleToolBtn = [this] (juce::TextButton& btn, LfoWaveformEditor::Tool tool) {
        btn.setColour (juce::TextButton::textColourOffId, Colors::text);
        btn.setColour (juce::TextButton::textColourOnId, Colors::text);
        btn.onClick = [this, tool, &btn]() {
            waveformEditor.setTool (tool);
            toolPointer.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            toolPencil.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            toolEraser.setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
            btn.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.4f));
        };
        addAndMakeVisible (btn);
    };
    styleToolBtn (toolPointer, LfoWaveformEditor::PointerTool);
    styleToolBtn (toolPencil, LfoWaveformEditor::PencilTool);
    styleToolBtn (toolEraser, LfoWaveformEditor::EraserTool);
    toolPointer.setColour (juce::TextButton::buttonColourId, Colors::lfoBlue.withAlpha (0.4f));

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

    int targetsX = getWidth() / 2 + 10;
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

    int wfRight = getWidth() / 2 - 10;
    // Tool buttons above waveform
    int toolY = 46;
    toolPointer.setBounds (10, toolY, 30, 14);
    toolPencil.setBounds (43, toolY, 36, 14);
    toolEraser.setBounds (82, toolY, 42, 14);
    waveformEditor.setBounds (10, toolY + 16, wfRight - 10, 94);

    int ky = 160;
    rateKnob.setBounds (10, ky, 56, 60);
    depthKnob.setBounds (70, ky, 56, 60);
    riseKnob.setBounds (130, ky, 56, 60);
    smoothKnob.setBounds (190, ky, 56, 60);

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
    syncButton.setColour (juce::TextButton::buttonColourId,
        lfo.tempoSync ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);
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

    retrigButton.setColour (juce::TextButton::buttonColourId,
        lfo.retrigger ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

    polarButton.setButtonText (lfo.unipolar ? "UNI" : "BI");
    polarButton.setColour (juce::TextButton::buttonColourId,
        lfo.unipolar ? Colors::surfaceRaised : Colors::lfoBlue.withAlpha (0.4f));

    envButton.setColour (juce::TextButton::buttonColourId,
        lfo.envelopeMode ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

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
    // Each LFO has 4 params: Rate(0), Depth(1), Rise(2), Smooth(3)
    auto makeHandler = [this] (int paramOffset) {
        return [this, paramOffset] (int macroIdx) {
            int internalIdx = activeLfo * 4 + paramOffset;
            proc.getMacroManager().addMapping (macroIdx, InternalParams::uid, internalIdx, 0.0f, 1.0f);
        };
    };
    rateKnob.onMacroDropped   = makeHandler (0);
    depthKnob.onMacroDropped  = makeHandler (1);
    riseKnob.onMacroDropped   = makeHandler (2);
    smoothKnob.onMacroDropped = makeHandler (3);
}

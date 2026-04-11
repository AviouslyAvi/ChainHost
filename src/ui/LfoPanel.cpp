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
    {
        shapeButtons[s].setButtonText (LfoEngine::shapeName ((LfoEngine::Shape) s));
        shapeButtons[s].setColour (juce::TextButton::textColourOffId, Colors::text);
        shapeButtons[s].onClick = [this, s]() {
            auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
            lfo.shape = (LfoEngine::Shape) s;
            lfo.useCustomShape = false;
            syncControlsToLfo();
        };
        addAndMakeVisible (shapeButtons[s]);
    }

    customShapeButton.setColour (juce::TextButton::textColourOffId, Colors::text);
    customShapeButton.onClick = [this]() {
        auto& lfo = proc.getLfoEngine().getLfo (activeLfo);
        lfo.useCustomShape = ! lfo.useCustomShape;
        if (lfo.useCustomShape && lfo.breakpoints.empty())
            lfo.breakpoints = LfoEngine::defaultBreakpoints();
        syncControlsToLfo();
    };
    addAndMakeVisible (customShapeButton);

    rateKnob.setRange (LfoEngine::kMinRate, LfoEngine::kMaxRate);
    rateKnob.setDefaultValue (1.0f);
    rateKnob.setSuffix (" Hz");
    rateKnob.setShowPercentage (true);
    rateKnob.onValueChange = [this]() { proc.getLfoEngine().getLfo (activeLfo).rate = rateKnob.getValue(); };
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

    int sx = ctrlX + 38;
    for (int s = 0; s < LfoEngine::NumShapes; ++s)
    {
        shapeButtons[s].setBounds (sx, 4, 30, 18);
        sx += 33;
    }
    customShapeButton.setBounds (sx, 4, 40, 18);

    int my = 26;
    syncButton.setBounds (10, my, 38, 16);
    syncDivBox.setBounds (52, my, 68, 16);
    retrigButton.setBounds (126, my, 36, 16);
    polarButton.setBounds (166, my, 32, 16);
    envButton.setBounds (202, my, 32, 16);

    int wfRight = getWidth() / 2 - 10;
    waveformEditor.setBounds (10, 46, wfRight - 10, 64);

    int ky = 114;
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

    for (int s = 0; s < LfoEngine::NumShapes; ++s)
        shapeButtons[s].setColour (juce::TextButton::buttonColourId,
            (!lfo.useCustomShape && (int) lfo.shape == s) ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

    customShapeButton.setColour (juce::TextButton::buttonColourId,
        lfo.useCustomShape ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

    syncButton.setButtonText (lfo.tempoSync ? "SYNC" : "FREE");
    syncButton.setColour (juce::TextButton::buttonColourId,
        lfo.tempoSync ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);
    syncDivBox.setVisible (lfo.tempoSync);
    syncDivBox.setSelectedId ((int) lfo.syncDiv + 1, juce::dontSendNotification);

    rateKnob.setVisible (! lfo.tempoSync);

    retrigButton.setColour (juce::TextButton::buttonColourId,
        lfo.retrigger ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

    polarButton.setButtonText (lfo.unipolar ? "UNI" : "BI");
    polarButton.setColour (juce::TextButton::buttonColourId,
        lfo.unipolar ? Colors::surfaceRaised : Colors::lfoBlue.withAlpha (0.4f));

    envButton.setColour (juce::TextButton::buttonColourId,
        lfo.envelopeMode ? Colors::lfoBlue.withAlpha (0.4f) : Colors::surfaceRaised);

    if (lfo.useCustomShape)
        waveformEditor.setBreakpoints (&lfo.breakpoints);
    else
        waveformEditor.setBreakpoints (nullptr);

    waveformEditor.setPresetShape (lfo.shape);
    waveformEditor.setEnabled (lfo.enabled);
    waveformEditor.setPhase (lfo.phase);

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

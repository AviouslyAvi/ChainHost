#include "ChainHostEditor.h"

ChainHostEditor::ChainHostEditor (ChainHostProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBrowser (p), chainContainer (p), mappingPanel (p), lfoPanel (p)
{
    setLookAndFeel (&laf);

    auto styleBtn = [] (juce::TextButton& b, juce::Colour bg, juce::Colour fg = Colors::text) {
        b.setColour (juce::TextButton::buttonColourId, bg);
        b.setColour (juce::TextButton::textColourOffId, fg);
    };
    styleBtn (scanButton, Colors::surfaceRaised);
    styleBtn (addPluginButton, Colors::accent.withAlpha (0.45f));
    styleBtn (addChainButton, Colors::chainAccent.withAlpha (0.35f));
    styleBtn (presetsToggle, Colors::surfaceRaised);

    scanButton.onClick = [this]() {
        scanButton.setButtonText ("..."); scanButton.setEnabled (false);
        scanProgressPanel.startScan();
        scanProgressPanel.setVisible (true);
        resized();

        auto& scanner = proc.getScanner();

        scanner.onScanProgress = [this] (float p, const juce::String& name) {
            scanProgressPanel.updateProgress (p, name);
        };

        scanner.onScanFinished = [this] (int, const juce::StringArray&) {
            scanProgressPanel.setVisible (false);
            resized();
        };

        scanner.onScanComplete = [this]() {
            scanButton.setButtonText ("SCAN");
            scanButton.setEnabled (true);
        };

        scanner.scan();
    };
    addPluginButton.onClick = [this]() { showPluginMenu (0); };
    addChainButton.onClick = [this]() { proc.getChainGraph().addParallelChain (proc.getGraph()); refreshChainView(); };
    presetsToggle.onClick = [this]() {
        presetBrowserOpen = !presetBrowserOpen;
        presetsToggle.setColour (juce::TextButton::buttonColourId, presetBrowserOpen ? Colors::accent.withAlpha (0.45f) : Colors::surfaceRaised);
        presetBrowser.setVisible (presetBrowserOpen);
        if (presetBrowserOpen) presetBrowser.refresh();
        resized();
    };
    for (auto* btn : { &scanButton, &addPluginButton, &addChainButton, &presetsToggle })
        addAndMakeVisible (btn);

    presetBrowser.setVisible (false);
    presetBrowser.onPresetLoaded = [this]() {
        pluginWindows.clear();
        windowCloseTimestamps.clear();
        refreshChainView();
        mappingPanel.refresh();
        lfoPanel.refresh();
        refreshMacroLabels();
    };
    addAndMakeVisible (presetBrowser);

    scanProgressPanel.onDismissed = [this]() {
        scanProgressPanel.setVisible (false);
        resized();
    };
    addChildComponent (scanProgressPanel);

    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        macroKnobs[i].setArcColour (Colors::macroAccents[i]);
        macroKnobs[i].setDefaultValue (0.0f);
        macroKnobs[i].setShowPercentage (false);
        macroKnobs[i].setLabel ({});
        macroKnobs[i].onValueChange = [this, i]() { proc.getParameters()[i]->setValueNotifyingHost (macroKnobs[i].getValue()); };
        addAndMakeVisible (macroKnobs[i]);

        macroNames[i] = proc.getMacroManager().getMacroName (i);
        if (macroNames[i].isEmpty()) macroNames[i] = "Macro " + juce::String (i + 1);
        macroLabels[i].setText (macroNames[i], juce::dontSendNotification);
        macroLabels[i].setJustificationType (juce::Justification::centred);
        macroLabels[i].setColour (juce::Label::textColourId, Colors::textDim);
        macroLabels[i].setFont (juce::Font (juce::FontOptions (8.0f)));
        macroLabels[i].setEditable (false, true, false);  // double-click to edit
        macroLabels[i].onTextChange = [this, i]() {
            macroNames[i] = macroLabels[i].getText();
            proc.getMacroManager().setMacroName (i, macroNames[i]);
        };
        macroLabels[i].setInterceptsMouseClicks (true, true);
        macroLabels[i].addMouseListener (this, false);
        addAndMakeVisible (macroLabels[i]);

        auto* handle = dragHandleStorage.add (new MacroDragHandle (i));
        macroDragHandles[i] = handle;
        handle->setTooltip ("Drag onto a plugin parameter to assign this macro");
        handle->addMouseListener (this, false);
        addAndMakeVisible (handle);

        macroLearnBtns[i].setButtonText ("LEARN");
        macroLearnBtns[i].setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised.withAlpha (0.7f));
        macroLearnBtns[i].setColour (juce::TextButton::textColourOffId, Colors::textDim);
        macroLearnBtns[i].setTooltip ("Learn — move a parameter to map it");
        macroLearnBtns[i].onClick = [this, i]() {
            if (mappingPanel.isLearning() && mappingPanel.getLearningMacro() == i)
                mappingPanel.stopLearn();
            else
                mappingPanel.startLearn (i);
            for (int j = 0; j < MacroManager::numMacros; ++j) {
                bool learning = mappingPanel.isLearning() && mappingPanel.getLearningMacro() == j;
                macroLearnBtns[j].setColour (juce::TextButton::buttonColourId, learning ? Colors::learn.withAlpha (0.5f) : Colors::surfaceRaised);
                macroLearnBtns[j].setColour (juce::TextButton::textColourOffId, learning ? Colors::learn : Colors::textDim);
            }
        };
        addAndMakeVisible (macroLearnBtns[i]);

        macroLinkBtns[i].setTooltip ("Browse and select a plugin parameter to map to this macro");
        macroLinkBtns[i].onClick = [this, i]() { selectMacro (i); showMacroLinkMenu (i); };
        addAndMakeVisible (macroLinkBtns[i]);

        // Right-click context menu on macro knobs — remove LFO/macro mappings
        macroKnobs[i].onBuildContextMenu = [this, i] (juce::PopupMenu& menu) {
            auto& lfoEngine = proc.getLfoEngine();
            int itemId = 1;
            bool hasLfo = false;
            for (int li = 0; li < LfoEngine::numLfos; ++li)
            {
                auto& lfo = lfoEngine.getLfo (li);
                for (int ti = 0; ti < (int) lfo.targets.size(); ++ti)
                {
                    auto& t = lfo.targets[(size_t) ti];
                    if (t.type == LfoTarget::Macro && t.macroIndex == i)
                    {
                        menu.addItem (itemId, "Remove LFO " + juce::String (li + 1) + " modulation");
                        hasLfo = true;
                    }
                    ++itemId;
                }
            }
            auto& mappings = proc.getMacroManager().getMappings (i);
            if (! mappings.empty())
            {
                if (hasLfo) menu.addSeparator();
                menu.addItem (1000, "Remove all macro mappings (" + juce::String ((int) mappings.size()) + ")");
            }
        };
        macroKnobs[i].onContextMenuResult = [this, i] (int result) {
            if (result == 1000)
            {
                proc.getMacroManager().clearMappings (i);
                refreshMacroLabels();
                return;
            }
            // Find the LFO target to remove
            auto& lfoEngine = proc.getLfoEngine();
            int itemId = 1;
            for (int li = 0; li < LfoEngine::numLfos; ++li)
            {
                auto& lfo = lfoEngine.getLfo (li);
                for (int ti = 0; ti < (int) lfo.targets.size(); ++ti)
                {
                    if (itemId == result)
                    {
                        lfoEngine.removeTarget (li, ti);
                        lfoPanel.refresh();
                        return;
                    }
                    ++itemId;
                }
            }
        };

        // Dragging the blue halo on a macro knob adjusts the LFO depth targeting it
        macroKnobs[i].onModDepthChanged = [this, i] (float newDepth) {
            auto& lfoEngine = proc.getLfoEngine();
            for (int li = 0; li < LfoEngine::numLfos; ++li)
            {
                auto& lfo = lfoEngine.getLfo (li);
                for (auto& t : lfo.targets)
                    if (t.type == LfoTarget::Macro && t.macroIndex == i)
                    {
                        lfo.depth = juce::jlimit (0.0f, 1.0f, newDepth);
                        lfoPanel.updatePhase();
                        return;
                    }
            }
        };

        // Accept LFO drag-drop on macro knobs → add as LFO target
        macroKnobs[i].onLfoDropped = [this, i] (int lfoIdx) {
            LfoTarget t;
            t.type = LfoTarget::Macro;
            t.macroIndex = i;
            proc.getLfoEngine().addTarget (lfoIdx, t);
            lfoPanel.refresh();
        };
    }
    selectMacro (0);

    mappingPanel.onMappingChanged = [this]() { refreshMacroLabels(); };
    addChildComponent (mappingPanel);  // hidden — learn backend only
    lfoPanel.setupMacroDropHandlers();
    addAndMakeVisible (lfoPanel);

    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (true, false, true, false);
    addAndMakeVisible (chainViewport);

    refreshChainView(); refreshMacroLabels();
    setSize (940, 820);
    startTimer (50);
}

ChainHostEditor::~ChainHostEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
    pluginWindows.clear();
}

void ChainHostEditor::timerCallback()
{
    cleanupClosedWindows();
    mappingPanel.checkLearn();
    if (!mappingPanel.isLearning())
    {
        for (int j = 0; j < MacroManager::numMacros; ++j)
            if (macroLearnBtns[j].findColour (juce::TextButton::buttonColourId) != Colors::surfaceRaised)
            {
                macroLearnBtns[j].setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
                macroLearnBtns[j].setColour (juce::TextButton::textColourOffId, Colors::textDim);
            }
        refreshMacroLabels();
    }

    // Auto-hide scrollbar logic
    auto& vsb = chainViewport.getVerticalScrollBar();
    auto& hsb = chainViewport.getHorizontalScrollBar();
    bool isMouseOver = chainViewport.isMouseOver (true);
    bool isScrolling = vsb.isMouseButtonDown (true) || hsb.isMouseButtonDown (true);

    if (isMouseOver || isScrolling) {
        scrollIdleCounter = 0;
        scrollAlpha = std::min (1.0f, scrollAlpha + 0.15f);
    } else {
        scrollIdleCounter++;
        if (scrollIdleCounter > kScrollHideTicks)
            scrollAlpha = std::max (0.0f, scrollAlpha - 0.1f);
    }
    vsb.setAlpha (scrollAlpha);
    hsb.setAlpha (scrollAlpha);

    lfoPanel.updatePhase();

    // Sync macro knob values and blue halo from LFO modulation
    auto& lfoEngine = proc.getLfoEngine();
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        macroKnobs[i].setValue (proc.getParameters()[i]->getValue(), false);

        // Skip mod depth update while user is dragging the halo
        if (macroKnobs[i].isDraggingHalo()) continue;

        // Compute mod depth: sum of all LFOs targeting this macro
        float totalMod = 0.0f;
        for (int li = 0; li < LfoEngine::numLfos; ++li)
        {
            auto& lfo = lfoEngine.getLfo (li);
            if (! lfo.enabled) continue;
            for (auto& t : lfo.targets)
                if (t.type == LfoTarget::Macro && t.macroIndex == i)
                    totalMod += lfo.depth;
        }
        macroKnobs[i].setModDepth (totalMod);
    }
}

void ChainHostEditor::ChainContainer::paint (juce::Graphics& g)
{
    auto& cg = proc.getChainGraph();
    for (int i = 0; i < cg.getNumChains(); ++i) {
        int y = i * 88;
        if (i % 2 == 0) { g.setColour (Colors::surface.withAlpha (0.12f)); g.fillRect (0, y, getWidth(), 86); }

        g.setColour (Colors::chainAccent.withAlpha (0.1f));
        g.fillRect (10, y + 8, 30, 30);
        g.setColour (Colors::chainAccent.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
        g.drawText (juce::String (i + 1), 10, y + 8, 30, 30, juce::Justification::centred);
        g.setColour (Colors::textDim.withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (7.5f)));
        g.drawText ("CHAIN", 10, y + 40, 30, 10, juce::Justification::centred);

        g.setColour (Colors::border.withAlpha (0.3f));
        g.drawLine (0, (float)(y + 88), (float)getWidth(), (float)(y + 88), 1.0f);
    }
}

void ChainHostEditor::ChainContainer::resized()
{
    for (int ci = 0; ci < chainRows.size(); ++ci) {
        auto* row = chainRows[ci];
        int y = ci * 88;
        row->volumeKnob.setBounds (34, y + 4, 64, 80);
        int px = 100;
        for (auto* slot : row->slotComponents) {
            int sw = slot->getDesiredWidth();
            slot->setBounds (px, y + 2, sw, 84);
            px += sw + 6;
        }
        row->addToChainButton.setBounds (px + 4, y + 28, 30, 30);
        row->removeChainButton.setBounds (getWidth() - 30, y + 32, 22, 22);
    }
}

void ChainHostEditor::ChainViewport::mouseWheelMove (const juce::MouseEvent& e,
                                                      const juce::MouseWheelDetails& wheel)
{
    // Shift+scroll or native horizontal trackpad gesture → horizontal scroll
    bool wantsHorizontal = e.mods.isShiftDown()
                        || (std::abs (wheel.deltaX) > std::abs (wheel.deltaY));

    if (wantsHorizontal && getViewedComponent() != nullptr)
    {
        float dx = (std::abs (wheel.deltaX) > std::abs (wheel.deltaY)) ? wheel.deltaX : wheel.deltaY;
        auto pos = getViewPosition();
        int contentW = getViewedComponent()->getWidth();
        int delta = juce::roundToInt (dx * 300.0f);
        pos.x = juce::jlimit (0, juce::jmax (0, contentW - getWidth()), pos.x - delta);
        setViewPosition (pos);
    }
    else
    {
        // Normal vertical scroll — let Viewport handle it
        juce::Viewport::mouseWheelMove (e, wheel);
    }
}

void ChainHostEditor::paint (juce::Graphics& g)
{
    constexpr int kHeaderH = 48;
    constexpr int kBottomH = 440;
    constexpr int kMacroColW = 160;
    constexpr int kTargetsColW = 0;

    g.fillAll (Colors::bg);

    {
        g.setColour (Colors::surface);
        g.fillRect (0, 0, getWidth(), kHeaderH);
        g.setColour (Colors::border);
        g.drawLine (0, (float) kHeaderH, (float) getWidth(), (float) kHeaderH, 1.0f);

        g.setColour (Colors::accent);
        g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
        g.drawText ("ChainHost", 16, 8, 160, 32, juce::Justification::centredLeft);
        g.setColour (Colors::textDim.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("v0.2", 138, 20, 30, 12, juce::Justification::centredLeft);
    }

    int macroTop = getHeight() - kBottomH;
    int macroStripW = kMacroColW;
    int targetsX = getWidth() - kTargetsColW;

    // Macro strip background
    g.setColour (Colors::bgDeep);
    g.fillRect (0, macroTop, macroStripW, getHeight() - macroTop);

    // "MACROS" title centered
    g.setColour (Colors::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.drawText ("MACROS", 0, macroTop + 4, macroStripW, 16, juce::Justification::centred);
    // Underline
    g.setColour (Colors::border.withAlpha (0.5f));
    g.drawLine (8.0f, (float)(macroTop + 22), (float)(macroStripW - 8), (float)(macroTop + 22), 1.0f);

    // Cell backgrounds and borders
    int cellW = macroStripW / 2;
    int cellH = (getHeight() - macroTop - 26) / 4;
    int gridTop = macroTop + 26;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 2; ++col)
        {
            int idx = col * 4 + row;
            int cx = col * cellW;
            int cy = gridTop + row * cellH;
            // Cell bg
            g.setColour (Colors::surface.withAlpha (0.15f));
            g.fillRect (cx, cy, cellW, cellH);
            // Cell border
            g.setColour (Colors::border.withAlpha (0.3f));
            g.drawRect (cx, cy, cellW, cellH, 1);
            // Selected macro highlight
            if (idx == selectedMacro) {
                g.setColour (Colors::accent.withAlpha (0.45f));
                g.drawRect (cx, cy, cellW, cellH, 1);
            }
            // Macro number — top-left
            g.setColour (Colors::textDim.withAlpha (0.6f));
            g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            g.drawText (juce::String (idx + 1), cx + 4, cy + 2, 14, 12, juce::Justification::centredLeft);

            // Macro name glow
            {
                auto& mappings2 = proc.getMacroManager().getMappings (idx);
                auto glowCol = mappings2.empty() ? Colors::textDim : Colors::accent;
                g.setFont (juce::Font (juce::FontOptions (8.0f)));
                for (int pass = 3; pass >= 1; --pass)
                {
                    float spread = (float) pass * 1.5f;
                    g.setColour (glowCol.withAlpha (0.12f / (float) pass));
                    g.drawText (macroNames[idx],
                                cx + 2 - (int) spread, cy + cellH - 14 - (int) spread,
                                cellW - 4 + (int)(spread * 2), 12 + (int)(spread * 2),
                                juce::Justification::centred);
                }
            }

            // Link count — centered on the same control row as drag/link buttons
            auto& mappings = proc.getMacroManager().getMappings (idx);
            if (! mappings.empty()) {
                g.setColour (Colors::accent.withAlpha (0.7f));
                g.setFont (juce::Font (juce::FontOptions (8.0f)));
                constexpr int controlRowY = 26;
                constexpr int leftControlW = 26;
                constexpr int rightControlW = 26;
                g.drawText (juce::String ((int)mappings.size()) + " link" + (mappings.size() > 1 ? "s" : ""),
                            cx + leftControlW, cy + cellH - controlRowY, cellW - leftControlW - rightControlW, 12, juce::Justification::centred);
            }
        }

    // Top border
    g.setColour (Colors::border);
    g.drawLine (0, (float)macroTop, (float)getWidth(), (float)macroTop, 1.0f);
    // Right border of macro strip
    g.setColour (Colors::border.withAlpha (0.4f));
    g.drawLine ((float)macroStripW, (float)macroTop, (float)macroStripW, (float)getHeight(), 1.0f);
}

void ChainHostEditor::resized()
{
    constexpr int kHeaderH = 48;
    constexpr int kBottomH = 440;
    constexpr int kMacroColW = 160;
    constexpr int kTargetsColW = 0;

    int x = 180;
    scanButton.setBounds (x, 12, 52, 24); x += 58;
    addPluginButton.setBounds (x, 12, 80, 24); x += 86;
    addChainButton.setBounds (x, 12, 68, 24);
    presetsToggle.setBounds (getWidth() - 82, 12, 70, 24);

    int chainTop = kHeaderH;
    if (scanProgressPanel.isVisible())
    {
        scanProgressPanel.setBounds (0, chainTop, getWidth(), ScanProgressPanel::kPanelHeight);
        chainTop += ScanProgressPanel::kPanelHeight;
    }
    if (presetBrowserOpen) { presetBrowser.setBounds (0, chainTop, getWidth(), 140); chainTop += 140; }

    int macroTop = getHeight() - kBottomH;
    int targetsX = getWidth() - kTargetsColW;
    chainViewport.setBounds (0, chainTop, getWidth(), macroTop - chainTop);

    auto& cg = proc.getChainGraph();
    int maxW = getWidth();
    for (int ci = 0; ci < (int) chainContainer.chainRows.size(); ++ci)
    {
        auto* row = chainContainer.chainRows[ci];
        int rowW = 100; // left margin (chain number + volume knob)
        for (auto* slot : row->slotComponents)
            rowW += slot->getDesiredWidth() + 6;
        rowW += 40 + 40; // add btn + remove btn margin
        if (rowW > maxW) maxW = rowW;
    }
    chainContainer.setSize (maxW, cg.getNumChains() * 88);

    int macroStripW = kMacroColW;
    int cellW = macroStripW / 2;
    int cellH = (getHeight() - macroTop - 26) / 4;
    int gridTop = macroTop + 26;
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        int col = i / 4;
        int row2 = i % 4;
        int cx = col * cellW;
        int cy = gridTop + row2 * cellH;
        // LEARN pill above knob
        macroLearnBtns[i].setBounds (cx + 6, cy + 2, cellW - 12, 14);
        // Knob centered with halo space — smaller to fit cell
        int knobSz = juce::jmin (cellW - 24, cellH - 48);
        macroKnobs[i].setBounds (cx + (cellW - knobSz) / 2, cy + 18, knobSz, knobSz + 8);
        // Label at bottom
        macroLabels[i].setBounds (cx + 2, cy + cellH - 14, cellW - 4, 12);
        // Drag handle + link at bottom corners
        macroDragHandles[i]->setBounds (cx + 4, cy + cellH - 28, 20, 16);
        macroLinkBtns[i].setBounds (cx + cellW - 24, cy + cellH - 28, 20, 16);
    }

    int panelLeft = macroStripW + 4;
    int panelTop = macroTop + 4;
    lfoPanel.setBounds (panelLeft, panelTop, getWidth() - panelLeft, getHeight() - panelTop);
}

void ChainHostEditor::refreshChainView()
{
    chainContainer.chainRows.clear();
    auto& cg = proc.getChainGraph();
    for (int ci = 0; ci < cg.getNumChains(); ++ci) {
        auto* row = chainContainer.chainRows.add (new ChainViewRow());
        auto& chain = cg.getChain (ci);
        row->volumeKnob.setValue (chain.volume, false); row->volumeKnob.setDefaultValue (1.0f);
        row->volumeKnob.onValueChange = [this, ci, row]() { proc.getChainGraph().setChainVolume (proc.getGraph(), ci, row->volumeKnob.getValue()); };
        chainContainer.addAndMakeVisible (row->volumeKnob);

        row->removeChainButton.setColour (juce::TextButton::buttonColourId, Colors::remove.withAlpha (0.3f));
        row->removeChainButton.setColour (juce::TextButton::textColourOffId, Colors::text);
        row->removeChainButton.onClick = [this, ci]() {
            for (auto& s : proc.getChainGraph().getChain (ci).slots) {
                closePluginWindow (s.nodeId);
                proc.getMacroManager().removeMappingsForUid (s.uid);
                proc.getLfoEngine().removeTargetsForNode (s.nodeId);
            }
            proc.getChainGraph().removeParallelChain (proc.getGraph(), ci);
            refreshChainView(); mappingPanel.refresh(); lfoPanel.refresh(); refreshMacroLabels();
        };
        if (cg.getNumChains() > 1) chainContainer.addAndMakeVisible (row->removeChainButton);

        row->addToChainButton.setColour (juce::TextButton::buttonColourId, Colors::accent.withAlpha (0.3f));
        row->addToChainButton.setColour (juce::TextButton::textColourOffId, Colors::accent);
        row->addToChainButton.onClick = [this, ci]() { showPluginMenu (ci); };
        chainContainer.addAndMakeVisible (row->addToChainButton);

        for (int si = 0; si < (int)chain.slots.size(); ++si) {
            auto* comp = row->slotComponents.add (new PluginSlotComponent (proc, chain.slots[(size_t)si].nodeId, ci, si));
            comp->onOpenEditor = [this] (auto nid) { openPluginWindow (nid); };
            comp->onRemove = [this] (auto nid) {
                auto uid = proc.getChainGraph().getUidForNodeId (nid);
                closePluginWindow (nid); 
                if (uid.isNotEmpty()) proc.getMacroManager().removeMappingsForUid (uid);
                proc.getLfoEngine().removeTargetsForNode (nid); proc.getChainGraph().removePlugin (proc.getGraph(), nid);
                refreshChainView(); mappingPanel.refresh(); lfoPanel.refresh(); refreshMacroLabels();
            };
            comp->onMove = [this] (int fc, int fs, int tc, int ts) { proc.getChainGraph().movePlugin (proc.getGraph(), fc, fs, tc, ts); refreshChainView(); };
            comp->onCopy = [this] (int fc, int fs, int tc, int ts) { proc.getChainGraph().duplicatePlugin (proc.getGraph(), proc.getScanner(), fc, fs, tc, ts); refreshChainView(); };
            comp->onLayoutChanged = [this]() { resized(); chainContainer.resized(); repaint(); };
            chainContainer.addAndMakeVisible (comp);
        }
    }
    resized();
    chainContainer.resized();
    repaint();
}

void ChainHostEditor::refreshMacroLabels()
{
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        if (! macroLabels[i].isBeingEdited())
            macroLabels[i].setText (macroNames[i], juce::dontSendNotification);
        auto& mappings = proc.getMacroManager().getMappings (i);
        macroLabels[i].setColour (juce::Label::textColourId,
            mappings.empty() ? Colors::textDim : Colors::accent);
    }
    repaint();  // for link count in paint()
}

void ChainHostEditor::mouseDown (const juce::MouseEvent& e)
{
    // Left-click on macro label or drag handle selects that macro
    if (e.mods.isLeftButtonDown() && !e.mods.isPopupMenu())
    {
        for (int i = 0; i < MacroManager::numMacros; ++i)
            if (e.eventComponent == &macroLabels[i] || e.eventComponent == macroDragHandles[i])
            {
                selectMacro (i);
                refreshMacroLabels();
                return;
            }
    }

    if (e.mods.isPopupMenu())
    {
        for (int i = 0; i < MacroManager::numMacros; ++i)
        {
            if (e.eventComponent == &macroLabels[i] || e.eventComponent == macroDragHandles[i])
            {
                selectMacro (i);
                auto& mappings = proc.getMacroManager().getMappings (i);

                juce::PopupMenu menu;
                menu.addItem (2000, "Rename...");
                menu.addSeparator();

                if (!mappings.empty())
                {
                    menu.addSectionHeader ("Mappings (" + juce::String ((int)mappings.size()) + ")");
                    int id = 1;
                    for (auto& m : mappings) {
                        juce::String itemName = "?";
                        if (m.slotUid == InternalParams::uid) {
                            itemName = "ChainHost > " + InternalParams::paramName (m.paramIndex);
                        } else {
                            auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
                            if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
                                auto& params = node->getProcessor()->getParameters();
                                if (m.paramIndex < params.size())
                                    itemName = node->getProcessor()->getName() + " > " + params[m.paramIndex]->getName (30);
                            }
                        }
                        menu.addItem (id++, "Remove: " + itemName);
                    }
                    menu.addSeparator();
                    menu.addItem (1000, "Clear All Mappings");
                }

                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (macroLabels[i]),
                    [this, i, mappings] (int result) {
                        if (result == 2000) {
                            showMacroRenameEditor (i);
                        } else if (result == 1000) {
                            proc.getMacroManager().clearMappings (i);
                            refreshMacroLabels();
                        } else if (result > 0 && result <= (int) mappings.size()) {
                            auto& m = mappings[(size_t) result - 1];
                            proc.getMacroManager().removeMapping (i, m.slotUid, m.paramIndex);
                            refreshMacroLabels();
                        }
                    });
                return;
            }
        }
    }
}

void ChainHostEditor::showPluginMenu (int chainIndex)
{
    auto types = proc.getScanner().getPluginList().getTypes();
    if (types.isEmpty()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "No Plugins", "Click SCAN first."); return; }
    juce::PopupMenu menu; std::map<juce::String, juce::PopupMenu> subs; int id = 1;
    for (auto& d : types) { subs[d.manufacturerName.isEmpty() ? "Unknown" : d.manufacturerName].addItem (id++, d.name); }
    for (auto& [mfr, sub] : subs) menu.addSubMenu (mfr, sub);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addPluginButton),
        [this, types, chainIndex] (int r) { if (r > 0 && r <= types.size()) {
            proc.getChainGraph().addPlugin (proc.getGraph(), proc.getScanner(), types[r-1], chainIndex);
            refreshChainView(); mappingPanel.refresh(); lfoPanel.refresh(); }});
}

void ChainHostEditor::openPluginWindow (juce::AudioProcessorGraph::NodeID nodeId)
{
    cleanupClosedWindows();
    auto now = juce::Time::getMillisecondCounter();
    auto it = windowCloseTimestamps.find (nodeId.uid);
    if (it != windowCloseTimestamps.end()) { if ((now - it->second) < kReopenCooldownMs) return; windowCloseTimestamps.erase (it); }
    for (int i = pluginWindows.size(); --i >= 0;)
        if (pluginWindows[i]->getNodeId() == nodeId) { pluginWindows[i]->clearContentComponent(); pluginWindows.remove (i); windowCloseTimestamps.erase (nodeId.uid); return; }
    auto* node = proc.getGraph().getNodeForId (nodeId);
    if (!node) return;
    auto* plugProc = node->getProcessor();
    if (!plugProc->hasEditor()) return;
    if (auto* editor = plugProc->createEditor())
        pluginWindows.add (new PluginWindow (plugProc->getName(), nodeId, editor,
            getTopLevelComponent(),
            [this] (auto nid) {
                windowCloseTimestamps[nid.uid] = juce::Time::getMillisecondCounter();
            }));
}

void ChainHostEditor::closePluginWindow (juce::AudioProcessorGraph::NodeID nodeId) {
    for (int i = pluginWindows.size(); --i >= 0;)
        if (pluginWindows[i]->getNodeId() == nodeId) { pluginWindows[i]->clearContentComponent(); pluginWindows.remove (i); return; }
}

void ChainHostEditor::cleanupClosedWindows() {
    for (int i = pluginWindows.size(); --i >= 0;) if (!pluginWindows[i]->isVisible()) pluginWindows.remove (i);
    auto now = juce::Time::getMillisecondCounter();
    for (auto it = windowCloseTimestamps.begin(); it != windowCloseTimestamps.end();)
        (now - it->second) > kReopenCooldownMs * 2 ? it = windowCloseTimestamps.erase (it) : ++it;
}


bool ChainHostEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".vst3"))
            return true;
    return false;
}

void ChainHostEditor::filesDropped (const juce::StringArray& files, int /*x*/, int y)
{
    int chainTop = 48 + (presetBrowserOpen ? 140 : 0);
    int chainIndex = juce::jmax (0, (y - chainTop) / 88);
    if (chainIndex >= proc.getChainGraph().getNumChains())
        chainIndex = juce::jmax (0, proc.getChainGraph().getNumChains() - 1);

    for (auto& filePath : files)
    {
        if (! filePath.endsWithIgnoreCase (".vst3"))
            continue;

        juce::OwnedArray<juce::PluginDescription> results;
        auto& fm = proc.getScanner().getFormatManager();
        for (int fi = 0; fi < fm.getNumFormats(); ++fi)
            if (fm.getFormat (fi)->getName() == "VST3")
            {
                proc.getScanner().getPluginList().scanAndAddFile (filePath, true, results, *fm.getFormat (fi));
                break;
            }

        if (results.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                "Load Failed", "Could not load plugin:\n" + filePath);
            continue;
        }

        if (results.size() == 1)
        {
            proc.getChainGraph().addPlugin (proc.getGraph(), proc.getScanner(),
                                            *results[0], chainIndex);
        }
        else
        {
            auto descs = std::make_shared<std::vector<juce::PluginDescription>>();
            for (auto* d : results) descs->push_back (*d);
            juce::PopupMenu menu;
            for (int i = 0; i < (int) descs->size(); ++i)
                menu.addItem (i + 1, (*descs)[static_cast<size_t> (i)].name);
            menu.showMenuAsync ({}, [this, descs, chainIndex] (int r)
            {
                if (r > 0)
                    proc.getChainGraph().addPlugin (proc.getGraph(), proc.getScanner(),
                                                    (*descs)[static_cast<size_t> (r - 1)], chainIndex);
                refreshChainView();
            });
            continue;
        }
    }
    refreshChainView();
}

//==============================================================================
// MacroDragHandle
//==============================================================================

void ChainHostEditor::MacroDragHandle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1);
    g.setColour (selected ? Colors::accent.withAlpha (0.5f) : Colors::surfaceRaised.withAlpha (0.6f));
    g.fillRoundedRectangle (b, 3.0f);

    // Draw 4-way arrows (up/down/left/right from center)
    g.setColour (selected ? Colors::text : Colors::textDim);
    float cx = b.getCentreX(), cy = b.getCentreY();
    float armLen = juce::jmin (b.getWidth(), b.getHeight()) * 0.28f;
    float headSz = armLen * 0.45f;
    float thick = 1.2f;

    // Lines from center
    g.drawLine (cx, cy - armLen, cx, cy + armLen, thick);   // vertical
    g.drawLine (cx - armLen, cy, cx + armLen, cy, thick);   // horizontal

    // Up arrow head
    g.drawLine (cx, cy - armLen, cx - headSz, cy - armLen + headSz, thick);
    g.drawLine (cx, cy - armLen, cx + headSz, cy - armLen + headSz, thick);
    // Down arrow head
    g.drawLine (cx, cy + armLen, cx - headSz, cy + armLen - headSz, thick);
    g.drawLine (cx, cy + armLen, cx + headSz, cy + armLen - headSz, thick);
    // Left arrow head
    g.drawLine (cx - armLen, cy, cx - armLen + headSz, cy - headSz, thick);
    g.drawLine (cx - armLen, cy, cx - armLen + headSz, cy + headSz, thick);
    // Right arrow head
    g.drawLine (cx + armLen, cy, cx + armLen - headSz, cy - headSz, thick);
    g.drawLine (cx + armLen, cy, cx + armLen - headSz, cy + headSz, thick);
}

void ChainHostEditor::MacroDragHandle::mouseDown (const juce::MouseEvent&)
{
    // Select this macro
    if (auto* editor = findParentComponentOfClass<ChainHostEditor>())
        editor->selectMacro (macroIndex);
}

void ChainHostEditor::MacroDragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 4)
        if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this))
            dc->startDragging ("macro:" + juce::String (macroIndex), this);
}

void ChainHostEditor::selectMacro (int i)
{
    selectedMacro = juce::jlimit (0, MacroManager::numMacros - 1, i);
    for (int j = 0; j < MacroManager::numMacros; ++j)
        macroDragHandles[j]->selected = (j == selectedMacro);
    refreshMacroLabels();
    repaint();
}

void ChainHostEditor::showMacroLinkMenu (int macroIdx)
{
    juce::PopupMenu menu;
    auto& cg = proc.getChainGraph();
    int itemId = 1;
    struct PI { juce::String uid; int p; };
    std::vector<PI> lookup;

    for (int ci = 0; ci < cg.getNumChains(); ++ci)
        for (auto& slot : cg.getChain (ci).slots)
            if (auto* node = proc.getGraph().getNodeForId (slot.nodeId))
            {
                auto& params = node->getProcessor()->getParameters();
                if (params.isEmpty()) continue;
                juce::PopupMenu sub;
                for (int pi = 0; pi < params.size(); ++pi)
                {
                    lookup.push_back ({ slot.uid, pi });
                    sub.addItem (itemId++, params[pi]->getName (40));
                }
                auto lbl = node->getProcessor()->getName();
                if (cg.getNumChains() > 1)
                    lbl = "Chain " + juce::String (ci + 1) + " > " + lbl;
                menu.addSubMenu (lbl, sub);
            }

    if (itemId == 1)
    {
        menu.addItem (-1, "(No plugins loaded)", false, false);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (macroLinkBtns[macroIdx]),
        [this, macroIdx, lookup] (int r)
        {
            if (r > 0 && r <= (int) lookup.size())
            {
                proc.getMacroManager().addMapping (macroIdx,
                    lookup[(size_t)(r - 1)].uid, lookup[(size_t)(r - 1)].p, 0.0f, 1.0f);
                refreshMacroLabels();
            }
        });
}

//==============================================================================
// MacroLinkButton — draws chain link icon
//==============================================================================

void ChainHostEditor::MacroLinkButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1);
    g.setColour (Colors::surfaceRaised.withAlpha (0.6f));
    g.fillRoundedRectangle (b, 3.0f);

    // Draw chain link icon: two interlocking ovals
    g.setColour (Colors::textDim);
    float cx = b.getCentreX(), cy = b.getCentreY();
    float linkW = b.getWidth() * 0.3f, linkH = b.getHeight() * 0.22f;
    float gap = linkW * 0.25f;
    // Left link
    g.drawRoundedRectangle (cx - linkW - gap * 0.5f, cy - linkH, linkW * 1.4f, linkH * 2.0f, linkH, 1.2f);
    // Right link
    g.drawRoundedRectangle (cx + gap * 0.5f - linkW * 0.4f, cy - linkH, linkW * 1.4f, linkH * 2.0f, linkH, 1.2f);
}

//==============================================================================
// showMacroRenameEditor
//==============================================================================

void ChainHostEditor::showMacroRenameEditor (int macroIdx)
{
    macroLabels[macroIdx].showEditor();
}

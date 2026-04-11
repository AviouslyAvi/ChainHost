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
    styleBtn (tabMappings, Colors::accent.withAlpha (0.45f));
    styleBtn (tabLfo, Colors::surfaceRaised);

    scanButton.onClick = [this]() {
        scanButton.setButtonText ("..."); scanButton.setEnabled (false);
        proc.getScanner().onScanComplete = [this]() { scanButton.setButtonText ("SCAN"); scanButton.setEnabled (true); };
        proc.getScanner().scan();
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
    tabMappings.onClick = [this]() { setActiveTab (0); };
    tabLfo.onClick = [this]() { setActiveTab (1); };

    for (auto* btn : { &scanButton, &addPluginButton, &addChainButton, &presetsToggle, &tabMappings, &tabLfo })
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

    juce::Colour macroColours[] = {
        Colors::accent, Colors::accentBright, juce::Colour (0xffcc6644), juce::Colour (0xffdd8855),
        juce::Colour (0xffbb7744), juce::Colour (0xffcc9955), juce::Colour (0xffddaa66), juce::Colour (0xffeebb77)
    };
    for (int i = 0; i < MacroManager::numMacros; ++i)
    {
        macroKnobs[i].setArcColour (macroColours[i]);
        macroKnobs[i].setDefaultValue (0.0f);
        macroKnobs[i].onValueChange = [this, i]() { proc.getParameters()[i]->setValueNotifyingHost (macroKnobs[i].getValue()); };
        addAndMakeVisible (macroKnobs[i]);

        macroLabels[i].setText ("MACRO " + juce::String (i + 1), juce::dontSendNotification);
        macroLabels[i].setJustificationType (juce::Justification::centred);
        macroLabels[i].setColour (juce::Label::textColourId, Colors::textDim);
        macroLabels[i].setFont (juce::Font (juce::FontOptions (8.5f)));
        macroLabels[i].setInterceptsMouseClicks (true, false);
        macroLabels[i].addMouseListener (this, false);
        addAndMakeVisible (macroLabels[i]);

        learnButtons[i].setButtonText ("LEARN");
        learnButtons[i].setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
        learnButtons[i].setColour (juce::TextButton::textColourOffId, Colors::textDim);
        learnButtons[i].onClick = [this, i]() {
            if (mappingPanel.isLearning() && mappingPanel.getLearningMacro() == i) mappingPanel.stopLearn();
            else mappingPanel.startLearn (i);
            for (int j = 0; j < MacroManager::numMacros; ++j) {
                bool learning = mappingPanel.isLearning() && mappingPanel.getLearningMacro() == j;
                learnButtons[j].setColour (juce::TextButton::buttonColourId, learning ? Colors::learn.withAlpha (0.5f) : Colors::surfaceRaised);
                learnButtons[j].setColour (juce::TextButton::textColourOffId, learning ? Colors::learn : Colors::textDim);
            }
        };
        addAndMakeVisible (learnButtons[i]);
    }

    mappingPanel.onMappingChanged = [this]() { refreshMacroLabels(); };
    addAndMakeVisible (mappingPanel);
    lfoPanel.setVisible (false);
    addAndMakeVisible (lfoPanel);

    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (true, false, true, false);
    addAndMakeVisible (chainViewport);

    refreshChainView(); refreshMacroLabels();
    setSize (940, 740);
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
        for (int i = 0; i < MacroManager::numMacros; ++i)
            if (learnButtons[i].findColour (juce::TextButton::buttonColourId) != Colors::surfaceRaised) {
                learnButtons[i].setColour (juce::TextButton::buttonColourId, Colors::surfaceRaised);
                learnButtons[i].setColour (juce::TextButton::textColourOffId, Colors::textDim);
                refreshMacroLabels();
            }

    // Auto-hide scrollbar logic
    auto& vsb = chainViewport.getVerticalScrollBar();
    bool isMouseOver = chainViewport.isMouseOver (true);
    bool isScrolling = vsb.isMouseButtonDown (true);

    if (isMouseOver || isScrolling) {
        scrollIdleCounter = 0;
        scrollAlpha = std::min (1.0f, scrollAlpha + 0.15f);
    } else {
        scrollIdleCounter++;
        if (scrollIdleCounter > kScrollHideTicks)
            scrollAlpha = std::max (0.0f, scrollAlpha - 0.1f);
    }
    vsb.setAlpha (scrollAlpha);

    if (activeTab == 1) lfoPanel.updatePhase();
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
        row->volumeKnob.setBounds (44, y + 4, 50, 68);
        int px = 100;
        for (auto* slot : row->slotComponents) { slot->setBounds (px, y + 2, 152, 84); px += 158; }
        row->addToChainButton.setBounds (px + 4, y + 28, 30, 30);
        row->removeChainButton.setBounds (getWidth() - 30, y + 32, 22, 22);
    }
}

void ChainHostEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bg);

    {
        g.setColour (Colors::surface);
        g.fillRect (0, 0, getWidth(), 48);
        g.setColour (Colors::border);
        g.drawLine (0, 48, (float)getWidth(), 48, 1.0f);

        g.setColour (Colors::accent);
        g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
        g.drawText ("ChainHost", 16, 8, 160, 32, juce::Justification::centredLeft);
        g.setColour (Colors::textDim.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("v0.2", 138, 20, 30, 12, juce::Justification::centredLeft);
    }

    int macroTop = getHeight() - 230;
    int macroStripW = 160;
    g.setColour (Colors::border);
    g.drawLine (0, (float)macroTop, (float)getWidth(), (float)macroTop, 1.0f);
    g.setColour (Colors::surface.withAlpha (0.6f));
    g.fillRect (0, macroTop + 1, macroStripW, getHeight() - macroTop - 1);
    g.setColour (Colors::textDim.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("MACROS", 8, macroTop + 4, 60, 12, juce::Justification::centredLeft);
    g.setColour (Colors::border.withAlpha (0.4f));
    g.drawLine ((float)macroStripW, (float)macroTop, (float)macroStripW, (float)getHeight(), 1.0f);
}

void ChainHostEditor::resized()
{
    int x = 180;
    scanButton.setBounds (x, 12, 52, 24); x += 58;
    addPluginButton.setBounds (x, 12, 80, 24); x += 86;
    addChainButton.setBounds (x, 12, 68, 24);
    presetsToggle.setBounds (getWidth() - 82, 12, 70, 24);

    int chainTop = 48;
    if (presetBrowserOpen) { presetBrowser.setBounds (0, 48, getWidth(), 140); chainTop = 188; }

    int macroTop = getHeight() - 230;
    chainViewport.setBounds (0, chainTop, getWidth(), macroTop - chainTop);
    
    auto& cg = proc.getChainGraph();
    chainContainer.setSize (getWidth(), cg.getNumChains() * 88);

    int macroStripW = 160;
    int macroColW = 74;
    int macroRowH = 54;
    int macroStartY = macroTop + 14;
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        int col = i / 4;
        int row2 = i % 4;
        int mx = 6 + col * macroColW;
        int my = macroStartY + row2 * macroRowH;
        macroLabels[i].setBounds (mx, my, macroColW, 10);
        macroKnobs[i].setBounds (mx + 2, my + 10, macroColW - 4, 32);
        learnButtons[i].setBounds (mx + 8, my + 44, macroColW - 16, 14);
    }

    int panelLeft = macroStripW + 4;
    tabMappings.setBounds (panelLeft, macroTop + 4, 76, 18);
    tabLfo.setBounds (panelLeft + 82, macroTop + 4, 44, 18);
    int panelTop = macroTop + 26;
    mappingPanel.setBounds (panelLeft, panelTop, getWidth() - panelLeft, getHeight() - panelTop);
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
            chainContainer.addAndMakeVisible (comp);
        }
    }
    resized(); repaint();
}

void ChainHostEditor::refreshMacroLabels()
{
    for (int i = 0; i < MacroManager::numMacros; ++i) {
        auto& mappings = proc.getMacroManager().getMappings (i);
        if (mappings.empty()) {
            macroLabels[i].setText ("MACRO " + juce::String (i + 1), juce::dontSendNotification);
            macroLabels[i].setColour (juce::Label::textColourId, Colors::textDim);
        } else {
            juce::String name;
            for (auto& m : mappings) {
                auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
                if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
                    auto& params = node->getProcessor()->getParameters();
                    if (m.paramIndex < params.size()) {
                        if (name.isNotEmpty()) name += ", ";
                        name += params[m.paramIndex]->getName (20);
                    }
                }
            }
            if (name.isEmpty()) name = "?";
            if (name.length() > 28) name = name.substring (0, 25) + "...";
            macroLabels[i].setText (name, juce::dontSendNotification);
            macroLabels[i].setColour (juce::Label::textColourId, Colors::accent);
        }
    }
}

void ChainHostEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        for (int i = 0; i < MacroManager::numMacros; ++i)
        {
            if (e.eventComponent == &macroLabels[i])
            {
                auto& mappings = proc.getMacroManager().getMappings (i);
                if (mappings.empty()) return;

                juce::PopupMenu menu;
                menu.addSectionHeader ("Macro " + juce::String (i + 1) + " Mappings");
                int id = 1;
                for (auto& m : mappings) {
                    juce::String itemName = "?";
                    auto nodeId = proc.getChainGraph().getNodeIdForUid (m.slotUid);
                    if (auto* node = proc.getGraph().getNodeForId (nodeId)) {
                        auto& params = node->getProcessor()->getParameters();
                        if (m.paramIndex < params.size())
                            itemName = node->getProcessor()->getName() + " > " + params[m.paramIndex]->getName (30);
                    }
                    menu.addItem (id++, "Remove: " + itemName);
                }
                menu.addSeparator();
                menu.addItem (1000, "Clear All Mappings");

                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (macroLabels[i]),
                    [this, i, mappings] (int result) {
                        if (result == 1000) {
                            proc.getMacroManager().clearMappings (i);
                        } else if (result > 0 && result <= (int) mappings.size()) {
                            auto& m = mappings[(size_t) result - 1];
                            proc.getMacroManager().removeMapping (i, m.slotUid, m.paramIndex);
                        }
                        refreshMacroLabels();
                        mappingPanel.refresh();
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
    for (auto* win : pluginWindows) if (win->getNodeId() == nodeId) { win->toFront (true); return; }
    auto* node = proc.getGraph().getNodeForId (nodeId);
    if (!node) return;
    auto* plugProc = node->getProcessor();
    if (!plugProc->hasEditor()) return;
    if (auto* editor = plugProc->createEditor())
        pluginWindows.add (new PluginWindow (plugProc->getName(), nodeId, editor,
            getTopLevelComponent(),
            [this] (auto nid) { windowCloseTimestamps[nid.uid] = juce::Time::getMillisecondCounter(); }));
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

void ChainHostEditor::setActiveTab (int tab) {
    activeTab = tab;
    mappingPanel.setVisible (tab == 0); lfoPanel.setVisible (tab == 1);
    tabMappings.setColour (juce::TextButton::buttonColourId, tab == 0 ? Colors::accent.withAlpha (0.45f) : Colors::surfaceRaised);
    tabLfo.setColour (juce::TextButton::buttonColourId, tab == 1 ? Colors::lfoBlue.withAlpha (0.45f) : Colors::surfaceRaised);
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
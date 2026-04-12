# ChainHost Quick-Start Tutorial

Welcome to ChainHost — a VST3 plugin host with Serum-style LFO modulation, macro controls, and multi-chain signal routing.

---

## 1. The Chain Section (Top Area)

The chain area is where you load and route your plugins.

### Loading Plugins

- Click **+ PLUGIN** in the header to add a plugin to the currently selected chain.
- You can also **drag a .vst3 file** directly from Finder into ChainHost to load it.
- Click **+ CHAIN** to add a parallel signal chain.

### Chain Rows

Each chain row shows:
- **Chain number** (left side) — identifies the chain
- **Volume knob** — controls the chain's output level
- **Plugin slots** — click a slot to open the plugin's editor window
- **+ button** — add another plugin to the end of this chain
- **x button** — remove this chain

### Plugin Windows

When you click a plugin slot, its editor opens in a floating window that stays above ChainHost but not above other apps (like FL Studio's Patcher behavior).

---

## 2. The Macro Section (Bottom-Left)

Macros let you control multiple plugin parameters with a single knob.

### Macro Knobs

- **Turn the knob** to change the macro value — all mapped parameters move together.
- **Double-click the label** below a knob to rename it.
- **Right-click a knob** to see options (remove LFO modulation, remove mappings).

### Assigning Parameters to a Macro

There are three ways to map a parameter:

1. **Drag handle** (the crosshair icon) — drag it onto a plugin parameter in an open editor window.
2. **Link button** (chain-link icon) — click to browse a list of all available plugin parameters and select one.
3. **LEARN button** — click it (turns orange), then move a parameter in any loaded plugin. ChainHost detects the change and creates the mapping automatically.

### The Blue Halo Ring

When an LFO is modulating a macro knob, a blue arc appears around it showing the modulation range. You can **drag the blue halo** up/down to resize the modulation depth in real time.

---

## 3. The LFO Section (Bottom-Right)

ChainHost has 4 independent LFOs that can modulate any plugin parameter or macro knob.

### LFO Tabs

Click **LFO 1** through **LFO 4** to switch between them. Each has its own waveform, rate, targets, and settings.

### Enable / Shape

- **ON/OFF** — enables or disables this LFO.
- **Shape dropdown** — choose a preset shape (Sine, Triangle, Saw, Square, etc.). This resets the waveform editor to that shape.

### Control Row

| Button | What it does |
|--------|-------------|
| **FREE / SYNC** | Toggle between free-running Hz rate or tempo-synced note divisions |
| **TRIG** | Retrigger — restarts the LFO phase on each new MIDI note |
| **UNI / BI** | Unipolar (0 to 1) or Bipolar (-1 to 1) output range |
| **ENV** | Envelope mode — LFO plays once and stops (one-shot, great for plucks) |
| **FWD / REV / P.P.** | Playback direction: Forward, Reverse, or Ping-Pong |

### The Waveform Editor

This is a breakpoint editor like Serum's. You can draw custom LFO shapes:

| Tool | What it does |
|------|-------------|
| **PTR** (Pointer) | Select and move breakpoints. Drag the curve handles between points to adjust tension. |
| **---** (Flat) | Click to insert a flat (horizontal) segment |
| **arrow-up** | Click to insert a rising ramp segment |
| **arrow-down** | Click to insert a falling ramp segment |
| **erase** | Click a breakpoint to delete it, or drag across multiple to sweep-delete |

- **Grid snapping**: Use the X and Y dropdowns below the waveform to set grid resolution.
- **Tension**: When hovering or dragging a curve handle, a percentage tooltip shows the current tension value.
- **Playhead**: The white vertical line shows the current LFO phase position in real-time (60fps).

### LFO Knobs

| Knob | What it controls |
|------|-----------------|
| **Rate** | LFO speed in Hz (free mode) or sync division (sync mode) |
| **Depth** | How much the LFO affects its targets (0-100%) |
| **Delay** | Time before the LFO starts after a note trigger (0-5s) |
| **Phase** | Starting phase offset (0-100%) |
| **Rise** | Fade-in time — LFO gradually reaches full depth (0-5s) |
| **Smooth** | Smooths/filters the LFO output to reduce sharp edges |

### Targets Section (Right Column)

This is where you assign what the LFO modulates.

| Control | What it does |
|---------|-------------|
| **LINK** | Opens a menu to pick a plugin parameter or macro as a target |
| **Drag handle** (4-way arrows) | Drag onto a macro knob or plugin parameter to assign this LFO |
| **Link icon** (chain links) | Browse and select a parameter from a categorized list |
| **LEARN** | Click, then move any parameter in a loaded plugin — ChainHost auto-detects and assigns it |

Once targets are assigned, they appear as rows below with an **x** button to remove them.

### LFO + Macros (Bidirectional)

- You can **drag an LFO onto a macro knob** to modulate it. The blue halo ring shows the range.
- You can also **assign a macro to control LFO parameters** (Rate, Depth, etc.) for meta-modulation.

---

## 4. Sync Mode Details

When **SYNC** is active:
- The Rate knob scrolls through note divisions (1/16, 1/8, 1/4, 1/2, 1/1, etc.)
- The knob label shows the current division name
- Select **Hz** from the sync dropdown to switch back to free-running mode

---

## 5. Presets

Click **PRESETS** in the header to open the preset browser. Save and recall your entire chain + macro + LFO configuration.

---

## 6. Tips and Workflow

- **Hover over any button** to see a tooltip explaining what it does.
- **Right-click a macro knob** to quickly remove LFO modulation or macro mappings.
- **Use envelope mode** (ENV) with a custom shape for one-shot volume or filter sweeps.
- **Ping-Pong direction** is great for smooth, bouncing modulation that never has a hard reset.
- **Rise time** prevents click artifacts when an LFO starts modulating a parameter at full depth.
- **Multiple LFOs on one target**: You can assign LFO 1 and LFO 2 to the same parameter — their outputs combine.

---

## Keyboard / Mouse Reference

| Action | How |
|--------|-----|
| Open plugin editor | Click the plugin slot |
| Rename macro | Double-click the macro label |
| Remove LFO from macro | Right-click the macro knob |
| Adjust mod depth | Drag the blue halo ring up/down |
| Delete breakpoint | Switch to erase tool, click the point |
| Reset knob to default | Double-click any knob |

---

Happy modulating!

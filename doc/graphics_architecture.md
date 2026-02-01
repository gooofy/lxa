# Graphics and Intuition Architecture for lxa

This document outlines the architectural approach for implementing `graphics.library` and `intuition.library` support in `lxa`, enabling graphical applications and the Workbench environment.

## 1. High-Level Goals

1.  **Hybrid Display Architecture**: Support both "Integrated" (Hosted/Sandbox) and "Rootless" (Seamless) modes.
2.  **Hardware Abstraction**: Decouple the Amiga API from the underlying Host display system (X11, Wayland, macOS, Windows) using a backend adapter (likely SDL2 initially).
3.  **High-Level Emulation (HLE)**: Emulate `graphics.library` and `intuition.library` logic rather than low-level custom chips (Copper/Blitter/Denise), allowing for resolution independence and better integration.
4.  **RTG-like Behavior**: Treat the display memory as "Retargetable Graphics" (RTG) buffers rather than strictly Chip RAM, though we may maintain Planar<->Chunky conversion for compatibility.

## 2. Display Modes

`lxa` will support two distinct operation modes, selectable at runtime or configuration.

### 2.1. Integrated Mode (Sandbox)
*Analogous to a traditional emulator or virtual machine.*

*   **Concept**: The entire Amiga display environment (one or more Amiga Screens) is contained within one or more Host Windows.
*   **Custom Screens**: Each Amiga "Custom Screen" maps to a dedicated Host Window (or fullscreen).
*   **Public Screens (Workbench)**: The default "Workbench" screen appears as a single Host Window. All Amiga Windows on the Workbench exist virtually inside this Host Window.
*   **Input**: The Host Window captures mouse/keyboard when focused.
*   **Pros**: High compatibility, simple composition, easy to handle "active window" logic within AmigaOS.

### 2.2. Rootless Mode (Seamless)
*Analogous to "Coherence" or "Unity" modes in VM software.*

*   **Concept**: Amiga Windows are mapped 1:1 to Host Windows.
*   **Public Screens**: The "Workbench" screen is virtual/transparent. It does not have a background Host Window. Instead, every Amiga Window opened on Workbench becomes a distinct Host Window managed by the Host Window Manager.
*   **Custom Screens**: Still map to a single Host Window (or Fullscreen), as they represent a dedicated workspace.
*   **Input**: Host events are routed to the specific Amiga Window that received them.
*   **Pros**: Seamless integration with the host desktop. Amiga apps look like native Linux apps.
*   **Challenges**:
    *   **Z-Order**: Syncing Amiga window depth with Host window depth.
    *   **Clipping**: Handling non-rectangular windows (if supported) or global clipping.
    *   **Global Elements**: Screen title bars, menus, and requesters need special handling (e.g., a "Dock" or "Menu Bar" window).

## 3. Architecture Components

### 3.1. Host Backend (`host/display`)
A new subsystem in the host process (`src/lxa`) responsible for managing native windows.

*   **API**: Abstract interface (`LxaDisplay`)
    *   `CreateSurface(width, height, format)`
    *   `UpdateSurface(surface, rect, pixels)`
    *   `CreateWindow(surface, flags)`
    *   `MoveWindow(id, x, y)`
    *   `SetWindowTitle(id, text)`
*   **Implementation**: SDL2 is the primary candidate for cross-platform support.
    *   *Rootless*: Can use `SDL_CreateWindow` for each Amiga window.
    *   *Integrated*: Uses `SDL_CreateWindow` for the Screen, renders Amiga windows to a texture.

### 3.2. Graphics Library (`src/rom/graphics`)
Reimplementation of `graphics.library`.

*   **BitMaps**: Managed as "Chunky" buffers in RAM. `lxa` will favor chunky pixels (8-bit or 32-bit) internally for speed and ease of mapping to Host textures.
*   **RastPorts**: The drawing context.
*   **Drawing Commands**:
    *   `WritePixel`, `Draw`, `RectFill`: Implemented in C (ROM side).
    *   **Optimization**: Complex operations (Blits, AreaFills) can be accelerated via `emucall` to the host, leveraging hardware acceleration (OpenGL/Vulkan via SDL2) if possible, or optimized host-side C routines.
*   **Display Database**: Tracks `View`, `ViewPort`, and `ColorMap` structures.

### 3.3. Intuition Library (`src/rom/intuition`)
Reimplementation of `intuition.library`.

*   **Screen Management**:
    *   On `OpenScreen()`: Calls Host to create a window (if Custom Screen) or associates with the virtual desktop.
*   **Window Management**:
    *   On `OpenWindow()`:
        *   *Integrated*: Creates internal structures, draws into the Screen's BitMap.
        *   *Rootless*: Calls Host to create a new Native Window. The Window's `RastPort` points to a separate BitMap backing this window.
*   **Input Handling (IDCMP)**:
    *   Host receives SDL events (Mouse, Keyboard, WindowClose).
    *   Host maps these to Amiga `InputEvent` structures.
    *   `input.device` (or `intuition` directly via emucall) receives events and distributes them to the active Window's IDCMP port.

## 4. Technical Challenges & Solutions

### 4.1. The "BitMap" Problem
AmigaOS assumes direct access to Chip RAM.
*   **Solution**: We will allocate "Fake Chip RAM" (normal RAM).
*   **Drawing**:
    *   If apps write directly to memory: We must detect this (dirty pages?) or periodically blit the entire buffer to the Host texture (slow but compatible).
    *   If apps use `graphics.library`: We intercept calls (`WritePixel`, `RectFill`) and update the Host surface immediately (or batch updates).

### 4.2. Rootless Menu Bar
Amiga menus are attached to the Screen top bar.
*   **Solution**:
    *   Create a "Menu Bar Window" at the top of the screen or active window.
    *   Or, use a specific "floating" window for the active app's menu.

### 4.3. Rootless Requesters
System requesters (Volume request, Errors) block execution.
*   **Solution**: These must open as "Modal" Host Windows (always on top of their parent).

## 5. Implementation Roadmap (Phased)

### Phase 13: Graphics Foundation
*Goal: Basic drawing capabilities and a single host window.*
*   **13.1**: Implement `graphics.library` stubs and data structures (`BitMap`, `RastPort`, `ViewPort`).
*   **13.2**: Host Display Subsystem (SDL2 integration).
*   **13.3**: `OpenScreen` (creates host window) and `CloseScreen`.
*   **13.4**: Basic drawing (`SetAPen`, `WritePixel`, `RectFill`, `Draw`) rendering to the screen buffer.
*   **13.5**: Blit screen buffer to SDL window (framebuffer update).

### Phase 14: Intuition Basics
*Goal: Windows and basic UI.*
*   **14.1**: `intuition.library` structure. `OpenWindow` (renders to Screen buffer - Integrated mode logic first).
*   **14.2**: Basic Gadgets (system gadgets: close, drag - strictly visual).
*   **14.3**: Input Loop. Map SDL mouse/key events to `input.device`.
*   **14.4**: IDCMP messages (`MOUSEBUTTONS`, `RAWKEY`).

### Phase 15: Advanced Graphics & Compatibility
*Goal: Richer visuals and better performance.*
*   **15.1**: `BltBitMap` (Blitter emulation via CPU).
*   **15.2**: Text rendering (`Text`, `SetFont`).
*   **15.3**: Image support (`DrawImage`, `DrawBorder`).
*   **15.4**: Layers library (basic clipping).

### Phase 16: Rootless Mode
*Goal: The "Seamless" experience.*
*   **16.1**: Implement "Window Mode" switch in configuration.
*   **16.2**: Refactor `OpenWindow` to trigger Host Window creation.
*   **16.3**: Per-window BitMap management.
*   **16.4**: Event routing refactor (Host Window ID -> Amiga Window pointer).
*   **16.5**: Rootless Menu handling strategy.

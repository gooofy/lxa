# LXA UI Testing Infrastructure

This document describes the UI testing infrastructure for LXA, enabling automated testing of graphical applications including input simulation and screen verification.

## Overview

LXA requires a comprehensive testing approach that goes beyond simple unit tests to verify that graphical applications work correctly. This includes:

1. **Console Device Unit Tests** - Test console.device in isolation
2. **UI Integration Tests** - Test input/output through windows
3. **Application Integration Tests** - Run real Amiga applications with automated input
4. **Screen Capture Tests** - Verify visual output matches expectations

## Architecture

### Test Host Modes

LXA can operate in several modes for testing:

1. **Headless Mode** - No display, console I/O only (current default for tests)
2. **Virtual Framebuffer Mode** - SDL renders to memory buffer, no window
3. **Screen Capture Mode** - Renders to buffer with screenshot capability
4. **Interactive Mode** - Normal windowed operation

### Input Injection

For automated testing, we need to inject input events without requiring a real display:

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Test Script    │────▶│  Input Injector  │────▶│  IDCMP Queue    │
│  (Python/C)     │     │  (Emucall)       │     │  (ROM)          │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```

New emucalls for input injection:
- `EMU_CALL_TEST_INJECT_KEY` - Inject a keyboard event (rawkey + qualifiers)
- `EMU_CALL_TEST_INJECT_MOUSE` - Inject a mouse event (x, y, buttons)
- `EMU_CALL_TEST_INJECT_STRING` - Inject a string as keyboard events

### Screen Capture

For visual verification:

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  ROM Rendering  │────▶│  Screen Buffer   │────▶│  Test Capture   │
│  (BitMap)       │     │  (Host Memory)   │     │  (PNG/Compare)  │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```

New emucalls for screen capture:
- `EMU_CALL_TEST_CAPTURE_SCREEN` - Capture current screen to file
- `EMU_CALL_TEST_CAPTURE_WINDOW` - Capture specific window to file
- `EMU_CALL_TEST_COMPARE_SCREEN` - Compare screen against reference image

## Console Device Testing

### Unit Test Categories

#### 1. Output Tests (`tests/console/output/`)
- CSI sequence parsing and execution
- Character rendering at correct positions
- Scrolling behavior
- Color and attribute handling
- Cursor positioning

#### 2. Input Tests (`tests/console/input/`)
- Raw key to ASCII conversion
- Modifier key handling (Shift, Ctrl, Caps Lock)
- Input buffer management
- Line mode vs raw mode
- Echo behavior

#### 3. Line Editing Tests (`tests/console/editing/`)
- Backspace within typed text
- Line buffer limits
- Return/newline handling
- Special character handling

### Console Test Program Structure

```c
/* tests/console/input_basic/main.c */
#include <exec/types.h>
#include <devices/console.h>
#include <intuition/intuition.h>

/* Test helper: inject keyboard input */
extern void test_inject_key(UWORD rawkey, UWORD qualifier);
extern void test_inject_string(const char *str);

int main(void)
{
    struct Window *win;
    struct IOStdReq *con_io;
    char buf[80];
    
    /* Open window and console */
    win = OpenWindow(...);
    con_io = open_console(win);
    
    /* Test 1: Basic character input */
    test_inject_string("hello\r");
    DoIO(con_io);  /* CMD_READ */
    ASSERT_STREQ(buf, "hello\r");
    
    /* Test 2: Backspace removes typed chars */
    test_inject_string("abc\b\bd\r");  /* type abc, backspace twice, type d */
    DoIO(con_io);
    ASSERT_STREQ(buf, "ad\r");
    
    /* Test 3: Cannot backspace past line start */
    /* Program outputs "default:" then reads */
    con_write(con_io, "default:");
    test_inject_string("\b\b\bnew\r");  /* Try to backspace over "default:" */
    DoIO(con_io);
    ASSERT_STREQ(buf, "new\r");  /* Only got "new", didn't affect "default:" */
    
    return 0;
}
```

## Application Integration Testing

### Test Runner Enhancements

Extend `tests/run_apps.py` with UI testing capabilities:

```python
class UITestRunner(AppRunner):
    """Enhanced runner with UI testing support."""
    
    def inject_keys(self, keys: str):
        """Inject keyboard input into the running application."""
        pass
    
    def capture_screen(self, filename: str):
        """Capture current screen state."""
        pass
    
    def wait_for_text(self, text: str, timeout: float = 5.0):
        """Wait for specific text to appear on screen."""
        pass
    
    def run_ui_test(self, app_name: str, script: list):
        """Run a UI test script against an application."""
        pass
```

### UI Test Script Format

Define UI tests in `app.json`:

```json
{
    "name": "KickPascal 2",
    "executable": "bin/KP2/KP",
    "ui_tests": [
        {
            "name": "startup",
            "steps": [
                {"wait_for": "Workspace/KBytes:", "timeout": 10},
                {"inject": "100\r"},
                {"wait_for": "loading", "timeout": 5},
                {"capture": "startup_complete.png"}
            ],
            "expected_screen": "reference/startup.png"
        }
    ]
}
```

### Screen Comparison

For visual regression testing:

1. **Reference Images** - Store expected screen states as PNG files
2. **Pixel Comparison** - Compare actual vs expected with tolerance
3. **Text Extraction** - OCR or font-aware text extraction for verification

## Implementation Phases

### Phase 1: Console Device Unit Tests (Priority: HIGH)
- [ ] Create `tests/console/input_basic/` - Basic keyboard input
- [ ] Create `tests/console/input_modifiers/` - Shift, Ctrl, Caps Lock
- [ ] Create `tests/console/input_buffer/` - Buffer limits and overflow
- [ ] Create `tests/console/line_mode/` - Cooked mode line editing
- [ ] Create `tests/console/raw_mode/` - Character-at-a-time input
- [ ] Create `tests/console/echo/` - Echo enable/disable

### Phase 2: Input Injection Infrastructure (Priority: HIGH)
- [ ] Add `EMU_CALL_TEST_INJECT_KEY` emucall
- [ ] Add `EMU_CALL_TEST_INJECT_STRING` emucall
- [ ] Add test helper library for ROM-side injection
- [ ] Add Python bindings for test scripts

### Phase 3: Screen Capture Infrastructure (Priority: MEDIUM)
- [ ] Add `EMU_CALL_TEST_CAPTURE_SCREEN` emucall
- [ ] Implement headless rendering mode
- [ ] Add PNG export for screen captures
- [ ] Implement image comparison utilities

### Phase 4: Application Integration Tests (Priority: MEDIUM)
- [ ] Extend `run_apps.py` with UI test support
- [ ] Create KickPascal 2 UI test suite
- [ ] Create reference screenshots for comparison
- [ ] Add CI integration for UI tests

### Phase 5: Advanced Testing (Priority: LOW)
- [ ] Text extraction from screen buffer
- [ ] Fuzzy image comparison with tolerance
- [ ] Performance benchmarking
- [ ] Memory leak detection during UI tests

## Console Device Behavior Specification

Based on research of authentic Amiga console.device behavior:

### Cooked Mode (Default)
- Line-buffered: Characters held until Return pressed
- Automatic echo: Typed characters displayed immediately
- Automatic filtering: Special keys filtered out
- Line editing: Backspace deletes typed characters only
- **Backspace limit**: Cannot delete past where Read() was called

### Raw Mode
- Character-at-a-time: Each keypress available immediately
- No automatic echo: Application must echo
- No filtering: All keys passed through
- No automatic line editing

### Key Behaviors to Test

1. **Normal typing**: Characters added to buffer and echoed
2. **Backspace**: Removes last typed character (if any)
3. **Return**: Terminates input, returns buffer contents
4. **Tab**: Advances to next tab stop (every 8 chars)
5. **Ctrl+C**: Sends break signal
6. **Arrow keys**: Generate escape sequences (raw mode only)

### Input Buffer Invariants

- Buffer contains ONLY user-typed characters
- Program-output text is NOT in buffer
- Backspace can only remove buffer contents
- Return terminates and returns buffer + newline

## Files to Create/Modify

### New Files
- `doc/ui_testing.md` - This document
- `src/lxa/test_inject.c` - Input injection emucalls
- `src/lxa/test_capture.c` - Screen capture emucalls
- `src/rom/lxa_test_helpers.c` - ROM-side test utilities
- `tests/console/input_basic/main.c` - Basic input tests
- `tests/console/line_mode/main.c` - Line editing tests
- `tests/ui/kp2_startup/test.py` - KP2 UI test

### Modified Files
- `src/lxa/lxa.c` - Add test emucalls
- `src/lxa/display.c` - Add headless/capture modes
- `tests/run_apps.py` - Add UI test runner
- `roadmap.md` - Add UI testing phase

## Example Test Cases

### Console Input Test: Backspace Behavior
```
Test: Backspace only deletes user-typed characters

Setup:
  1. Open window with console
  2. Console writes "Enter value: 80" to screen
  3. Console starts CMD_READ

Actions:
  1. User presses Backspace 5 times
  2. User types "100"
  3. User presses Return

Expected:
  - Screen shows: "Enter value: 80100" (backspace had no visual effect)
  - Read returns: "100\r" (only user input)
  
Rationale:
  - "80" was output by program, not in input buffer
  - Backspace cannot delete what's not in buffer
  - Authentic Amiga behavior
```

### Console Input Test: Normal Line Edit
```
Test: Backspace deletes typed characters

Setup:
  1. Open window with console
  2. Console writes "Enter name: " to screen
  3. Console starts CMD_READ

Actions:
  1. User types "Jonh"
  2. User presses Backspace twice
  3. User types "hn"
  4. User presses Return

Expected:
  - Screen shows: "Enter name: John" (corrected spelling)
  - Read returns: "John\r"
```

## Running Tests

```bash
# Run all console tests
make -C tests console

# Run specific console test
./build/host/bin/lxa tests/console/input_basic/input_basic

# Run UI tests for an application
./tests/run_apps.py --ui-test kickpascal2

# Run all UI tests
./tests/run_apps.py --all --ui-test
```

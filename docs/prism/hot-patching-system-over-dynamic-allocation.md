# Feature Request: Persistent Memory Hot-Patching System

## Summary
Implementation of a background hot-patching system that monitors the Dreamcast memory (RAM) and applies specific byte-level modifications once a target pattern is identified. This is designed to replace outdated static memory patching methods that fail with modern/dynamic memory management.

## Context & Motivation
Traditional memory patching (fixed-address "cheats") is often impossible for a specific subset of the Dreamcast library, most notably **Windows CE-based titles**. These games use dynamic memory allocation, meaning code and data can reside at different offsets every time the game is loaded or a new level is entered.

Current workarounds often require "cold patching" (modifying the GDI/CDI files directly), which is:
1. **Destructive**: It alters the original game files.
2. **Difficult to share**: Distributing patched binaries often violates copyright or requires complex XDelta patches.
3. **Inflexible**: Hard to toggle on/off.

A "Search & Replace" system with a periodic trigger solves this by allowing Flycast to identify and fix code in real-time, regardless of where the Windows CE kernel placed it in RAM.

## Proposed Logic
- **Trigger**: A background task that runs every `N` seconds (configurable).
- **Scan**: The emulator scans the specific memory regions (e.g., `0x8c000000` onwards).
- **Find & Replace**: Once `find_sequence` is detected, it is replaced by `replace_sequence`.
- **Persistence**:
    - **Option A (Once)**: Stop scanning once the patch is applied (best for code fixes).
    - **Option B (Continuous)**: Re-apply if the game overwrites the memory (best for values that the game engine frequently resets).

## Proposed Configuration Format (YAML)
To keep it readable and easy to share, a structured format like YAML is suggested.

```yaml
# example_patch.yaml
metadata:
  name: "Esppiral's 60 FPS + Physics Fix"
  game_id: "T1201N" # Optional: target specific Game ID

settings:
  polling_interval_ms: 2000 # Scan every 2 seconds
  stop_after_match: true     # Stop searching once applied

patches:
  - id: "change the framerate to 60fps"
    find: "02705A0E033E1C8B44D00B4006E443D3C36560753360CE0150E4526541C708F1536050700260"
    replace: "01705A0E033E1C8B44D00B4006E443D3C36560753360CE0150E4526541C708F1536050700260"

  - id: "compute the physics accordingly"
    find: "0000F0413C03000040DC1B8C44030000400300001C719DF00AF101E0F2612071"
    replace: "000070423C03000040DC1B8C44030000400300001C719DF00AF101E0F2612071"
```

## Technical Implementation Details
1. **Memory Range**: Focus on the main RAM area where Windows CE loads its binaries.
2. **Performance**: Scanning should be throttled or use a low-priority thread to avoid stuttering.
3. **Masking (Optional)**: Support for wildcards (e.g., `??`) for patterns containing pointers or variable data.

## Conclusion
This feature would bridge the gap between simple cheats and complex engine modifications. By moving away from "90s style" static addressing and providing a "set-and-forget" hot-patching system, Flycast would become the primary platform for advanced Dreamcast community hacks.
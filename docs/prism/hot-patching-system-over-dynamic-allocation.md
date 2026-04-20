# Feature Request: Persistent Memory Hot-Patching System

## Summary
Implementation of a background hot-patching system that monitors the Dreamcast memory (RAM) and applies specific byte-level modifications once a target pattern is identified. This is designed to replace outdated static memory patching methods that fail with modern/dynamic memory management, specifically for **Windows CE titles**.

## Context & Motivation
Traditional memory patching (fixed-address "cheats") is often impossible for Windows CE-based titles. These games use dynamic memory allocation, meaning code and data reside at different offsets every session.

For Windows CE titles, the memory is generally allocated and mapped during the initial boot/loading sequence. Since the Dreamcast has a limited pool of **16MB of main RAM**, scanning the entire memory space is extremely fast on modern hardware and has negligible performance impact.

A "Search & Replace" system with a periodic trigger allows Flycast to identify and fix code in real-time without destructive "cold patching" of game files.

## Proposed Logic
- **Trigger**: A background task running every `N` seconds until the pattern is found.
- **Optimized Scan (Search Window)**: Ability to define an address range (e.g., `0x8c100000 - 0x8c500000`) to further speed up the process, although scanning the full 16MB is already very efficient.
- **Find & Replace**: Standard byte-pattern matching.
- **Auto-Optimization Log**: When a pattern is found, Flycast outputs a log message indicating the exact address and **suggests a narrowed search window** for the user to optimize their configuration file.

## Proposed Configuration Format (YAML)
The `address_range` parameter allows for instantaneous results.

```yaml
# example_patch.yaml
metadata:
  name: "Esppiral's 60 FPS + Physics Fix"
  game_id: "T1201N"

trigger:
  polling_interval_ms: 2000 
  policy: once     

patches:
  - id: "change the framerate to 60fps"
    find: "02705A0E033E1C8B44D00B4006E443D3C36560753360CE0150E4526541C708F1536050700260"
    replace: "01705A0E033E1C8B44D00B4006E443D3C36560753360CE0150E4526541C708F1536050700260"

  - id: "compute the physics accordingly"
    find: "0000F0413C03000040DC1B8C44030000400300001C719DF00AF101E0F2612071"
    replace: "000070423C03000040DC1B8C44030000400300001C719DF00AF101E0F2612071"
```

## Technical Implementation Details
1. **Low Overhead**: Scanning 16MB of RAM is a trivial task for modern CPUs. The impact on emulation frame times will be non-existent if handled in a separate thread.
2. **Logging Feature**:
    - Upon success, log: `[Patch System] Pattern 'fps_fix_part_1' found at 0x8c12A450.`
    - Suggestion: `[Patch System] Suggestion: update address_range to '0x8c100000-0x8c200000' for instant matching next time.`
3. **Timing**: Since Windows CE loads binaries at startup, the trigger system ensures the patch is applied as soon as the kernel finishes its mapping.

## Conclusion
This feature modernizes Flycast's patching capabilities. By leveraging the small 16MB memory footprint of the Dreamcast, we can implement a robust, user-friendly "Search & Replace" system that solves the dynamic memory issues of Windows CE titles once and for all.
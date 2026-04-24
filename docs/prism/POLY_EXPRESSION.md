# Expression Engine Specification (C++ / ExprTK)

This document provides the technical details required to implement the Flycast G-Buffer expression evaluator in C++ using the [ExprTK](https://github.com/ArashPartow/exprtk) library.

## 1. Variable Mapping

The evaluator should expose the following variables to the user. These variables correspond to the OpenEXR channels provided by Flycast-Prism.

| Variable Name | EXR Channel Name | Description |
| :--- | :--- | :--- |
| `WP_X` | `Metadata.WorldPos.X` | World Position X |
| `WP_Y` | `Metadata.WorldPos.Y` | World Position Y |
| `WP_Z` | `Metadata.WorldPos.Z` | World Position Z |
| `Z` | `Depth.Z` | Linear Depth |
| `TH` | `Metadata.TextureHash` | Texture Hash (uint32) |
| `PC` | `Metadata.PolyCount` | Polygon Count (uint32) |
| `MID` | `Material.ID` | Raw Material ID (uint32) |

## 2. Material ID (MID) Decompositon

To simplify user expressions, the following boolean variables (0.0 or 1.0) should be pre-computed from the `MID` (Material ID) channel:

### List Type (Bits 4-6)
- `MID_OPAQUE`: `((MID >> 4) & 0x7) == 0`
- `MID_OPAQUE_MOD`: `((MID >> 4) & 0x7) == 1`
- `MID_TRANSLUCENT`: `((MID >> 4) & 0x7) == 2`
- `MID_TRANSLUCENT_MOD`: `((MID >> 4) & 0x7) == 3`
- `MID_PUNCH_THROUGH`: `((MID >> 4) & 0x7) == 4`

### Flags
- `MID_HAS_TEX`: `(MID >> 3) & 1` (Has Texture)
- `MID_GOURAUD`: `(MID >> 2) & 1` (Gouraud Shading)
- `MID_HAS_BUMP`: `(MID >> 1) & 1` (Has Bump Map)
- `MID_FOG`: `MID & 1` (Fog Control enabled)

## 3. Custom Functions

### `is_close(a, b, atol, rtol)`
This function is used for robust floating-point comparisons.

**Implementation Formula:**
```cpp
bool is_close(double a, double b, double atol = 1e-5, double rtol = 1e-3) {
    return std::abs(a - b) <= (atol + rtol * std::abs(b));
}
```

**ExprTK Integration:**
You can register this as a custom function or use a macro-like string replacement if necessary. However, ExprTK supports custom function objects:

```cpp
template <typename T>
struct is_close_func : public exprtk::igeneric_function<T>
{
    is_close_func() : exprtk::igeneric_function<T>("vv|vvv|vvvv") {}

    T operator()(const std::size_t& ps_index, parameter_list_t parameters)
    {
        T a = parameters[0];
        T b = parameters[1];
        T atol = (parameters.size() >= 3) ? T(parameters[2]) : T(1e-5);
        T rtol = (parameters.size() >= 4) ? T(parameters[3]) : T(1e-3);

        return (std::abs(a - b) <= (atol + rtol * std::abs(b))) ? T(1) : T(0);
    }
};
```

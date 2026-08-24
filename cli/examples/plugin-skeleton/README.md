# Plugin Skeleton — Example Custom SDG Indicator

This directory contains a minimal example plugin that demonstrates how to
build a custom SDG indicator for the GeoSDG CLI plugin system.

## Files

| File | Description |
|------|-------------|
| `plugin.json` | Plugin descriptor (name, parameters, inputs, outputs) |
| `MyIndicator.h` | Plugin header (class declaration) |
| `MyIndicator.cpp` | Plugin implementation (GDAL raster reading + computation) |
| `CMakeLists.txt` | Build configuration |

## Building

```bash
# From this directory:
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/gdal
cmake --build build

# Copy the library next to plugin.json:
cp build/libsdg_custom_forest.* .   # macOS/Linux
# or: cp build/Release/sdg_custom_forest.dll .  # Windows
```

## Installing

Copy the entire directory (including `plugin.json` and the compiled library)
to the GeoSDG CLI plugins folder:

```
geosdg-cli/
├── bin/
│   └── geosdg-cli
└── plugins/
    └── sdg-custom-forest/
        ├── plugin.json
        └── libsdg_custom_forest.dylib
```

## Using

```bash
# List available plugins (should show sdg-custom-forest)
geosdg-cli list-plugins

# Show plugin info
geosdg-cli plugin-info sdg-custom-forest

# Run the plugin
geosdg-cli sdg-custom-forest --init-lucc data/lucc.tif --forest-types 1,2,3
```

## SDK Header

The only header you need to include is `IIndicator.h`:

```cpp
#include "IIndicator.h"

class MyIndicator : public IIndicator {
    // Implement all pure virtual methods...
};

extern "C" IIndicator* createIndicator() { return new MyIndicator(); }
extern "C" void destroyIndicator(IIndicator* ptr) { delete ptr; }
```

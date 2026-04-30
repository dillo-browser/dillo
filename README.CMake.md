# Dillo CMake Build System

This directory contains a modern CMake build system for the Dillo web browser, replacing the traditional autotools-based build process.

## Quick Start

### Prerequisites
- CMake 3.10 or higher
- C++11 compatible compiler
- FLTK development libraries
- OpenSSL (for TLS support)
- libpng, libjpeg (for image support)
- pkg-config

### Build
```bash
# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4

# Install (requires sudo for system installation)
sudo cmake --install build

# Run from build directory (development)
./build/run_dillo.sh
```

### Uninstall
```bash
sudo cmake --build build --target uninstall
```

## Build Options

All features can be enabled/disabled with CMake options:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_COOKIES=ON \
    -DENABLE_PNG=ON \
    -DENABLE_JPEG=ON \
    -DENABLE_GIF=ON \
    -DENABLE_SVG=ON \
    -DENABLE_WEBP=ON \
    -DENABLE_IPV6=ON \
    -DENABLE_TLS=ON \
    -DENABLE_XEMBED=ON \
    -DENABLE_CONTROL_SOCKET=ON
```

## Installation Paths

By default, files are installed to standard locations:
- Executables: `/usr/local/bin/`
- Configuration: `/usr/local/etc/dillo/`
- DPI plugins: `/usr/local/lib/dillo/dpi/`
- Desktop files: `/usr/local/share/applications/`
- Icons: `/usr/local/share/icons/hicolor/`

To change the installation prefix:
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
```

## Development

### Running from Build Directory
The build system supports development mode where you can run dillo directly from the build directory without installation:

```bash
./build/run_dillo.sh
```

This script automatically:
- Sets up DPI plugin paths
- Copies configuration files to user directory
- Generates proper dpidrc configuration
- Runs dillo with correct environment

### Build Targets
- `dillo` - Main browser executable
- `dpid_bin` - DPI daemon (installed as `dpid`)
- `dpidc` - DPI client
- `dilloc` - Control socket client
- Various `.dpi` plugins for different protocols

### Output Directory
All executables are built to the build directory root for easy access during development.

## Migration from Autotools

This CMake build system is a drop-in replacement for the autotools build system. Key differences:

1. **Build commands**: Use `cmake` instead of `./configure && make`
2. **Configuration**: CMake options instead of configure flags
3. **Development mode**: Built-in support for running from build directory
4. **Cross-platform**: Better Windows/macOS support potential

## Troubleshooting

### Common Issues

**"cannot open output file .: Is a directory"**
- This was a target name conflict that has been resolved
- The `dpid` target is now built as `dpid_bin` to avoid directory conflicts

**Missing dependencies**
- Install FLTK development packages: `libfltk1.3-dev`
- Install image libraries: `libpng-dev libjpeg-dev`
- Install OpenSSL: `libssl-dev`

### Clean Build
To start fresh:
```bash
rm -rf build/
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
```

## Architecture

The build system is organized as follows:
- `CMakeLists.txt` - Main configuration and options
- `lout/CMakeLists.txt` - Utility library
- `dw/CMakeLists.txt` - Display widget libraries (3 sub-libraries)
- `dlib/CMakeLists.txt` - Dillo utility library
- `dpip/CMakeLists.txt` - DPI interface library
- `src/IO/CMakeLists.txt` - Input/output library with TLS support
- `src/CMakeLists.txt` - Main browser and control client
- `dpid/CMakeLists.txt` - DPI daemon and client
- `dpi/CMakeLists.txt` - All DPI plugins

## Contributing

When modifying the build system:
1. Test both Debug and Release builds
2. Verify installation and uninstallation work
3. Test development mode with `run_dillo.sh`
4. Ensure all targets build without warnings
5. Update this documentation for any new features

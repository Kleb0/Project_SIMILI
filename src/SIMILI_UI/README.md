# SIMILI_UI - CEF Integration

## What is this?

Chromium Embedded Framework (CEF) integration for SIMILI - allows modern HTML/CSS/JS UI alongside OpenGL 3D viewport.

## Architecture

- **SIMILI.exe** - Main OpenGL application
- **SIMILI_UI.exe** - Separate CEF browser process
- **Communication** - Windows Named Pipes IPC

## Why separate processes?

- ✅ No runtime library conflicts (/MT vs /MD)
- ✅ Isolation - crash-proof
- ✅ Can be launched independently

## Build

Run from project root:
```bash
build_ui.bat
deploy_ui.bat
```

Or use VS Code task: `Ctrl+Shift+B`

## Requirements

- CEF binaries in `src/ThirdParty/CEF/cef_binary/`
- Visual Studio 2022
- CMake 3.16+

## IPC Protocol

### UI → Main App
- `browser_created` - Browser ready
- `browser_closed` - Browser closed

### Main App → UI
- `quit` - Shutdown UI

## Future

- [ ] CEF Off-Screen Rendering (single window)
- [ ] Replace ImGui panels with web UI
- [ ] Hot reload HTML/CSS

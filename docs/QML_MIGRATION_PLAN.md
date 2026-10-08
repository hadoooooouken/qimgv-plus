# qimgv-plus: Qt Widgets to Qt Quick (QML) Migration Plan

Target: Qt 6.12 LTS, MSVC, Windows 10/11 x64. Branch: `qml`.

This document defines the approved, sequential migration stages. Each stage
is sized to be handed out as one task, keeps the application buildable and
runnable, and lists its acceptance criteria. Stage IDs (`S0.1`, `S1.3`, ...)
are the units referred to by the "Standing QML migration authorization" in
`AGENTS.md`.

---

## 1. Current Architecture (as of 3.6.4)

### 1.1 Layers

| Layer | Content | Size |
|---|---|---|
| Entry | `main.cpp`: `QApplication`, Fusion-based `ProxyStyle`, single-instance `QLocalServer`, global singletons (`settings`, `actionManager`, `scriptManager`, `inputMap`, `appActions`, `shrRes`) | ~260 lines |
| Mediator | `Core` (`core.h/.cpp`): navigation, slideshow, editing, file operations, history, cold start, AI resize | ~3000 lines |
| Components | `components/`: `DirectoryModel`, `DirectoryManager` + Windows watcher, `DirectoryPresenter` (MVP presenter over `IDirectoryView`), `Loader`, `Scaler`, `Thumbnailer`, caches, `Upscaler` (ncnn/Vulkan), `FileOpController`, `BatchConverter`, `WallpaperController`, `MimePayloadManager`, `ScriptManager`, `ActionManager` | UI-free, reusable |
| Sources | `sourcecontainers/`: `Image`, `ImageStatic`, `ImageAnimated`, `Thumbnail`, `DocumentInfo` (exiv2) | UI-free |
| Utils | `utils/`: `ImageLib`, `HdrToneMapper` (CPU), `ColorManager` (ICC via `GetICMProfileW` + `QColorSpace`), readers (DjVu, Blend), `PngWriter` | UI-free |
| GUI | `gui/`: `MW` (main window), `CentralWidget` (`QStackedWidget`), `DocumentWidget`, `ViewerWidget`, `ImageViewerV2` (`QGraphicsView`), `ThumbnailView` (`QGraphicsView` widget pool), folder view, 19 overlays, panels, 7 dialogs, 22 custom widgets | ~22k lines |

### 1.2 Rendering path today

- `ImageViewerV2` is a `QGraphicsView` whose viewport is a `QOpenGLWidget`
  (`AA_UseDesktopOpenGL`). On Windows this means desktop OpenGL composited
  into a widget window through an extra FBO copy.
- `FilterPixmapItem` / `PanoramaGraphicsItem` issue raw OpenGL calls from
  `QGraphicsItem::paint()`: texture upload with mipmaps, GLSL 1.x shaders
  (`res/shaders/filter.frag`: colour adjustments, CAS, smart sharpen;
  `boxreduce.*`: exact-ratio downsample; `panorama.frag`). Images larger than
  `GL_MAX_TEXTURE_SIZE` fall back to CPU painting.
- High-quality CPU filters (`QI_FILTER_SMART`, `QI_FILTER_MKS2021`) go through
  the `Scaler` component and a round trip back into the view.
- HDR tone mapping (`HdrToneMapper`, ~950 lines) and colour management
  (`QColorSpace::transform`) run on the CPU before upload.
- Everything else (thumbnail grid, strip, overlays, panels, fade animations
  via `QGraphicsOpacityEffect`, menus) is CPU-rasterized by the widget
  painter.

### 1.3 Coupling relevant to the migration

- `Core` holds a raw `MW *mw` and calls ~50 distinct `MW` methods (181 call
  sites): messages (`showMessage/Error/Warning/Success`: ~65), view mode
  queries (`currentViewMode`: 24), dialogs (`showConfirmation`,
  `fileReplaceDialog`, `getSaveFileName`, `showResizeDialog`), viewer
  commands (`showImage`, `showAnimation`, `onScalingFinished`,
  `visibleOriginalImageRect`, `currentScale`, ...). `core.h` includes
  `gui/mainwindow.h` and `gui/dialogs/printdialog.h`.
- `DirectoryPresenter` already talks to views only through `IDirectoryView`
  (MVP). This is the cleanest seam in the codebase and is reused as-is.
- 30 GUI files read the global `settings` directly (411 call sites).
  `Settings` has no `Q_PROPERTY` declarations, only getters/setters and a
  single `settingsChanged()` signal.
- `ActionManager::processEvent(QInputEvent*)` is the single keyboard/mouse
  shortcut dispatcher. It is UI-toolkit-agnostic and is kept.
- The "proxy" classes (`*OverlayProxy`, `FolderViewProxy`,
  `ThumbnailStripProxy`) exist to defer widget construction for startup
  speed. Qt Quick replaces them with `Loader { asynchronous: true }`.

### 1.4 Existing debt the migration must not propagate (flagged)

- Manual `delete` of singletons in `main.cpp::cleanupSingletons()` and raw
  owning pointers (`new ProxyStyle`, `new QLocalServer`, `MW *mw`) violate
  the ownership rule.
- `setColorAdjustments(float x7)` and `applyColorAdjustments(float x7)`
  violate the "group loose parameters into a struct" rule.
- `#ifdef _WIN32` / `#ifdef __GLIBC__` blocks in `main.cpp` / `core.h`.
- `ImageViewerV2` mixes view-transform math (fit modes, zoom levels, anchored
  zoom, edge snapping, view locks) with rendering and input handling.
- `SettingsDialog` (~4300 lines) mixes UI construction with settings
  load/save logic.

---

## 2. Target Architecture

```
main.cpp
 └─ QApplication (kept: QPrintDialog / QPrinter need QtWidgets)
     ├─ AppServices (owns singletons via std::unique_ptr, replaces cleanupSingletons)
     ├─ Core (mediator, unchanged responsibilities; talks to UI ports only)
     ├─ UI ports (pure C++ interfaces, implemented twice during transition):
     │    INotificationPort, IDialogPort, IViewerPort, IShellPort,
     │    IWindowPort, IViewModePort (outbound) + UiEvents (inbound signals)
     ├─ Widgets UI (legacy, until S4.2)        ─┐
     └─ Quick UI (QQmlApplicationEngine)        ─┴─ selected at startup
          ├─ QML module  qimgv.ui       (QML_ELEMENT C++ types + .qml files)
          ├─ QML module  qimgv.render   (ImageRenderItem: QQuickRhiItem, shaders via qt_add_shaders)
          └─ Bridges     (QObject adapters: SettingsBridge, ThemeBridge,
                          ActionBridge, DirectoryViewAdapter : IDirectoryView, ...)
```

Principles:

1. **Core and components do not change ownership.** The migration only adds
   adapters on the UI side and narrow interfaces on the `Core` side. No new
   responsibilities are added to `Core`.
2. **No business logic in QML.** QML files bind properties, lay out items,
   run visual animations and forward user intent to C++ (`invokable` methods
   or signals). Decisions (what to load, what to save, error handling) stay
   in C++ components.
3. **One GPU path.** All pixels on screen go through the Qt Quick scene graph
   on QRhi (Direct3D 11 by default). Image filtering, tone mapping, colour
   management and downsampling move into RHI shaders.
4. **Dual UI during transition.** The Quick UI is opt-in (`--ui=quick` and a
   hidden setting) until parity (S4.1), then becomes the default, then the
   widget UI is deleted (S4.2). Every stage leaves both UIs runnable.
5. **Render-thread discipline.** Qt Quick on Windows uses the threaded render
   loop. Renderer objects (`QQuickRhiItemRenderer`, `QSGNode`s) never touch
   `Core`, `Settings` or components. Data crosses only in
   `QQuickRhiItemRenderer::synchronize()` / `updatePaintNode()` while the GUI
   thread is blocked, as immutable value objects (`std::shared_ptr<const
   QImage>`, parameter structs).

---

## 3. Qt 6.12 (and recent 6.x) Feature Selection

### 3.1 Adopt

| Feature | Since | Where it is used |
|---|---|---|
| `QQuickRhiItem` (+ `TextureFormat::RGBA16F/RGBA32F`) | 6.7 | `ImageRenderItem`: portable GPU image renderer on D3D11/D3D12/Vulkan/GL, multi-pass (reduce, sharpen, tone map) |
| `qt_add_shaders()` (`.qsb`, GLSL 440 compiled for HLSL/SPIR-V/GLSL) | 6.0 | Port of `filter.frag`, `boxreduce.*`, `panorama.frag` |
| QRhi `generateMips`, 3D textures, float textures, `TextureSizeMax` query | 6.6 public | Mip chains, colour LUTs, HDR sources, tiling decision |
| `QQuickGraphicsConfiguration::setPipelineCacheSaveFile/LoadFile` | 6.5 | Persist pipeline cache to avoid shader compile cost at cold start |
| `qt_add_qml_module` + qmlcachegen/qmlsc AOT, `QML_ELEMENT`, `QML_SINGLETON` | 6.x | All QML is compiled; no runtime QML parsing of app code |
| `QQmlEngine::setExternalSingletonInstance()`; `QML_UNCREATABLE` combined with `QML_SINGLETON` | **6.12** | Expose existing C++ singletons (settings/theme/actions bridges) to QML without a factory or context properties |
| Missing `required property` is now an error | **6.12** | Use `required property` in every delegate; regressions fail loudly |
| `QRangeModel` (drag-and-drop, `sort()`, `match()`, `headerData()`, flags) | 6.10 / **6.12** | Small list models: bookmarks, format filters, shortcuts table, batch converter queue, scripts list |
| QML `SortFilterProxyModel` (+ `RangeFilter`, `RegExpFilter`, `AllOfFilter`, `AnyOfFilter`) | 6.10 / **6.12** | Filtering in settings (shortcut search), format filter list |
| `TreeView` (QML) over the existing `FileSystemModelCustom` | 6.3+ | Folder tree in folder view |
| `Popup.popupType: Popup.Window` / `Popup.Native` | 6.8 | Context menu and combo popups may exceed window bounds, like today's `QMenu` |
| `MenuItem` shows the shortcut of its `Action` | **6.12** | Context menu shortcuts, synced from `ActionManager` |
| `ToolTip.policy`, `ComboBox.highlightOnHover`, `Menu.separatorsCollapsible` | **6.12** | Panel buttons, sort/filter combos, context menu |
| `DoubleSpinBox`, `DialogButtonBox.defaultButton` | 6.11 | Colour adjustments, resize, CAS settings dialogs |
| `SearchField` (+ text selection APIs) | 6.10 / **6.12** | Name filter in folder view, shortcut search |
| `Flickable` position-to-child / `flickTo` APIs | 6.11 | Scroll-to-selection in grid and strip |
| `RectangularShadow` (per-corner radii), `MultiEffect` | 6.9 / 6.11 | GPU shadows and backdrop blur for overlays and floating panels |
| `Color` QML singleton | **6.12** | Derive hover/pressed/disabled variants in the style instead of hand-written extra colours |
| `QAccessibilityHints::motionPreference` | **6.12** | Disable fade/zoom/scroll animations when the OS requests reduced motion |
| Animated SVG in `QMovie` | **6.12** | Animated SVG documents flow through the existing `ImageAnimated` frame path at no extra cost |
| `VectorImage` `asynchronous` / `status` | **6.12** | Optional GPU (curve renderer) SVG display path, see S2.6 |
| Qt Canvas Painter (`QCanvasPainterItem`), now fully supported | **6.12** | GPU 2D painting for custom-drawn controls that are awkward as `Shape`s (map overlay, slider tracks), see S3.4 |
| `qmllint` new checks, `qmlls`, `<exe>_qmlpreview` target | **6.12** | CI-style lint target and fast QML iteration |
| `lupdate` preserves CRLF in `.ts` files | **6.12** | Translations keep repository line endings |
| `rcc` content deduplication | **6.12** | Automatic; no action needed |

### 3.2 Evaluate (spike only, not on the critical path)

| Feature | Reason |
|---|---|
| HDR swapchain for Qt Quick windows | Qt Quick has no public, documented API for an scRGB/HDR10 window swapchain in 6.12. A spike (S2.5b) may test the internal option; production keeps SDR output with GPU tone mapping. |
| Windows screen colour space derived from ICC (`ColorProfileGetDisplayDefault`) in the 6.12 QPA plugin | No public `QScreen` accessor was found in the 6.12 docs; `ColorManager`'s `GetICMProfileW` path stays until a public API exists. |
| Direct3D 12 / Vulkan scene graph backends | Offered as a user setting; D3D11 remains the default for startup time and driver maturity. |

### 3.3 Do not adopt

| Feature | Reason |
|---|---|
| `QtQuick.Controls.Native` style | Cannot be themed; the app has its own colour schemes (`ThemeStore`). Use a custom style built on `Basic`. |
| Qt Labs StyleKit / `QStyleKitStyle` | Labs, technology preview, no compatibility promise. |
| `QtCanvas2D` QML API | Technology preview in 6.12. Use the supported C++ `QCanvasPainterItem` instead. |
| Qt TaskTree, `QRangeModelAdapter` | Technology preview; existing `QThreadPool`/`QRunnable` architecture is kept. |
| `QQuickWidget` / `createWindowContainer` hybrid embedding | Extra offscreen copy per frame and two compositors; contradicts the GPU goal. The Quick UI is a separate top-level window. |
| QML `Image` + `QQuickImageProvider` for the main image | No custom filtering/tone mapping, CPU decode on provider threads, duplicate caching. |
| Qt Quick 3D for the panorama view | Large module and startup cost for a single fragment shader. |
| `QtQuick.Pdf` | PDF pages are already rasterized into the image pipeline by `ImageStatic::loadPdf()`. |

---

## 4. Stages

Legend for each stage: **Goal**, **Owner** (component that owns the logic),
**Scope**, **Acceptance**. Stages within a phase are sequential unless marked
"parallelizable".

### Phase 0: Preparation (refactoring only, no visible change)

These stages are pure refactoring commits, kept separate from new
functionality as required by `AGENTS.md`.

#### S0.1 Build system and QML module skeleton
- **Goal:** the project builds with Qt Quick available and an empty Quick UI
  can be launched behind a flag.
- **Owner:** build system; `main.cpp` startup selection only.
- **Scope:**
  - Raise `find_package(Qt6 6.12 ...)` and add `Quick`, `Qml`,
    `QuickControls2`, `QuickDialogs2`, `ShaderTools`, `QuickTest`, `Test`.
  - `qt_policy(SET ...)` / `qt_standard_project_setup(REQUIRES 6.12)` where
    compatible with the existing MSVC profile.
  - Create QML modules `qimgv.ui` and `qimgv.render` with
    `qt_add_qml_module` (static backing libraries linked into
    `qimgv-plus`), `NO_GENERATE_QTCONF` if the generated `qt.conf` conflicts
    with the custom plugin path logic in `main.cpp`.
  - Add `qt_add_lupdate` coverage for QML sources.
  - Add a `qimgv_tests` target (Qt Test) and a `qimgv_qml_tests` target
    (Qt Quick Test), registered with CTest. The repository currently has no
    tests; later stages add to these.
  - Add a `--ui=quick` command-line option that opens an empty
    `ApplicationWindow` (no behaviour) instead of `MW`.
- **Acceptance:** Release and Debug presets configure and build; the default
  startup path is unchanged; `--ui=quick` shows an empty window;
  `windeployqt` deploys the QML imports; CTest runs (empty suites pass).

#### S0.2 Ownership and startup cleanup
- **Goal:** remove manual memory management on the startup path before more
  objects are added to it.
- **Owner:** new `AppServices` value-owner in `main.cpp` scope.
- **Scope:** replace `initSingletons()/cleanupSingletons()` with an RAII
  owner (`std::unique_ptr` members, reverse destruction order preserved);
  give `ProxyStyle` and `QLocalServer` deterministic ownership; remove the
  `#ifdef _WIN32` / `__GLIBC__` blocks.
- **Acceptance:** zero manual `delete` in `main.cpp`; behaviour, single
  instance and command-line modes unchanged.

#### S0.3 UI ports: decouple `Core` from `MW`
- **Goal:** `Core` no longer includes or names any widget class.
- **Owner:** `Core` keeps its responsibilities; new interfaces live in
  `gui/ports/` (header-only, no Qt Widgets includes).
- **Split:** the stage is too large for one reviewable unit, so it is
  organised as sub-stages S0.3a–S0.3e. They are delivered together in one
  commit; each sub-stage below states its own scope.
- **Acceptance (whole stage):** `core.h` has no `gui/` include except the
  port headers; `ColdStartWindowController` depends on ports, not `MW`;
  no component under `components/` names `MW`; behaviour unchanged.

##### S0.3a `ColorAdjustments` value type
- `utils/coloradjustments.h` replaces the 7-float colour adjustment
  signatures across `Core`, `MW`, `ViewerWidget`, `ImageViewerV2`,
  `FilterPixmapItem`, `PanoramaGraphicsItem`, the colour adjustments
  overlay, `ImageLib::applyColorAdjustments()`/`getColorAdjustmentMatrix()`
  and `BatchConversionJob`. `hasAdjustments()` replaces the four
  duplicated epsilon checks.

##### S0.3b `INotificationPort`
- `showNotification(NotificationRequest)` (text, kind, optional duration)
  plus `hideNotifications()`; convenience helpers (`showMessage`,
  `showSuccess`, `showWarning`, `showError`, `showAiUpscale`,
  `showDirectory`, `showDirectoryStart/End`) are non-virtual.
- Components no longer reach the window from worker threads:
  `WallpaperController` emits `notificationRequested()` on the GUI
  thread; `FileOperationTask` reports errors through `FileOpTaskNotifier`
  and `FileOpController::operationFailed()`. `Core` forwards both to the
  port.

##### S0.3c `IDialogPort`
- Modal dialogs with value-object results: `confirm` →
  `ConfirmationResult`, `resolveFileReplace` → `FileReplaceDecision`
  (formerly `DialogResult`), `requestSavePath` → `SavePathResult`,
  `requestResize` → `std::optional<ResizeRequest>`, `requestText` →
  `TextInputResult` (create folder), `runBatchConverter` →
  `BatchConversionResult`, `print(PrintRequest)`.
- `FileOperationTask` asks `FileOpController::resolveFileReplace()`
  through a blocking queued call; the controller owns the `IDialogPort`
  reference.
- Settings stays a UI-internal action (the UI host routes
  `openSettings`); the rename prompt is non-modal and lives on
  `IShellPort`.

##### S0.3d `IViewerPort`, `IShellPort`, `IWindowPort`, `IViewModePort`
- `IViewerPort`: displayed image/animation, scaled image and upscaled
  crop delivery, view queries (`visibleOriginalImageRect`,
  `currentScale`, `devicePixelRatio`, `isBusyInteracting`,
  `isRenderingSettled`, `panoramaMode`).
- `IShellPort` (added; not in the original stage text): directory path,
  `ShellFileInfo` (replaces the 11-argument `setCurrentInfo`), metadata,
  sorting notifications, folder tree refresh, save overlay, crop panel,
  fullscreen info bar, rename prompt.
- `IWindowPort` (added): show/hide, visibility, conceal/reveal (opacity),
  raise/activate, native handle, geometry save, update suspension. Used by
  `Core::raiseWindow()`/standby and `ColdStartWindowController`.
- `IViewModePort` is implemented by the new `ViewModeController`
  (`components/viewmode/`), which owns the document/folder mode;
  `viewModeApplied()` drives the widget UI. Covered by `qimgv_tests`.
- `UiEvents` (`gui/ports/uievents.h`) is the inbound side: the user
  intents and readiness signals (`documentRenderingSettled`,
  `visibleThumbnailsReady`, `filesystemViewReady`) `Core` and
  `ColdStartWindowController` connect to.

##### S0.3e Widget UI composition root
- `WidgetUi` (`gui/widgetui/`) owns `MW` (`std::unique_ptr`), the
  `ViewModeController`, `UiEvents` and the port adapters
  (`widgetportadapters.*`), forwards `MW` signals into `UiEvents` and
  connects the window-only `ActionManager` actions (zoom, fit, scroll,
  fullscreen, overlays, settings).
- `Core(const UiPorts &)` receives the ports and the two `IDirectoryView`
  views grouped in `UiPorts`; `main.cpp` builds `AppTranslator` →
  `WidgetUi` → `Core`. Translation loading moved out of `Core` into
  `AppTranslator` so it still precedes widget construction.
- `MW` is now destroyed at exit. This exposed a latent teardown bug in
  `MainPanel` (member order let `~QWidget` delete a non-heap layout) and
  required an explicit release order of the shared widget pointers in
  `~MW`.

#### S0.4 Extract view-transform math from `ImageViewerV2`
- **Goal:** a UI-independent, unit-tested model of zoom/pan/fit that both
  the old and the new viewer use.
- **Owner:** new `ViewTransform` (plain C++ value type + `ViewTransformController`
  `QObject`) in `components/viewtransform/`.
- **Scope:** fit window/width/height/original, expand-small-images, zoom
  levels and fixed steps, anchored zoom (`zoomAnchor`), min/max scale,
  edge snapping, centring, lock zoom / lock view, saved viewport position,
  DPR handling, panorama yaw/pitch/FOV clamping. `ImageViewerV2` is switched
  to use it (refactor only).
- **Acceptance:** `ImageViewerV2` behaviour identical; `qimgv_tests` covers
  every fit mode, anchored zoom invariants, lock modes and DPR 1.0/1.5/2.0.
- **Delivered:**
  - `ViewTransform` (value type: scale, image position in viewport
    coordinates, fit/lock state, `ViewTransformConfig` settings struct),
    `PanoramaView` (yaw/pitch/FOV) and `ViewTransformController`
    (`QObject`; samples viewport size and pointer through `IViewSurface`
    at the start of every operation, then emits `transformChanged`,
    `scaleChanged`, `positionChanged`, `imageCentered`,
    `anchoredZoomApplied`, `panoramaChanged`).
  - `ImageViewerV2` no longer scrolls: its view scene rect is top-left
    aligned so scene coordinates equal viewport coordinates, and the image
    items are placed at `ViewTransform::imagePosition()`. This removes the
    10000/10000 item offset, the scroll-bar clamp of very large zoomed
    images and the first-`fitWindow` scroll-bar workaround.
  - Fit-mode re-detection after a manual zoom uses
    `ViewTransform::kScaleEpsilon` everywhere (previously exact `==` in
    three of four places).
  - Positions may differ from 3.6.4 by at most one logical pixel (rounding
    of the former integer scroll offsets).
  - `qimgv_tests` now runs several suites from `tests/unit/main.cpp`.

#### S0.5 QML bridges for global services
- **Goal:** QML can read settings, theme and actions through typed,
  notifying properties.
- **Owner:** bridge `QObject`s in `gui/quick/bridges/`; `Settings`,
  `ThemeStore`, `ActionManager` themselves stay untouched except for
  signals they already emit.
- **Scope:**
  - `SettingsBridge` (`QML_SINGLETON` + `QML_UNCREATABLE`, instance supplied
    with `QQmlEngine::setExternalSingletonInstance()`): `Q_PROPERTY` per
    UI-relevant setting, grouped by area (viewer, panel, folder view,
    overlays). Re-emits on `Settings::settingsChanged`.
  - `Theme` singleton: all `ColorScheme` colours, fonts, metrics from
    `UiMetrics`; icon glyph lookup from `IconFontManager`.
  - `ActionBridge`: `invoke(name)`, `shortcutFor(name)`,
    `handleKeyEvent()` that forwards `QKeyEvent`/`QWheelEvent` from QML items
    into `ActionManager::processEvent()`; exposes a `QRangeModel` of actions
    for menus.
- **Acceptance:** Qt Quick Test verifies property notifications for a
  settings change and a theme switch; `qmllint` passes on the module.
- **Delivered:**
  - New QML module `qimgv.bridges` (`gui/quick/bridges/`, static library
    imported like `qimgv.ui`). QML names: `AppSettings` (`SettingsBridge`),
    `Theme` (`ThemeBridge`), `Actions` (`ActionBridge`), plus the enum
    namespaces `SettingsEnums` and `FluentIcons`.
  - Push model: the bridges never read the global singletons. They hold
    snapshots (`UiSettingsSnapshot`, `ThemeSnapshot`) or talk to the
    `IActionDispatcher` seam, so `qimgv_qml_tests` links them without the
    application services. The app-side readers live in
    `gui/quick/adapters/` (`BridgeSnapshots`, `ActionManagerDispatcher`);
    `QuickUiHost` owns the bridges (declared before the engine so they
    outlive it), refreshes all three on `Settings::settingsChanged` (which
    also announces theme switches and shortcut edits) and publishes them with
    `setExternalSingletonInstance()`. `--ui=quick` now starts `AppServices`.
  - Settings are grouped into the QML value types `viewerSettings`,
    `panelSettings`, `folderViewSettings` and `overlaySettings`, one notify
    signal per group, emitted only when that group changed. Enum-typed
    settings use the scoped `SettingsEnums.*` mirrors, whose values are taken
    from `settings_types.h`.
  - `Theme`: `colors` (all 44 `ColorScheme` colours, camelCase), `fonts`
    (`base`, `compact`, `section`, `large`), `dark`, `iconFontFamily`,
    constant `compactIconSize`/`standardIconSize`, and
    `glyph(FluentIcons.X)`.
  - `Actions`: `invoke()`, `shortcutFor()`, `handleKeyEvent()` /
    `handleWheelEvent()` (rebuild `QKeyEvent`/`QWheelEvent` from the QML
    event's properties for `ActionManager::processEvent()`), and `actions`, a
    `QRangeModelAdapter` list with `name`/`shortcut` roles that is reset only
    when a shortcut actually changed.
  - Refactors needed by the bridges: `FluentIcon` and its codepoint table
    moved to `utils/fluenticon.{h,cpp}` (`Q_NAMESPACE FluentIcons`, a
    using-declaration keeps `FluentIcon::X`) in the static library
    `qimgv_fluenticons`, so its meta-object exists once; `IconFontManager`
    gained `family()`. The text-style point sizes moved from
    `Settings::loadStylesheet()` into `UiMetrics::typographyFor()`, shared by
    the stylesheet and `Theme.fonts`.
  - `Main.qml` binds its background to `Theme.colors.background`.
    `qimgv_qml_tests` is also the `qimgv.tests` module, whose `Fixture`
    singleton provides per-engine bridges over a fake dispatcher;
    `tst_bridges.qml` covers per-group notification, no notification on an
    unchanged snapshot, binding updates, enums, a theme switch, glyph lookup,
    invoke/shortcut lookup, the model, and key and wheel forwarding.
  - Flagged, not changed: `Settings::absoluteZoomStep()` /
    `setAbsoluteZoomStep()` are declared in `settings.h` but never defined, so
    the bridge does not expose them.

### Phase 1: GPU image renderer (the core of the GPU goal)

All Phase 1 work lives in the `qimgv.render` module and is exercised by a
test QML scene and by `--ui=quick`. The widget viewer is not modified.

#### S1.1 `ImageRenderItem`: base textured rendering
- **Goal:** display a `std::shared_ptr<const QImage>` through QRhi with
  correct premultiplied alpha, mipmaps and nearest/bilinear/trilinear
  sampling.
- **Owner:** `ImageRenderItem : QQuickRhiItem` (GUI thread state) and
  `ImageRenderer : QQuickRhiItemRenderer` (render thread).
- **Scope:**
  - Transform from `ViewTransform` (S0.4) supplied as a value object in
    `synchronize()`.
  - Upload path equivalent to `FilterPixmapItem::uploadPremultipliedTexture`
    (no straight-alpha conversion), `generateMips` on upload.
  - **Tiling** for images larger than `QRhi::TextureSizeMax`: a tile grid of
    textures with one-texel overlap; replaces today's CPU fallback.
  - Transparency checkerboard and background colour drawn in the same pass.
  - Device-loss / `QRhi` resource rebuild handling; errors reported through a
    `renderError(QString)` signal, never swallowed.
- **Acceptance:** pixel comparison against the current viewer at 100 %,
  50 %, 25 %, 300 % on opaque, alpha and 16-bit test images; a 32000 x 8000
  image renders without CPU fallback; no GUI-thread access from the
  renderer.

#### S1.2 Shader port: colour adjustments, sharpening, exact downsample
- **Goal:** feature parity with `filter.frag` and `boxreduce.*` on all RHI
  backends.
- **Owner:** `ImageRenderer`; shader sources in `res/shaders/rhi/*.frag/.vert`
  (GLSL 440), compiled by `qt_add_shaders`.
- **Scope:** port CAS, smart sharpen (CPU-equivalent path
  `QI_FILTER_SMART_GPU`), colour matrix/offset adjustments; settle-triggered
  exact-ratio box reduce into an intermediate render target
  (`QRhiTextureRenderTarget`), reused while the scale is unchanged; uniform
  buffer layout as a `std140` struct with named `constexpr` defaults.
- **Acceptance:** visual parity with the widget viewer for every
  `ScalingFilter` GPU mode; D3D11, D3D12 and Vulkan backends verified via
  `QSG_RHI_BACKEND`.

#### S1.3 GPU replacement for CPU display scaling
- **Goal:** stop the `Scaler` CPU round trip for on-screen display.
- **Owner:** `ImageRenderer` for display; `Scaler` remains for file
  output (resize/save/batch).
- **Scope:** implement `QI_FILTER_SMART` and `QI_FILTER_MKS2021` as
  separable two-pass shaders (horizontal then vertical into intermediate
  targets) applied on settle; keep the CPU path selectable as a fallback
  setting for one release.
- **Acceptance:** visual comparison against CPU output within a documented
  tolerance; no `scalingRequested` traffic from the Quick viewer; settle
  latency measured and not worse than today.

#### S1.4 GPU HDR tone mapping and colour management
- **Goal:** move `HdrToneMapper` and display colour transforms to the GPU.
- **Owner:** `ImageRenderer` (shaders); `ColorManager` remains the single
  source of the target colour space and produces transform data.
- **Scope:**
  - Upload linear float images (`QImage::Format_RGBA16FPx4` /
    `RGBA32FPx4`) as `RGBA16F` / `RGBA32F` textures.
  - Port each `ToneMapOperator` and the "HDR white" parameter to shader
    code; CPU tone mapping is skipped when the GPU path is active.
  - Colour management: parametric spaces (sRGB, Display P3, Adobe RGB,
    custom primaries) as a 3x3 matrix + transfer function in the shader;
    ICC profiles that are not parametric as a 3D LUT texture built once per
    (source profile, target profile) pair on a worker thread with
    `QColorTransform`, cached by profile identity.
- **Acceptance:** EXR/HDR/AVIF/JXL HDR samples match the CPU output within
  tolerance; switching operator or white level re-renders without
  re-decoding; LUT generation never runs on the GUI or render thread.

#### S1.5 Animation, panorama and upscaled-crop layers
- **Goal:** remaining viewer content types on the GPU path.
- **Owner:** `ImageRenderItem` (layers), `ImageAnimated` (frame source,
  unchanged).
- **Scope:** animated frames pushed as texture updates (reuse textures,
  no reallocation per frame), playback/loop/finished state exposed as
  properties; panorama mode as a renderer mode using a ported
  `panorama.frag`; upscaled-crop overlay layer (`setUpscaledCrop`) as a
  second textured quad.
- **Acceptance:** GIF/WebP/APNG/animated SVG playback timing matches the
  widget viewer; panorama yaw/pitch/FOV parity; AI upscale crop displays at
  the correct position at every zoom.

#### S1.6 `ImageViewport` interaction component
- **Goal:** the complete viewer as a reusable QML component.
- **Owner:** `ImageViewport.qml` (visuals, input handlers) +
  `ViewTransformController` (all math and state).
- **Scope:** `WheelHandler` (wheel zoom, trackpad detection preserved),
  `DragHandler` (pan, wrapping pan, RMB zoom gesture, drag-out start),
  `PinchHandler`, `TapHandler` (double click, click zones); smooth zoom and
  scroll animations driven by the controller, disabled by
  `QAccessibilityHints::motionPreference`; key events forwarded to
  `ActionBridge`; `renderingSettled` derived from
  `QQuickWindow::afterFrameEnd` after the settle pass completes (replaces
  `QOpenGLWidget::frameSwapped`).
- **Acceptance:** every viewer action in `ActionManager` works in
  `--ui=quick`; zoom indicator and cursor auto-hide behave as before;
  `IViewerPort` is fully implemented by a `QuickViewerPort` adapter.

### Phase 2: Quick application shell

#### S2.1 Main window, startup and window states
- **Goal:** a functional `--ui=quick` window showing images from `Core`.
- **Owner:** `QuickMainWindowController` (`QObject`) implementing the ports
  for the Quick UI; `Main.qml` for layout only.
- **Scope:** `QQmlApplicationEngine` created after services; geometry
  save/restore, fullscreen and pseudo-fullscreen, multi-display memory;
  `DropArea` forwarding to `Core::onDropIn` via `MimePayloadManager`;
  `ColdStartWindowController` adapted to reveal on the first rendered frame
  (no opacity trick, no delayed show; see `AGENTS.md` startup rules);
  pipeline cache load/save via `QQuickGraphicsConfiguration`; graphics API
  setting (D3D11 default, D3D12, Vulkan) applied before the first window.
- **Acceptance:** cold start time and first-frame time measured against the
  widget UI on the same machine (Release preset) and not worse; single
  instance raise works.

#### S2.2 Style and theme
- **Goal:** the Quick UI matches the current look in light and dark schemes.
- **Owner:** custom Qt Quick Controls style module `qimgv.style` (based on
  `Basic`), reading only the `Theme` singleton.
- **Scope:** Button, ToolButton, ComboBox (`highlightOnHover`), Slider,
  SpinBox/DoubleSpinBox, CheckBox, RadioButton, TextField/SearchField,
  ScrollBar, Menu/MenuItem (shortcut display), ToolTip (`policy`), Popup;
  icon font glyph component; derived state colours via the 6.12 `Color`
  singleton; overlay shadows via `RectangularShadow`.
- **Acceptance:** Qt Quick Test snapshot of a control gallery in both
  schemes; theme switch at runtime updates without restart.

#### S2.3 Overlays and floating messages (parallelizable after S2.2)
- **Goal:** port the 19 overlays as GPU-composited items.
- **Owner:** each overlay is a QML component; state comes from existing
  `Core`/`MW` signals through the ports; any logic currently inside overlay
  `.cpp` files (e.g. rename validation, copy/move target lists) moves to a
  small C++ model first.
- **Scope:** floating message, zoom indicator, image info (exif model),
  fullscreen info bar, controls overlay, click zones, copy/move, rename,
  save confirm, colour adjustments (`DoubleSpinBox`, live preview through
  `ImageRenderItem`), CAS settings, crop overlay (handles via `Shape`),
  draggable slider. Fade animations via `Behavior on opacity` and
  `Loader { asynchronous: true }` for rarely used overlays (replaces the
  `*Proxy` classes).
- **Acceptance:** every overlay reachable from its action; keyboard focus
  rules match (`acceptKeyboardFocus`); no overlay is instantiated at
  startup unless enabled by settings.

#### S2.4 Directory view adapter and thumbnail strip
- **Goal:** the bottom/side thumbnail panel in QML.
- **Owner:** `DirectoryViewAdapter : QAbstractListModel, IDirectoryView`
  (C++); `DirectoryPresenter` is reused unchanged.
- **Scope:** model roles (path, name, isDir, thumbnail, pending,
  unavailable, selected, dragHover); visible-range reporting from
  `ListView` drives `thumbnailsRequested` (same contract as
  `ThumbnailView::loadVisibleThumbnails`); `ThumbnailItem` (`QQuickItem`)
  creates its texture with `QQuickWindow::createTextureFromImage` in
  `updatePaintNode` from the `Thumbnail` held by the model (no image
  provider, no URL round trip); panel pin/auto-hide, position
  (top/bottom/left/right), styles.
- **Acceptance:** scrolling 10 000 items stays at display refresh rate
  with no GUI-thread decode; `visibleThumbnailsReady` semantics preserved
  for cold start.

#### S2.5 Context menu and side/crop panel
- **Goal:** menus and the crop side panel.
- **Owner:** menu structure defined in C++ (`ContextMenuModel` built from
  `ActionManager`), presentation in QML.
- **Scope:** `Menu { popupType: Popup.Native }` with fallback to
  `Popup.Window`; `Menu.separatorsCollapsible`; path selector submenu;
  crop panel with numeric inputs and aspect presets.
- **Acceptance:** menu opens beyond window edges; all shortcuts displayed
  match `ActionManager`.

#### S2.5b Spike: HDR output (optional, not blocking)
- **Goal:** measure whether an scRGB swapchain is viable for HDR displays.
- **Scope:** prototype only, in a separate branch; documents findings.

#### S2.6 SVG display path
- **Goal:** sharp SVG at any zoom on the GPU.
- **Owner:** `ImageViewport` chooses the layer; `Loader` decides the
  document type as today.
- **Scope:** default: re-rasterize the SVG with `QSvgRenderer` on a worker
  thread at the settled scale and display through `ImageRenderItem`
  (filters keep working). Evaluate `VectorImage { asynchronous: true;
  preferredRendererType: VectorImage.CurveRenderer }` as an opt-in fast path
  for simple SVGs, switching on `status`.
- **Acceptance:** SVG parity with `QGraphicsSvgItem` rendering; no GUI
  thread stall on complex SVGs.

### Phase 3: Folder view and dialogs

#### S3.1 Folder view
- **Goal:** the folder browser in QML.
- **Owner:** second `DirectoryViewAdapter` instance for the grid;
  `FileSystemModelCustom` reused as the `TreeView` model; bookmarks as a
  `QRangeModel` with drag-and-drop (6.12).
- **Scope:** `GridView` with rubber-band selection, range/toggle selection,
  keyboard navigation and type-ahead (forwarded to the presenter), drag-out
  (`Drag.mimeData` with `text/uri-list` built by `MimePayloadManager`),
  drop-into; top bar: path, sort `ComboBox`, format filter, name filter
  `SearchField`, icon size `Slider`; splitter (`SplitView`).
- **Acceptance:** feature parity checklist for `FolderView`,
  `FolderGridView`, `BookmarksWidget`; thumbnail cache and cold start
  readiness unchanged.

#### S3.2 Small dialogs
- **Goal:** resize, file replace, rename, shortcut creator, script editor.
- **Owner:** each dialog gets a C++ view-model (`QObject`) holding
  validation and result building; QML is presentation only.
- **Scope:** `Dialog` + `DialogButtonBox.defaultButton`; native
  `FileDialog`/`FolderDialog`/`MessageDialog` from `QtQuick.Dialogs` for
  save paths and confirmations.
- **Acceptance:** `IDialogPort` fully implemented by the Quick UI.

#### S3.3 Settings dialog
- **Goal:** split the 4300-line `SettingsDialog` into a settings view-model
  and QML pages.
- **Owner:** `SettingsEditorModel` (C++): load, validate, apply, reset
  defaults, shortcut table (`QRangeModel` + `SortFilterProxyModel`),
  scripts list, theme editor; QML pages per tab, loaded lazily.
- **Scope:** done in two commits: (a) refactor, extracting the model and
  switching the widget dialog to it; (b) QML pages.
- **Acceptance:** applying settings from either UI yields identical
  `qimgv-plus.ini` content.

#### S3.4 Batch converter, print, map overlay
- **Goal:** the remaining complex windows.
- **Owner:** `BatchConverter` component (unchanged) + `BatchConverterModel`;
  `PrintController` keeps `QPrinter`/`QPrintDialog` (native Windows dialog,
  reason for keeping `QApplication`); map overlay drawn with
  `QCanvasPainterItem` or tile `Image`s depending on its current content.
- **Acceptance:** batch progress updates do not stall the UI; printing
  output identical; map overlay parity.

### Phase 4: Switch-over and removal

#### S4.1 Parity review and default switch
- **Goal:** Quick UI becomes the default; `--ui=widgets` remains for one
  release as a fallback.
- **Scope:** full parity checklist (every action, every overlay, every
  dialog, every setting); performance report (cold start, first frame,
  zoom/pan frame times with `qmlprofiler` and `QSG_RENDER_TIMING`,
  thumbnail scroll, memory, VRAM); `qmllint` clean.

#### S4.2 Remove the widget UI
- **Goal:** delete the legacy code paths.
- **Scope:** remove `gui/` widget classes, `ProxyStyle`, `FilterPixmapItem`,
  `PanoramaGraphicsItem`, old GLSL shaders, `Qt6::OpenGLWidgets`,
  `Qt6::SvgWidgets`, `AA_UseDesktopOpenGL`, the `*Proxy` deferral classes,
  `IDirectoryView` widget implementations; keep `Qt6::Widgets` +
  `PrintSupport` only for printing.
- **Acceptance:** no `QWidget` subclass remains outside the print path;
  deploy size and startup measured.

---

## 5. Cross-Cutting Rules for Every Stage

- **Threads:** decode, scale, LUT generation and thumbnailing stay on the
  existing pools. The render thread only consumes immutable snapshots
  handed over in `synchronize()`/`updatePaintNode()`. No `QQmlEngine`
  access from worker threads.
- **Startup:** heavy QML (folder view, dialogs, settings) behind
  `Loader { active: false; asynchronous: true }`; only the viewer and the
  messages overlay are created before the first frame. Pipeline cache is
  loaded before the first window is exposed.
- **Errors:** RHI failures (`QRhi::isDeviceLost`, resource `create()`
  returning false, shader load failures) are reported through explicit
  signals to the owning controller and surfaced via `INotificationPort`.
- **Constants:** shader defaults, thresholds and timings are named
  `constexpr` values (C++) or `readonly property` values in a QML constants
  singleton.
- **Translations:** QML strings use `qsTr()`; contexts stay stable so
  existing `.ts` entries can be migrated.
- **Line endings:** new files are CRLF in the working tree; verify with
  `git ls-files --eol` after each stage.
- **Commits:** refactoring commits (Phase 0, S3.3a) separate from feature
  commits.

---

## 6. Stage Dependency Overview

```
S0.1 -> S0.2 -> S0.3 -> S0.4 -> S0.5
                          |       |
                          v       v
                S1.1 -> S1.2 -> S1.3 -> S1.4 -> S1.5 -> S1.6
                                                         |
                                                         v
                S2.1 -> S2.2 -> {S2.3, S2.4, S2.5, S2.6}   (S2.5b optional)
                                                         |
                                                         v
                                  S3.1 -> S3.2 -> S3.3 -> S3.4
                                                         |
                                                         v
                                                  S4.1 -> S4.2
```

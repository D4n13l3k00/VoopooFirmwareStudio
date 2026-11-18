#include "i18n.h"
#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")

namespace i18n {

static const char* g_table[(int)Language::Count][(int)Str::Count] = {};
static Language g_currentLanguage = Language::English;

static const std::vector<LanguageInfo> g_supportedLanguages = {
    { Language::English,     "en", "English" },
    { Language::Russian,     "ru", "Русский" },
    { Language::Ukrainian,   "uk", "Українська" },
    { Language::Belarusian,  "be", "Беларуская" },
    { Language::Kazakh,      "kk", "Қазақша" },
    { Language::Uzbek,       "uz", "Oʻzbekcha" },
    { Language::Azerbaijani, "az", "Azərbaycan" },
    { Language::Kyrgyz,      "ky", "Кыргызча" },
    { Language::Tajik,       "tg", "Тоҷикӣ" },
    { Language::Armenian,    "hy", "Հայերեն" },
    { Language::Georgian,    "ka", "ქართული" },
    { Language::Romanian,    "ro", "Română" },
    { Language::Polish,      "pl", "Polski" },
    { Language::German,      "de", "Deutsch" },
    { Language::Spanish,     "es", "Español" },
    { Language::French,      "fr", "Français" },
};

static void InitEnglish() {
    auto& s = g_table[(int)Language::English];
    s[(int)Str::MenuFile] = "File";
    s[(int)Str::MenuEdit] = "Edit";
    s[(int)Str::MenuLanguage] = "Language";
    s[(int)Str::MenuHelp] = "Help";
    s[(int)Str::SettingsTitle] = "Settings";
    s[(int)Str::SettingsGeneral] = "General";
    s[(int)Str::SettingsInterfaceEmulator] = "Interface / Emulator";
    s[(int)Str::SettingsDefaultsNote] = "Defaults apply to new views and documents; zooms at the next launch or when a new view opens. Current zooms and project content are not changed.";
    s[(int)Str::SettingsImmediateNote] = "Language, motion and completed-log review time apply when you save.";
    s[(int)Str::SettingsSmoothAnimations] = "Smooth interface transitions";
    s[(int)Str::SettingsLogReviewSeconds] = "Completed-log review time";
    s[(int)Str::SettingsDefaultTarget] = "Initially selected screen";
    s[(int)Str::SettingsDefaultBootStages] = "Startup screen stages";
    s[(int)Str::SettingsDefaultZooms] = "Default zoom";
    s[(int)Str::SettingsExpandedSections] = "Initially expanded sections";
    s[(int)Str::SettingsResetDefaults] = "Reset to factory defaults";
    s[(int)Str::SettingsSaveError] = "Could not save settings. Check that studio_config.ini and its folder are writable. Changes were not applied.";
    s[(int)Str::SettingsRollbackError] = "Could not save settings or restore the previous language on disk. Check studio_config.ini permissions and save again.";
    s[(int)Str::UpdateTitle] = "Studio updates";
    s[(int)Str::UpdateVersion] = "Installed version: %s";
    s[(int)Str::UpdateCheck] = "Check for updates";
    s[(int)Str::UpdateChecking] = "Checking GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "You have the latest version.";
    s[(int)Str::UpdateDownload] = "Download";
    s[(int)Str::UpdateDownloading] = "Downloading and verifying...";
    s[(int)Str::UpdateInstall] = "Install and restart";
    s[(int)Str::UpdateReady] = "Ready to install.";
    s[(int)Str::UpdateChangelog] = "Release notes";
    s[(int)Str::UpdateFailed] = "Update failed";
    s[(int)Str::UpdateAvailable] = "Available version: %s";
    s[(int)Str::MenuCreate] = "New...";
    s[(int)Str::MenuOpen] = "Open...";
    s[(int)Str::MenuImport] = "Import resources...";
    s[(int)Str::MenuExport] = "Export resources...";
    s[(int)Str::MenuBuild] = "Build firmware...";
    s[(int)Str::MenuExit] = "Exit";
    s[(int)Str::StartupChooseFirmware] = "Choose firmware";
    s[(int)Str::StartupOpenFile] = "Open file from PC...";
    s[(int)Str::StartupOpenBuiltin] = "Create";
    s[(int)Str::StartupNoProfiles] = "No built-in firmware available. Open a file from your PC.";
    s[(int)Str::StartupImageUnavailable] = "Image unavailable";
    s[(int)Str::SupportFull] = "Full";
    s[(int)Str::SupportNone] = "Not supported";
    s[(int)Str::SupportPalette] = "Color palette";
    s[(int)Str::SupportMonochrome] = "Monochrome display; no color palette.";
    s[(int)Str::EmulatorHistoryDay] = "History day";
    s[(int)Str::EmulatorClockHour] = "Hours";
    s[(int)Str::EmulatorClockMinute] = "Minutes";
    s[(int)Str::EmulatorClockField] = "Selected field";
    s[(int)Str::EditionFactory] = "Factory";
    s[(int)Str::EditionStandard] = "Standard";
    s[(int)Str::LabelModified] = "* Modified (unsaved)";
    s[(int)Str::UnsavedTitle] = "Unsaved firmware changes";
    s[(int)Str::UnsavedPrompt] = "Save your firmware changes before continuing?";
    s[(int)Str::BtnSaveChanges] = "Save";
    s[(int)Str::BtnDiscardChanges] = "Discard";
    s[(int)Str::BtnCancelAction] = "Cancel";
    s[(int)Str::MsgFirmwareOnlySaved] = "Firmware saved, but the report could not be saved: %s";
    s[(int)Str::SaveIdentityError] = "Cannot verify the destination, or it aliases a protected source/output file";
    s[(int)Str::SaveTemporaryError] = "Cannot create a temporary file beside the destination";
    s[(int)Str::SaveWriteError] = "Cannot write the complete temporary file";
    s[(int)Str::SaveFlushError] = "Cannot flush the temporary file to disk";
    s[(int)Str::SaveCloseError] = "Cannot close the temporary file";
    s[(int)Str::SaveBackupError] = "Cannot back up the existing destination; it was not replaced";
    s[(int)Str::SaveCommitError] = "Cannot atomically replace the destination; it was not replaced";
    s[(int)Str::MsgWindowsErrorCode] = "%s (Windows error %lu)";
    s[(int)Str::CompareSourceOpened] = "Current editor (including unsaved changes)";
    s[(int)Str::CompareSourceReference] = "Matching stock firmware";
    s[(int)Str::CompareSourceFile] = "Firmware or project file";
    s[(int)Str::PatchPreview] = "Preview · live draft on native firmware";
    s[(int)Str::PatchTransfer] = "Copy between sections";
    s[(int)Str::PatchCopyAll] = "Settings and image";
    s[(int)Str::PatchCopySettings] = "Settings only";
    s[(int)Str::PatchCopyImage] = "Image only";
    s[(int)Str::PatchClipboardError] = "Clipboard does not contain compatible patch data.";
    s[(int)Str::PatchChecking] = "Checking that the draft fits into the firmware...";
    s[(int)Str::PatchBuildError] = "Does not fit into the firmware: %s";
    s[(int)Str::RiskTitle] = "WARNING: TESTED ONLY IN THE EMULATOR";
    s[(int)Str::RiskBody] = "Code patches and UI layout changes were verified only in the built-in emulator. They have NOT been tested on real hardware.";
    s[(int)Str::RiskScope] = "Flashing such firmware is entirely at your own risk: the device may misbehave or fail to boot.";
    s[(int)Str::RiskWait] = "Read the warning (%d s)";
    s[(int)Str::RiskAccept] = "I understand the risk, continue";
    s[(int)Str::RiskSaveError] = "Could not save consent to the config file.";
    s[(int)Str::PatchCustomScreens] = "Custom screens";
    s[(int)Str::PatchMemoryTitle] = "Application Flash · draft · bootloader and settings excluded";
    s[(int)Str::PatchMemoryUsed] = "Used";
    s[(int)Str::PatchMemoryFree] = "Free";
    s[(int)Str::PatchMemoryPatches] = "Patches";
    s[(int)Str::PatchMemoryBytes] = "%zu B";
    s[(int)Str::PatchMemorySignedBytes] = "%+lld B";
    s[(int)Str::PatchMemoryDeltaNote] = "Delta vs loaded firmware; shared data counted in screen order.";
    s[(int)Str::PatchMemoryBoot] = "Δ Startup";
    s[(int)Str::PatchMemoryShutdown] = "Δ Shutdown";
    s[(int)Str::PatchMemoryDeviceInfo] = "Δ CHIP ID/version";
    s[(int)Str::PatchMemoryLayout] = "Δ UI layout";
    s[(int)Str::PatchSourceError] = "Image source or placement cannot be saved: maximum 16384 pixels per side and 64 MiB of RGBA pixels.";
    s[(int)Str::PatchPreviousFrame] = "Previous frame";
    s[(int)Str::PatchNextFrame] = "Next frame";
    s[(int)Str::PatchStop] = "Stop";
    s[(int)Str::PatchLoop] = "Loop";
    s[(int)Str::PatchPlaybackSpeed] = "Playback speed";
    s[(int)Str::PatchPlaybackTime] = "%.2f / %.2f s · frame %d/%d";
    s[(int)Str::PatchPlaybackPreparing] = "Preparing frames...";
    s[(int)Str::PatchPlaybackReady] = "Ready";
    s[(int)Str::PatchSoftWipe] = "Soft wipe";
    s[(int)Str::PatchChecker] = "Checker reveal";
    s[(int)Str::PatchAdjustments] = "Image adjustments";
    s[(int)Str::PatchBrightness] = "Brightness";
    s[(int)Str::PatchContrast] = "Contrast";
    s[(int)Str::PatchGamma] = "Gamma";
    s[(int)Str::PatchSaturation] = "Saturation";
    s[(int)Str::PatchSharpness] = "Sharpness";
    s[(int)Str::PatchToneCurve] = "Tone curve";
    s[(int)Str::PatchShadows] = "Shadows";
    s[(int)Str::PatchMidtones] = "Midtones";
    s[(int)Str::PatchHighlights] = "Highlights";
    s[(int)Str::PatchResetAdjustments] = "Reset adjustments";
    s[(int)Str::PatchPaletteColors] = "Palette colors";
    s[(int)Str::PatchPaletteOriginal] = "Original colors";
    s[(int)Str::PatchAdjustmentsNote] = "Adjustments and palette reduction can lose colors. The original image remains unchanged.";
    s[(int)Str::PatchPasteImage] = "Image";
    s[(int)Str::PatchPastePlacement] = "Placement and resampling";
    s[(int)Str::PatchPasteAdjustments] = "Corrections and palette";
    s[(int)Str::PatchPasteAnimation] = "Animations and screen enable";
    s[(int)Str::PatchPasteHint] = "1–4 toggle sections · Enter inserts · Esc cancels";
    s[(int)Str::FileProgressCancelled] = "File operation cancelled.";
    s[(int)Str::FileProgressFailed] = "File operation failed: %s";
    s[(int)Str::FileTaskOpenProject] = "Open project";
    s[(int)Str::FileTaskOpenFirmware] = "Open firmware";
    s[(int)Str::FileTaskSaveProject] = "Save project";
    s[(int)Str::FileTaskExportFirmware] = "Export firmware";
    s[(int)Str::FileTaskImportImages] = "Import images";
    s[(int)Str::FileTaskExportImages] = "Export images";
    s[(int)Str::FileTaskPreparing] = "Preparing...";
    s[(int)Str::FileTaskCancelling] = "Cancelling...";
    s[(int)Str::FileTaskComplete] = "Completed successfully";
    s[(int)Str::FileTaskCancelled] = "Cancelled";
    s[(int)Str::FileTaskFailed] = "Operation failed";
    s[(int)Str::FileTaskClosing] = "Closing in %.0f s - click to keep open";
    s[(int)Str::FileTaskPinned] = "Automatic closing paused";
    s[(int)Str::FileTaskClose] = "Close";
    s[(int)Str::FileTaskCopyLog] = "Copy log";
    s[(int)Str::PatchMemoryPending] = "Updating Flash usage...";
    s[(int)Str::PatchMemoryUnavailable] = "Flash accounting is available for DRAG S Score / Factory";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Resources";
    s[(int)Str::TabFontGenerator] = "Font Generator";
    s[(int)Str::TabFirmwareInfo] = "About firmware";
    s[(int)Str::TabFirmwareComparison] = "Comparison";
    s[(int)Str::CompareModeTwoFiles] = "Two files";
    s[(int)Str::CompareModeEditor] = "Editor and file";
    s[(int)Str::CompareLeft] = "Left";
    s[(int)Str::CompareRight] = "Right";
    s[(int)Str::CompareChooseFile] = "Choose file...";
    s[(int)Str::CompareReadOnlyNote] = "Read-only comparison; editor changes are included.";
    s[(int)Str::CompareChooseSources] = "Choose both sources to compare.";
    s[(int)Str::CompareEditorUnavailable] = "No firmware open in the editor.";
    s[(int)Str::CompareChanged] = "Changed";
    s[(int)Str::CompareAdded] = "Added";
    s[(int)Str::CompareRemoved] = "Removed";
    s[(int)Str::CompareEqual] = "Equal";
    s[(int)Str::CompareIncompatible] = "Incompatible";
    s[(int)Str::CompareDecodeError] = "Decode error";
    s[(int)Str::CompareStatusFilter] = "Status";
    s[(int)Str::CompareDifferencePreview] = "Difference";
    s[(int)Str::CompareHighlightPixels] = "Highlight differences";
    s[(int)Str::CompareDifferentPixels] = "%zu / %zu pixels differ";
    s[(int)Str::CompareMissingResource] = "Resource absent";
    s[(int)Str::CompareIncompatibleHint] = "Different resource types or dimensions: pixel comparison unavailable.";
    s[(int)Str::CompareImageUnavailable] = "Image unavailable";
    s[(int)Str::CompareTextureError] = "Could not create preview texture.";
    s[(int)Str::CompareAllStatuses] = "All statuses";
    s[(int)Str::TabFirmwareEmulator] = "Emulator";
    s[(int)Str::TabFirmwareLayout] = "UI layout";
    s[(int)Str::LayoutUnsupported] = "UI layout editing is unavailable for this firmware.";
    s[(int)Str::LayoutNativeNote] = "Drag an element or enter X/Y. The preview executes patched firmware; edits apply to the selected hardware revision.";
    s[(int)Str::LayoutRevision] = "Hardware revision";
    s[(int)Str::LayoutCheckingRevisions] = "Checking identical interfaces…";
    s[(int)Str::LayoutAllRevisions] = "All revisions";
    s[(int)Str::LayoutRevisionGroup] = "Identical UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Revision: %s";
    s[(int)Str::LayoutResetScope] = "Reset selected revisions";
    s[(int)Str::LayoutSelectionUnavailable] = "No matching element across the selected revisions.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI elements";
    s[(int)Str::LayoutNoElements] = "No movable blocks on this screen.";
    s[(int)Str::LayoutSharedBlock] = "This block shares its coordinates with other screens.";
    s[(int)Str::LayoutPositionX] = "X (pixels)";
    s[(int)Str::LayoutPositionY] = "Y (pixels)";
    s[(int)Str::LayoutOffset] = "Offset: %+d, %+d px";
    s[(int)Str::LayoutGridStep] = "Step: %d × %d px";
    s[(int)Str::LayoutGridSpacing] = "Grid spacing (px)";
    s[(int)Str::LayoutGridConstraint] = "Native coordinate steps and bounds take priority when no grid point is representable.";
    s[(int)Str::LayoutSnapGrid] = "Snap to grid";
    s[(int)Str::LayoutSnapElements] = "Snap to element edges / centers";
    s[(int)Str::LayoutGuides] = "Movement guides";
    s[(int)Str::LayoutAlign] = "Align to canvas";
    s[(int)Str::LayoutAlignLeft] = "Left";
    s[(int)Str::LayoutAlignCenter] = "Center";
    s[(int)Str::LayoutAlignRight] = "Right";
    s[(int)Str::LayoutAlignTop] = "Top";
    s[(int)Str::LayoutAlignMiddle] = "Middle";
    s[(int)Str::LayoutAlignBottom] = "Bottom";
    s[(int)Str::LayoutNudgeHint] = "Arrows: native pixel step; Shift: larger step. Click the canvas to focus.";
    s[(int)Str::LayoutOverlays] = "Element outlines";
    s[(int)Str::LayoutOverlapOverlay] = "Highlight overlaps";
    s[(int)Str::LayoutOverlaps] = "Intersecting elements";
    s[(int)Str::LayoutNoOverlaps] = "No rectangle intersections";
    s[(int)Str::LayoutDrawOrderUnknown] = "Native draw order not verified";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) is drawn later than %s (#%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Main redraw call order, not permanent layers. Live updates may differ; only pixels actually written cover earlier content.";
    s[(int)Str::LayoutDrawSequence] = "Native draw sequence";
    s[(int)Str::LayoutDrawInterleaved] = "Calls are interleaved; overlap depends on the part of each element.";
    s[(int)Str::EmulatorBefore] = "Before (opened firmware)";
    s[(int)Str::EmulatorAfter] = "After (staged changes)";
    s[(int)Str::EmulatorSideBySide] = "Side by side";
    s[(int)Str::EmulatorOriginalError] = "Opened firmware preview failed: %s";
    s[(int)Str::MenuSaveProject] = "Save project";
    s[(int)Str::MenuSaveProjectAs] = "Save project as...";
    s[(int)Str::MsgProjectSaved] = "Project saved: %s";
    s[(int)Str::MenuBuild] = "Export firmware";
    s[(int)Str::BtnOpenFirmware] = "Open firmware or project...";
    s[(int)Str::StartupOpenFile] = "Open firmware or project...";
    s[(int)Str::UnsavedPrompt] = "Save the project before continuing?";
    s[(int)Str::UnsavedPrompt] = "Save the project before continuing?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Reset element";
    s[(int)Str::LayoutResetRevision] = "Reset this revision's layout";
    s[(int)Str::LayoutResetAll] = "Reset all layout edits";
    s[(int)Str::LayoutPatchSites] = "Patch sites";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Preview state";
    s[(int)Str::LayoutMoveElement] = "Move UI element";
    s[(int)Str::LayoutRba] = "RBA mode";
    s[(int)Str::LayoutTemperatureControl] = "Temperature control";
    s[(int)Str::LayoutTemperature] = "Temperature (°C)";
    s[(int)Str::LayoutRenderFailed] = "Native framebuffer unavailable.";
    s[(int)Str::LayoutElementPower] = "Power";
    s[(int)Str::LayoutElementBattery] = "Battery";
    s[(int)Str::LayoutElementResistance] = "Resistance";
    s[(int)Str::LayoutElementDuration] = "Duration";
    s[(int)Str::LayoutElementPuffs] = "Puffs";
    s[(int)Str::LayoutElementMode] = "Mode";
    s[(int)Str::LayoutElementStatus] = "Status";
    s[(int)Str::LayoutElementScore] = "Score";
    s[(int)Str::LayoutElementStatistics] = "Statistics";
    s[(int)Str::LayoutElementVoltage] = "Voltage";
    s[(int)Str::LayoutElementGraph] = "Graph";
    s[(int)Str::LayoutElementRank] = "Rank";
    s[(int)Str::LayoutElementModeSelection] = "Mode selection";
    s[(int)Str::LayoutElementLogo] = "Logo";
    s[(int)Str::LayoutChangedElements] = "UI elements changed";
    s[(int)Str::EmulatorSupported] = "Native UI preview for all bundled DRAG firmware.";
    s[(int)Str::EmulatorNativeNote] = "Native firmware rendering; current edits included. No electrical hardware emulation.";
    s[(int)Str::EmulatorWatts] = "Power (W)";
    s[(int)Str::EmulatorPuffs] = "Puffs";
    s[(int)Str::EmulatorBattery] = "Battery (0–5)";
    s[(int)Str::EmulatorResistance] = "Resistance (mΩ)";
    s[(int)Str::EmulatorDuration] = "Duration (0.01 s)";
    s[(int)Str::EmulatorLocked] = "Locked";
    s[(int)Str::EmulatorCharging] = "Charging";
    s[(int)Str::EmulatorScore] = "Total score";
    s[(int)Str::EmulatorScoreDisplay] = "Best score";
    s[(int)Str::EmulatorRedraw] = "Redraw";
    s[(int)Str::EmulatorLandscape] = "Landscape";
    s[(int)Str::EmulatorScreen] = "Screen";
    s[(int)Str::EmulatorMainScreen] = "Main screen";
    s[(int)Str::FontSystemFonts] = "System fonts";
    s[(int)Str::FontUserFonts] = "User fonts";
    s[(int)Str::EmulatorMenuItem] = "Menu item";
    s[(int)Str::EmulatorMaterial] = "Material";
    s[(int)Str::EmulatorMenuConfirmed] = "Confirmed";
    s[(int)Str::EmulatorResources] = "Resources read by firmware";
    s[(int)Str::EmulatorNoResources] = "No catalog resource reads recorded.";
    s[(int)Str::EmulatorUidNote] = "Emulated MCU UID; not a connected device ID.";
    s[(int)Str::EmulatorLoaderVersion] = "Virtual loader version";
    s[(int)Str::EmulatorLoaderId] = "Virtual CHIP-ID bytes";
    s[(int)Str::EmulatorLoaderNote] = "Loader data is emulated, not read from firmware or a connected device.";
    s[(int)Str::TabFirmwarePatches] = "Patches";
    s[(int)Str::PatchBootLogo] = "Screen startup";
    s[(int)Str::PatchShutdownLogo] = "Screen shutdown";
    s[(int)Str::PatchDeviceInfo] = "Screen CHIP ID / version";
    s[(int)Str::PatchDeviceInfoNote] = "Static image only. Replaces the service information screen, never the main power/battery screen.";
    s[(int)Str::PatchLogoNote] = "Independent 80 × 160 images for all DRAG S Score / Factory revisions. Not verified on a physical device.";
    s[(int)Str::PatchUnsupported] = "This patch requires DRAG S Score or Factory firmware.";
    s[(int)Str::PatchEnabled] = "Enable custom logo";
    s[(int)Str::PatchCanvas] = "Composition · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Native screen preview";
    s[(int)Str::PatchPreviewNote] = "The native preview shows applied settings, not the uncommitted composition.";
    s[(int)Str::PatchFit] = "Fit";
    s[(int)Str::PatchCover] = "Cover";
    s[(int)Str::PatchStretch] = "Stretch";
    s[(int)Str::PatchScale] = "Scale";
    s[(int)Str::PatchAnimation] = "Entrance animation";
    s[(int)Str::PatchNone] = "None";
    s[(int)Str::PatchFade] = "Fade";
    s[(int)Str::PatchDuration] = "Duration (ms)";
    s[(int)Str::PatchAutoHardware] = "Automatic";
    s[(int)Str::PatchAnimationPhase] = "Animation phase";
    s[(int)Str::PatchPlay] = "Play";
    s[(int)Str::PatchBootStages] = "Replace startup stage";
    s[(int)Str::PatchFirstStage] = "First · VOOPOO animation";
    s[(int)Str::PatchSecondStage] = "Second · device name";
    s[(int)Str::PatchBothStages] = "Both";
    s[(int)Str::PatchExitAnimation] = "Exit animation";
    s[(int)Str::PatchHold] = "Hold (ms)";
    s[(int)Str::PatchResampling] = "Resampling";
    s[(int)Str::PatchNearest] = "Nearest · pixel art";
    s[(int)Str::PatchBilinear] = "Bilinear";
    s[(int)Str::PatchBicubic] = "Bicubic";
    s[(int)Str::PatchLanczos] = "Lanczos · photos";
    s[(int)Str::PatchIris] = "Circle";
    s[(int)Str::PatchDiamond] = "Diamond";
    s[(int)Str::PatchCenterHorizontal] = "Center / edges · horizontal";
    s[(int)Str::PatchCenterVertical] = "Center / edges · vertical";
    s[(int)Str::PatchBlinds] = "Blinds";
    s[(int)Str::PatchDissolve] = "Dissolve";
    s[(int)Str::PatchImportImage] = "Open image...";
    s[(int)Str::PatchImageFormats] = "PNG, JPEG, BMP, GIF, TIFF, ICO, TGA, PSD, HDR, PNM. WebP / HEIF require a Windows codec. Animated files: first frame only.";
    s[(int)Str::PatchPlacement] = "Image placement";
    s[(int)Str::EmulatorModeScreens] = "Screens";
    s[(int)Str::EmulatorModeDevice] = "Device";
    s[(int)Str::DeviceUnsupported] = "Device mode supports DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Firmware runs from Reset with virtual peripherals. Heating is simulated; this does not verify electrical safety.";
    s[(int)Str::DeviceReset] = "Restart";
    s[(int)Str::DevicePause] = "Pause";
    s[(int)Str::DeviceResume] = "Run";
    s[(int)Str::DeviceStep] = "Step";
    s[(int)Str::DeviceChanged] = "The running image does not include current edits.";
    s[(int)Str::DeviceApply] = "Restart with edits";
    s[(int)Str::DeviceButtons] = "Hold buttons or use Space / Up / Down.";
    s[(int)Str::DeviceAtomizerConnected] = "Coil connected";
    s[(int)Str::DeviceTime] = "Virtual time";
    s[(int)Str::DeviceSleeping] = "CPU sleeping";
    s[(int)Str::DeviceCpuRunning] = "CPU running";
    s[(int)Str::DeviceLoading] = "Loading device...";
    s[(int)Str::DeviceLog] = "Peripheral access";
    s[(int)Str::DeviceDiagnostics] = "Diagnostics";
    s[(int)Str::DeviceCpuRegisters] = "CPU registers";
    s[(int)Str::DeviceModeledClock] = "Modeled HCLK";
    s[(int)Str::DeviceClockNote] = "Model clock, not measured hardware or host speed.";
    s[(int)Str::DeviceInstructions] = "Instructions";
    s[(int)Str::DeviceMappedSram] = "Mapped SRAM";
    s[(int)Str::DeviceStack] = "Stack at SP";
    s[(int)Str::DeviceUnavailable] = "Unavailable";
    s[(int)Str::DeviceStackEmpty] = "SP is at the end of mapped SRAM.";
    s[(int)Str::DeviceStackInvalid] = "SP is unaligned or outside mapped SRAM.";
    s[(int)Str::DeviceStackReadFailed] = "Cannot read stack from mapped SRAM.";
    s[(int)Str::DeviceStackNote] = "Up to 32 words; raw memory, not an unwound call stack.";
    s[(int)Str::DeviceValue] = "Value";
    s[(int)Str::DeviceAddress] = "Address";
    s[(int)Str::DeviceBytes] = "Bytes";
    s[(int)Str::BadgeNoFirmware] = "| [No Firmware Opened - Drag & Drop .zwdf/.xwdf here]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu items | %zu records";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Open Firmware...";
    s[(int)Str::BtnExportAll] = "Export All Resources to Folder...";
    s[(int)Str::BtnImportFolder] = "Import Folder of PNGs...";
    s[(int)Str::BtnBuildFirmware] = " BUILD NEW FIRMWARE ";
    s[(int)Str::BtnRevertAllStaged] = "Discard All Changes";
    s[(int)Str::GroupAll] = "All Groups";
    s[(int)Str::GroupAlerts] = "Alerts (Dialogs & Icons)";
    s[(int)Str::GroupFonts] = "Fonts (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Score & Ranks";
    s[(int)Str::GroupStatusLabels] = "Status & UI Labels";
    s[(int)Str::GroupControls] = "Controls & Battery";
    s[(int)Str::GroupScreens] = "Screens & Boot Logos";
    s[(int)Str::GroupAnimations] = "Animations";
    s[(int)Str::PanelResourcesHeader] = "Resources";
    s[(int)Str::LabelSearch] = "Search";
    s[(int)Str::SearchPlaceholder] = "filter by title, id, address...";
    s[(int)Str::NoFirmwareLoadedList] = "No firmware loaded.\nFile > New / Open";
    s[(int)Str::NoResourcesMatchFilter] = "No resources match search filter.";
    s[(int)Str::PanelResourcePreviewHeader] = "Preview";
    s[(int)Str::SelectResourcePrompt] = "Select a resource from the list on the left to preview and edit.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Group";
    s[(int)Str::LabelColor] = "Color";
    s[(int)Str::LabelColorCustom] = "Custom";
    s[(int)Str::LabelFlashAddress] = "Flash Address";
    s[(int)Str::LabelDimensions] = "Dimensions";
    s[(int)Str::LabelBytes] = "Bytes";
    s[(int)Str::LabelRecords] = "Records";
    s[(int)Str::StatusModifiedStaged] = "Status: MODIFIED (staged in memory)";
    s[(int)Str::StatusOriginal] = "Status: ORIGINAL (from firmware)";
    s[(int)Str::StatusStockOriginal] = "Stock (Matches Reference)";
    s[(int)Str::StatusCustomDiffers] = "Custom (Differs from Reference)";
    s[(int)Str::BtnRestoreStock] = "Restore Factory Stock";
    s[(int)Str::LabelCompareWithStock] = "Compare with Stock Reference";
    s[(int)Str::LabelActivePreview] = "Current Active:";
    s[(int)Str::LabelStockReferencePreview] = "Factory Stock Reference:";
    s[(int)Str::FilterOnlyCustom] = "Only Custom / Non-Stock";
    s[(int)Str::MsgRestoredToStock] = "Restored %s to factory stock reference.";
    s[(int)Str::BtnReplaceFromPng] = "Replace from PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Restore Original";
    s[(int)Str::BtnExportThisPng] = "Export This PNG...";
    s[(int)Str::BtnExportHorizontal] = "Export Horizontal (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Color Palette & Renderer (RGB565)";
    s[(int)Str::LabelTheme] = "Theme";
    s[(int)Str::BtnApply] = "Apply";
    s[(int)Str::BtnResetAll] = "Reset All";
    s[(int)Str::LabelRole] = "Role";
    s[(int)Str::BtnResetRole] = "Reset Role";
    s[(int)Str::PromptLoadFwColors] = "Load firmware to inspect & patch colors.";
    s[(int)Str::LabelPreviewTint] = "Preview Tint";
    s[(int)Str::InfoDigitsMonochrome] = "Digits are monochrome masks. Their display color is set by firmware code, not stored in the glyph.";
    s[(int)Str::InfoPreviewNotSaved] = "Preview tint is not saved to firmware. RGB565 PNG export includes this tint.";
    s[(int)Str::LabelPreviewOnly] = "Preview only:";
    s[(int)Str::LabelPresets] = "Presets...";
    s[(int)Str::BtnResetColor] = "Reset Color";
    s[(int)Str::LabelZoom] = "Preview Zoom";
    s[(int)Str::LabelPixelGrid] = "Pixel Grid";
    s[(int)Str::LabelHorizontalPreview] = "Horizontal Readable Preview (136x22):";
    s[(int)Str::FontSec1Title] = "1. Font Selection";
    s[(int)Str::FontSec1WindowsFont] = "Windows Font";
    s[(int)Str::FontSec1BrowseTtf] = "Browse TTF/OTF File...";
    s[(int)Str::FontSec1ActiveFont] = "Active Font: %s";
    s[(int)Str::FontSec2Title] = "2. Display Preset";
    s[(int)Str::FontSec3Title] = "3. Render Parameters";
    s[(int)Str::FontSec3Threshold] = "Threshold";
    s[(int)Str::FontSec3ThresholdHint] = "Lower = bolder/thicker, Higher = thinner";
    s[(int)Str::FontSec3Stretch] = "Stretch to fill cell";
    s[(int)Str::FontSec3Baseline] = "Baseline Alignment";
    s[(int)Str::FontSec3CenterV] = "Center Vertically";
    s[(int)Str::FontSec3PaddingX] = "Padding X";
    s[(int)Str::FontSec3PaddingY] = "Padding Y";
    s[(int)Str::FontSec3Oversampling] = "Oversampling";
    s[(int)Str::FontSec4Title] = "4. Firmware Integration";
    s[(int)Str::FontSec4ApplyPreset] = " > APPLY GLYPHS TO OPENED FIRMWARE < ";
    s[(int)Str::FontSec4ApplyAll] = "Apply all fonts";
    s[(int)Str::FontSec5Title] = "5. Export to PNGs";
    s[(int)Str::FontSec5OutputFolder] = "Output Folder";
    s[(int)Str::FontSec5BrowseFolder] = "Browse Folder...";
    s[(int)Str::FontSec5OpenExplorer] = "Open in Explorer";
    s[(int)Str::FontSec5ExportAllPreset] = "Export all presets";
    s[(int)Str::FontSec5ExportButton] = "Export 1-Bit PNGs to Disk";
    s[(int)Str::FontRightLivePreview] = "Live Font Preview";
    s[(int)Str::FontRightNoGlyphs] = "No glyphs loaded.";
    s[(int)Str::FontPresetInfo] = "Preset: %s (%dx%d px, %d glyphs, size: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Package Specification";
    s[(int)Str::FwInfoNoFirmware] = "No firmware loaded. Open a .zwdf/.xwdf firmware file in the Resource Studio tab.";
    s[(int)Str::FwInfoFilePath] = "File Path: %s";
    s[(int)Str::FwInfoSha256] = "Source SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Package Size: %zu bytes";
    s[(int)Str::FwInfoHwRevisions] = "Hardware Revisions: %zu records";
    s[(int)Str::FwInfoRecognizedResources] = "Recognized Firmware Resources: %zu items (Fonts, Icons, Ranks, Labels, Controls, Screens)";
    s[(int)Str::FwInfoRecordsTable] = "Hardware Records Table";
    s[(int)Str::FwInfoColRecord] = "Record #";
    s[(int)Str::FwInfoColDevCode] = "Device Code";
    s[(int)Str::FwInfoColPayload] = "Payload Size";
    s[(int)Str::FwInfoColImage] = "Image Size";
    s[(int)Str::FwInfoColFlashBase] = "Flash Base";
    s[(int)Str::FwInfoColMSP] = "Stack Pointer (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Reset Vector";
    s[(int)Str::BtnRevertAllModifications] = "Revert All Modifications";
    s[(int)Str::MsgReadyOpenFw] = "Ready. Open a .zwdf/.xwdf firmware file or generate custom fonts.";
    s[(int)Str::MsgSuccessOpened] = "Opened: %s";
    s[(int)Str::MsgOpenError] = "Open error: %s";
    s[(int)Str::MsgAllReverted] = "All staged replacements reverted to original firmware state.";
    s[(int)Str::MsgExportSuccess] = "Exported %d assets to: %s";
    s[(int)Str::MsgExportError] = "Export error: %s";
    s[(int)Str::MsgImportSuccess] = "Imported %d PNGs from folder.";
    s[(int)Str::MsgImportError] = "Import error: %s";
    s[(int)Str::MsgBuildSuccess] = "Build SUCCESS: %s";
    s[(int)Str::MsgBuildError] = "Build error: %s";
    s[(int)Str::MsgFontNotLoaded] = "Error: No font loaded in generator!";
    s[(int)Str::MsgFontInjectedPreset] = "Directly injected %d glyphs of %s into firmware memory!";
    s[(int)Str::MsgFontInjectedAll] = "Applied %d glyphs.";
    s[(int)Str::MsgFontExportSuccess] = "Successfully exported %d PNG files to: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "Successfully exported %d files for %s to: %s";
    s[(int)Str::MsgFontExportError] = "Export error: %s";
    s[(int)Str::ModalBuildReportTitle] = "Build Report";
    s[(int)Str::ModalBuildSuccessBanner] = "FIRMWARE BUILD & VERIFICATION SUCCESSFUL!";
    s[(int)Str::ModalSavedFile] = "Saved file: %s";
    s[(int)Str::ModalOutputSize] = "Output size: %zu bytes";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Modified bitmap resources: %zu";
    s[(int)Str::ModalModColorGroups] = "Modified color patch groups: %zu";
    s[(int)Str::ModalPatchedRanges] = "Patched byte ranges: %zu across %zu hardware records";
    s[(int)Str::ModalUntouchedBytes] = "Verified untouched bytes: %zu bytes identical to original";
    s[(int)Str::ModalReportNote] = "A detailed verification report was saved as .report.json beside the firmware.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Open in Explorer";
    s[(int)Str::BtnCopyClipboard] = "Copy (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Paste (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Undo";
    s[(int)Str::BtnRedo] = "Redo";
    s[(int)Str::TooltipUndo] = "Undo: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Redo: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Nothing to undo (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Nothing to redo (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Pixel Tools:";
    s[(int)Str::ToolPencil] = "Pencil";
    s[(int)Str::ToolEraser] = "Eraser";
    s[(int)Str::BtnInvertPixels] = "Invert";
    s[(int)Str::BtnClearPixels] = "Clear";
    s[(int)Str::BtnFlipH] = "Flip H";
    s[(int)Str::BtnFlipV] = "Flip V";
    s[(int)Str::MsgCopiedToClipboard] = "Copied image of %s to clipboard.";
    s[(int)Str::MsgPastedFromClipboard] = "Pasted image from clipboard for %s!";
    s[(int)Str::MsgClipboardNoImage] = "No compatible bitmap image found in clipboard.";
    s[(int)Str::MsgUndoPerformed] = "Undone: %s";
    s[(int)Str::MsgRedoPerformed] = "Redone: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Firmware: %s (SHA256: %s... | %zu records | %zu staged)";
    s[(int)Str::BadgeProjectSummary] = "| Project: %s (SHA256: %s... | %zu records | %zu staged)";
    s[(int)Str::LabelCategory] = "Category";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelPreviewRank] = "Preview rank";
    s[(int)Str::LabelPreviewCyanMode] = "Preview: cyan mode (RBA)";
    s[(int)Str::LabelPreviewBackground] = "Firmware background: %s";
    s[(int)Str::LabelBrushColor] = "Brush color (RGB565 pixels)";
    s[(int)Str::InfoNativeRgb565] = "Colors are stored in the bitmap. The brush changes actual pixels.";
    s[(int)Str::InfoFirmwareColors] = "Preview uses firmware foreground and background colors. Change theme colors in the palette.";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Group: %s  |  Color: Custom (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Group: %s  |  Color: %s";
    s[(int)Str::LabelMetaHardware] = "Flash Address: 0x%08X  |  Dimensions: %u x %u px  |  Bytes: %zu  |  Records: %zu";
    s[(int)Str::LabelPixelCoord] = "Pixel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| LMB: %s  |  RMB: Erase";
    s[(int)Str::LabelDrawAction] = "Draw";
    s[(int)Str::LabelEraseAction] = "Erase";
    s[(int)Str::LabelFontPreset] = "Preset";
    s[(int)Str::LabelFontPixelGrid] = "Pixel Grid";
    s[(int)Str::MsgAppliedColorTheme] = "Applied color theme: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "Imported %zu matching PNGs from dropped directory.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Opened firmware via Drag & Drop: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop error: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Loaded font via Drag & Drop: %s";
    s[(int)Str::MsgFailedLoadFont] = "Failed to load dropped font file: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Dropped PNG matches original for %s (unmodified).";
    s[(int)Str::MsgStagedReplacement] = "Staged custom replacement for %s from dropped PNG!";
    s[(int)Str::MsgPngLoadError] = "PNG Load error: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Replaced %s matched original (unmodified).";
    s[(int)Str::MsgStagedReplacementCustom] = "Staged custom replacement for %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "Staged custom RGB565 PNG replacement for %s!";
    s[(int)Str::MsgRestoredOriginal] = "Restored %s to firmware original.";
    s[(int)Str::MsgSavedAssetPng] = "Saved %s to: %s";
    s[(int)Str::MsgSaveError] = "Save error: %s";
    s[(int)Str::MsgSavedBootLogo] = "Saved horizontal boot logo to: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Error: Refusing to overwrite source firmware! Choose a new filename.";
    s[(int)Str::MsgColorPatchError] = "Color patch error: %s";
    s[(int)Str::MsgPackError] = "Pack error: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Imported %zu changed PNGs (%zu matched original)";

    s[(int)Str::LabelBuiltin] = "built-in";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Cannot load built-in firmware: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Loaded built-in firmware: %s";
    s[(int)Str::ThemeFactoryDefault] = "Factory Default";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Original Voopoo Red & Yellow factory style";
    s[(int)Str::ThemeCyberpunkNeon] = "Cyberpunk Neon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Electric Cyan, Neon Pink, and Vivid Violet";
    s[(int)Str::ThemeMatrixGreen] = "Matrix Green";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT Terminal Phosphor & Emerald Green";
    s[(int)Str::ThemeArcticIceBlue] = "Arctic Ice Blue";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Glacier Cyan, Deep Sky, and Azure Glow";
    s[(int)Str::ThemeAmberFire] = "Amber Fire";
    s[(int)Str::ThemeAmberFireDesc] = "Molten Lava Orange, Golden Flame, and Ember";
    s[(int)Str::ThemePurpleNebula] = "Purple Nebula";
    s[(int)Str::ThemePurpleNebulaDesc] = "Deep Violet, Electric Lavender, and Neon Amethyst";
    s[(int)Str::ThemeStealthDarkGray] = "Stealth Dark Gray";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Matte Charcoal, Slate Graphite, and Gunmetal";
    s[(int)Str::RoleAccentRed] = "Accent / red mode";
    s[(int)Str::RoleAccentRedScope] = "Main UI accent in standard modes - 8 sites";
    s[(int)Str::RoleAccentCyan] = "Accent / cyan mode";
    s[(int)Str::RoleAccentCyanScope] = "Main UI accent in SMART/RBA states - 7 sites";
    s[(int)Str::RoleHighlightYellow] = "Highlight / yellow";
    s[(int)Str::RoleHighlightYellowScope] = "Score strip and service panels - 4 sites";
    s[(int)Str::RoleRankBronze] = "Rank / bronze";
    s[(int)Str::RoleRankBronzeScope] = "Score rank bronze palette";
    s[(int)Str::RoleRankGold] = "Rank / gold";
    s[(int)Str::RoleRankGoldScope] = "Score rank gold palette";
    s[(int)Str::RoleRankIce] = "Rank / ice";
    s[(int)Str::RoleRankIceScope] = "Score rank ice palette";
    s[(int)Str::RoleRankCyan] = "Rank / cyan";
    s[(int)Str::RoleRankCyanScope] = "Score rank cyan palette";
    s[(int)Str::RoleRankOrange] = "Rank / orange";
    s[(int)Str::RoleRankOrangeScope] = "Score rank orange palette";
    s[(int)Str::RoleRankRed] = "Rank / red";
    s[(int)Str::RoleRankRedScope] = "Score rank red palette";
    s[(int)Str::RoleDigitsWattage] = "Wattage digits";
    s[(int)Str::RoleDigitsWattageScope] = "Large wattage readout on main screen (19x40)";
    s[(int)Str::RoleDigitsStats] = "Stats digits";
    s[(int)Str::RoleDigitsStatsScope] = "Puff counter digits (11x20); background follows the mode accent";
    s[(int)Str::ColorPresetWhite] = "White (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Main / Red (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Cyan (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Service / Yellow (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bronze (#B57947)";
    s[(int)Str::ColorPresetGold] = "Gold (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Orange (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Green (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Main Digits 0-9, 10 files)";
    s[(int)Str::FontPreset11x20] = "11x20 (Secondary Digits 0-9, 10 files)";
    s[(int)Str::FontPreset7x13] = "7x13 (Small Status 0-9, A-F, 16 files)";
    s[(int)Str::FontPreset6x10] = "6x10 (Micro Status 0-9, 10 files)";
    s[(int)Str::FontOs1x] = "1x (Direct)";
    s[(int)Str::FontOs2x] = "2x (Fast)";
    s[(int)Str::FontOs4x] = "4x (Recommended)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
    s[(int)Str::ToolEyedropper] = "Eyedropper";
    s[(int)Str::ToolBucketFill] = "Fill";
    s[(int)Str::BtnInvertCanvas] = "Invert";
    s[(int)Str::BtnClearCanvas] = "Clear";
    s[(int)Str::LabelBrushSize] = "Size:";
}

static void InitRussian() {
    auto& s = g_table[(int)Language::Russian];
    s[(int)Str::MenuFile] = "Файл";
    s[(int)Str::MenuEdit] = "Правка";
    s[(int)Str::MenuLanguage] = "Язык";
    s[(int)Str::MenuHelp] = "Справка";
    s[(int)Str::UpdateTitle] = "Обновление студии";
    s[(int)Str::UpdateVersion] = "Установленная версия: %s";
    s[(int)Str::UpdateCheck] = "Проверить обновления";
    s[(int)Str::UpdateChecking] = "Проверка GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Установлена последняя версия.";
    s[(int)Str::UpdateDownload] = "Скачать";
    s[(int)Str::UpdateDownloading] = "Скачивание и проверка...";
    s[(int)Str::UpdateInstall] = "Установить и перезапустить";
    s[(int)Str::UpdateReady] = "Готово к установке.";
    s[(int)Str::UpdateChangelog] = "Изменения";
    s[(int)Str::UpdateFailed] = "Ошибка обновления";
    s[(int)Str::UpdateAvailable] = "Доступна версия: %s";
    s[(int)Str::MenuCreate] = "Создать...";
    s[(int)Str::MenuOpen] = "Открыть...";
    s[(int)Str::MenuImport] = "Импорт ресурсов...";
    s[(int)Str::MenuExport] = "Экспорт ресурсов...";
    s[(int)Str::MenuBuild] = "Собрать прошивку...";
    s[(int)Str::MenuExit] = "Выход";
    s[(int)Str::StartupChooseFirmware] = "Выберите прошивку";
    s[(int)Str::StartupOpenFile] = "Открыть файл с ПК...";
    s[(int)Str::StartupOpenBuiltin] = "Создать";
    s[(int)Str::StartupNoProfiles] = "Встроенные прошивки недоступны. Откройте файл с ПК.";
    s[(int)Str::StartupImageUnavailable] = "Нет изображения";
    s[(int)Str::SupportFull] = "Полностью";
    s[(int)Str::SupportNone] = "Не поддерживается";
    s[(int)Str::SupportPalette] = "Цветовая палитра";
    s[(int)Str::SupportMonochrome] = "Монохромный экран; цветовой палитры нет.";
    s[(int)Str::EmulatorHistoryDay] = "День истории";
    s[(int)Str::EmulatorClockHour] = "Часы";
    s[(int)Str::EmulatorClockMinute] = "Минуты";
    s[(int)Str::EmulatorClockField] = "Выбранное поле";
    s[(int)Str::EditionFactory] = "Заводская";
    s[(int)Str::EditionStandard] = "Стандартная";
    s[(int)Str::LabelModified] = "* Изменено (не сохранено)";
    s[(int)Str::UnsavedTitle] = "Несохранённые изменения прошивки";
    s[(int)Str::UnsavedPrompt] = "Сохранить изменения прошивки перед продолжением?";
    s[(int)Str::BtnSaveChanges] = "Сохранить";
    s[(int)Str::BtnDiscardChanges] = "Не сохранять";
    s[(int)Str::BtnCancelAction] = "Отмена";
    s[(int)Str::MsgFirmwareOnlySaved] = "Прошивка сохранена, но не удалось сохранить отчёт: %s";
    s[(int)Str::SaveIdentityError] = "Не удалось проверить путь, либо он указывает на защищённый исходный или выходной файл";
    s[(int)Str::SaveTemporaryError] = "Не удалось создать временный файл рядом с целевым";
    s[(int)Str::SaveWriteError] = "Не удалось полностью записать временный файл";
    s[(int)Str::SaveFlushError] = "Не удалось сбросить временный файл на диск";
    s[(int)Str::SaveCloseError] = "Не удалось закрыть временный файл";
    s[(int)Str::SaveBackupError] = "Не удалось создать резервную копию; целевой файл не заменён";
    s[(int)Str::SaveCommitError] = "Не удалось атомарно заменить целевой файл; он не заменён";
    s[(int)Str::MsgWindowsErrorCode] = "%s (ошибка Windows %lu)";
    s[(int)Str::CompareSourceOpened] = "Текущий редактор (с несохранёнными правками)";
    s[(int)Str::CompareSourceReference] = "Соответствующая заводская прошивка";
    s[(int)Str::CompareSourceFile] = "Файл прошивки или проекта";
    s[(int)Str::PatchPreview] = "Предпросмотр · черновик на нативной прошивке";
    s[(int)Str::PatchTransfer] = "Копирование между разделами";
    s[(int)Str::PatchCopyAll] = "Настройки и изображение";
    s[(int)Str::PatchCopySettings] = "Только настройки";
    s[(int)Str::PatchCopyImage] = "Только изображение";
    s[(int)Str::PatchClipboardError] = "В буфере обмена нет совместимых данных патча.";
    s[(int)Str::PatchChecking] = "Проверка, помещается ли черновик в прошивку...";
    s[(int)Str::PatchBuildError] = "Не помещается в прошивку: %s";
    s[(int)Str::RiskTitle] = "ВНИМАНИЕ: ПРОВЕРЕНО ТОЛЬКО В ЭМУЛЯТОРЕ";
    s[(int)Str::RiskBody] = "Патчи кода и изменения расположения UI проверены только во встроенном эмуляторе. На реальном железе они НЕ проверялись.";
    s[(int)Str::RiskScope] = "Прошивка таких изменений — на свой страх и риск: устройство может работать некорректно или не загрузиться.";
    s[(int)Str::RiskWait] = "Прочитайте предупреждение (%d с)";
    s[(int)Str::RiskAccept] = "Понимаю риск, продолжить";
    s[(int)Str::RiskSaveError] = "Не удалось сохранить согласие в конфиг.";
    s[(int)Str::PatchCustomScreens] = "Кастомные экраны";
    s[(int)Str::PatchMemoryTitle] = "Flash приложения · черновик · загрузчик и настройки исключены";
    s[(int)Str::PatchMemoryUsed] = "Занято";
    s[(int)Str::PatchMemoryFree] = "Свободно";
    s[(int)Str::PatchMemoryPatches] = "Патчи";
    s[(int)Str::PatchMemoryBytes] = "%zu Б";
    s[(int)Str::PatchMemorySignedBytes] = "%+lld Б";
    s[(int)Str::PatchMemoryDeltaNote] = "Δ к исходнику; общие данные учтены по порядку экранов.";
    s[(int)Str::PatchMemoryBoot] = "Δ Запуск";
    s[(int)Str::PatchMemoryShutdown] = "Δ Выключение";
    s[(int)Str::PatchMemoryDeviceInfo] = "Δ CHIP ID/версия";
    s[(int)Str::PatchMemoryLayout] = "Δ Расположение UI";
    s[(int)Str::PatchSourceError] = "Нельзя сохранить исходник или размещение: максимум 16384 пикселя по стороне и 64 МиБ пикселей RGBA.";
    s[(int)Str::PatchPreviousFrame] = "Предыдущий кадр";
    s[(int)Str::PatchNextFrame] = "Следующий кадр";
    s[(int)Str::PatchStop] = "Стоп";
    s[(int)Str::PatchLoop] = "Повтор";
    s[(int)Str::PatchPlaybackSpeed] = "Скорость воспроизведения";
    s[(int)Str::PatchPlaybackTime] = "%.2f / %.2f с · кадр %d/%d";
    s[(int)Str::PatchPlaybackPreparing] = "Подготовка кадров...";
    s[(int)Str::PatchPlaybackReady] = "Готово";
    s[(int)Str::PatchSoftWipe] = "Мягкая шторка";
    s[(int)Str::PatchChecker] = "Шахматное раскрытие";
    s[(int)Str::PatchAdjustments] = "Коррекция изображения";
    s[(int)Str::PatchBrightness] = "Яркость";
    s[(int)Str::PatchContrast] = "Контраст";
    s[(int)Str::PatchGamma] = "Гамма";
    s[(int)Str::PatchSaturation] = "Насыщенность";
    s[(int)Str::PatchSharpness] = "Резкость";
    s[(int)Str::PatchToneCurve] = "Тоновая кривая";
    s[(int)Str::PatchShadows] = "Тени";
    s[(int)Str::PatchMidtones] = "Средние тона";
    s[(int)Str::PatchHighlights] = "Светлые тона";
    s[(int)Str::PatchResetAdjustments] = "Сбросить коррекцию";
    s[(int)Str::PatchPaletteColors] = "Цветов в палитре";
    s[(int)Str::PatchPaletteOriginal] = "Исходные цвета";
    s[(int)Str::PatchAdjustmentsNote] = "Коррекция и сокращение палитры могут терять цвета. Исходное изображение не изменяется.";
    s[(int)Str::PatchPasteImage] = "Изображение";
    s[(int)Str::PatchPastePlacement] = "Размещение и интерполяция";
    s[(int)Str::PatchPasteAdjustments] = "Коррекция и палитра";
    s[(int)Str::PatchPasteAnimation] = "Анимации и включение экрана";
    s[(int)Str::PatchPasteHint] = "1–4 — переключение разделов · Enter — вставить · Esc — отмена";
    s[(int)Str::FileProgressCancelled] = "Файловая операция отменена.";
    s[(int)Str::FileProgressFailed] = "Ошибка файловой операции: %s";
    s[(int)Str::FileTaskOpenProject] = "Открытие проекта";
    s[(int)Str::FileTaskOpenFirmware] = "Открытие прошивки";
    s[(int)Str::FileTaskSaveProject] = "Сохранение проекта";
    s[(int)Str::FileTaskExportFirmware] = "Экспорт прошивки";
    s[(int)Str::FileTaskImportImages] = "Импорт изображений";
    s[(int)Str::FileTaskExportImages] = "Экспорт изображений";
    s[(int)Str::FileTaskPreparing] = "Подготовка...";
    s[(int)Str::FileTaskCancelling] = "Отмена...";
    s[(int)Str::FileTaskComplete] = "Операция завершена";
    s[(int)Str::FileTaskCancelled] = "Операция отменена";
    s[(int)Str::FileTaskFailed] = "Ошибка операции";
    s[(int)Str::FileTaskClosing] = "Закрытие через %.0f с — нажмите, чтобы оставить окно";
    s[(int)Str::FileTaskPinned] = "Автоматическое закрытие остановлено";
    s[(int)Str::FileTaskClose] = "Закрыть";
    s[(int)Str::FileTaskCopyLog] = "Копировать лог";
    s[(int)Str::PatchMemoryPending] = "Обновление занятого места...";
    s[(int)Str::PatchMemoryUnavailable] = "Расчёт Flash доступен для DRAG S Score / Factory";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ресурсы";
    s[(int)Str::TabFontGenerator] = "Генератор шрифтов";
    s[(int)Str::TabFirmwareInfo] = "О прошивке";
    s[(int)Str::TabFirmwareComparison] = "Сравнение";
    s[(int)Str::CompareModeTwoFiles] = "Два файла";
    s[(int)Str::CompareModeEditor] = "Редактор и файл";
    s[(int)Str::CompareLeft] = "Слева";
    s[(int)Str::CompareRight] = "Справа";
    s[(int)Str::CompareChooseFile] = "Выбрать файл...";
    s[(int)Str::CompareReadOnlyNote] = "Только просмотр; учитываются правки в редакторе.";
    s[(int)Str::CompareChooseSources] = "Выберите оба источника для сравнения.";
    s[(int)Str::CompareEditorUnavailable] = "В редакторе не открыта прошивка.";
    s[(int)Str::CompareChanged] = "Изменён";
    s[(int)Str::CompareAdded] = "Добавлен";
    s[(int)Str::CompareRemoved] = "Удалён";
    s[(int)Str::CompareEqual] = "Совпадает";
    s[(int)Str::CompareIncompatible] = "Несопоставим";
    s[(int)Str::CompareDecodeError] = "Ошибка декодирования";
    s[(int)Str::CompareStatusFilter] = "Статус";
    s[(int)Str::CompareDifferencePreview] = "Разница";
    s[(int)Str::CompareHighlightPixels] = "Подсветить отличия";
    s[(int)Str::CompareDifferentPixels] = "Различаются %zu / %zu пикселей";
    s[(int)Str::CompareMissingResource] = "Ресурс отсутствует";
    s[(int)Str::CompareIncompatibleHint] = "Разные типы или размеры ресурсов: попиксельное сравнение недоступно.";
    s[(int)Str::CompareImageUnavailable] = "Изображение недоступно";
    s[(int)Str::CompareTextureError] = "Не удалось создать текстуру предпросмотра.";
    s[(int)Str::CompareAllStatuses] = "Все статусы";
    s[(int)Str::TabFirmwareEmulator] = "Эмулятор";
    s[(int)Str::TabFirmwareLayout] = "Расположение UI";
    s[(int)Str::LayoutUnsupported] = "Редактор расположения UI недоступен для этой прошивки.";
    s[(int)Str::LayoutNativeNote] = "Перетащите элемент или введите X/Y. Предпросмотр выполняет изменённую прошивку; правки относятся к выбранной аппаратной ревизии.";
    s[(int)Str::LayoutRevision] = "Аппаратная ревизия";
    s[(int)Str::LayoutCheckingRevisions] = "Проверка одинаковых интерфейсов…";
    s[(int)Str::LayoutAllRevisions] = "Все ревизии";
    s[(int)Str::LayoutRevisionGroup] = "Одинаковый UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Ревизия: %s";
    s[(int)Str::LayoutResetScope] = "Сбросить выбранные ревизии";
    s[(int)Str::LayoutSelectionUnavailable] = "Для выбранных ревизий нет совместимого элемента.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Элементы UI";
    s[(int)Str::LayoutNoElements] = "На этом экране нет перемещаемых блоков.";
    s[(int)Str::LayoutSharedBlock] = "Координаты этого блока общие для нескольких экранов.";
    s[(int)Str::LayoutPositionX] = "X (пиксели)";
    s[(int)Str::LayoutPositionY] = "Y (пиксели)";
    s[(int)Str::LayoutOffset] = "Смещение: %+d, %+d пикс.";
    s[(int)Str::LayoutGridStep] = "Шаг: %d × %d пикс.";
    s[(int)Str::LayoutGridSpacing] = "Шаг сетки (пикс.)";
    s[(int)Str::LayoutGridConstraint] = "Если узлы сетки недоступны, приоритет имеют нативный шаг и границы координат.";
    s[(int)Str::LayoutSnapGrid] = "Привязка к сетке";
    s[(int)Str::LayoutSnapElements] = "Привязка к краям и центрам элементов";
    s[(int)Str::LayoutGuides] = "Направляющие при перемещении";
    s[(int)Str::LayoutAlign] = "Выровнять по экрану";
    s[(int)Str::LayoutAlignLeft] = "Слева";
    s[(int)Str::LayoutAlignCenter] = "По центру X";
    s[(int)Str::LayoutAlignRight] = "Справа";
    s[(int)Str::LayoutAlignTop] = "Сверху";
    s[(int)Str::LayoutAlignMiddle] = "По центру Y";
    s[(int)Str::LayoutAlignBottom] = "Снизу";
    s[(int)Str::LayoutNudgeHint] = "Стрелки: нативный шаг в пикселях; Shift: увеличенный шаг. Нажмите на экран для фокуса.";
    s[(int)Str::LayoutOverlays] = "Контуры элементов";
    s[(int)Str::LayoutOverlapOverlay] = "Подсветка пересечений";
    s[(int)Str::LayoutOverlaps] = "Пересекающиеся элементы";
    s[(int)Str::LayoutNoOverlaps] = "Пересечений прямоугольников нет";
    s[(int)Str::LayoutDrawOrderUnknown] = "Нативный порядок отрисовки не подтверждён";
    s[(int)Str::LayoutDrawOrderPair] = "%s (№%d) рисуется позже %s (№%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Порядок вызовов основной отрисовки, а не постоянные слои. При обновлениях он может отличаться; предыдущие данные перекрывают только реально записанные пиксели.";
    s[(int)Str::LayoutDrawSequence] = "Штатный порядок отрисовки";
    s[(int)Str::LayoutDrawInterleaved] = "Вызовы чередуются: перекрытие зависит от части элемента.";
    s[(int)Str::EmulatorBefore] = "До (открытая прошивка)";
    s[(int)Str::EmulatorAfter] = "После (текущие изменения)";
    s[(int)Str::EmulatorSideBySide] = "Рядом";
    s[(int)Str::EmulatorOriginalError] = "Ошибка предпросмотра открытой прошивки: %s";
    s[(int)Str::MenuSaveProject] = "Сохранить проект";
    s[(int)Str::MenuSaveProjectAs] = "Сохранить проект как...";
    s[(int)Str::MsgProjectSaved] = "Проект сохранён: %s";
    s[(int)Str::MenuBuild] = "Экспорт прошивки";
    s[(int)Str::BtnOpenFirmware] = "Открыть прошивку или проект...";
    s[(int)Str::StartupOpenFile] = "Открыть прошивку или проект...";
    s[(int)Str::UnsavedPrompt] = "Сохранить проект перед продолжением?";
    s[(int)Str::UnsavedPrompt] = "Сохранить проект перед продолжением?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Сбросить элемент";
    s[(int)Str::LayoutResetRevision] = "Сбросить расположение этой ревизии";
    s[(int)Str::LayoutResetAll] = "Сбросить все правки расположения";
    s[(int)Str::LayoutPatchSites] = "Адреса патчей";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Состояние предпросмотра";
    s[(int)Str::LayoutMoveElement] = "Переместить элемент UI";
    s[(int)Str::LayoutRba] = "Режим RBA";
    s[(int)Str::LayoutTemperatureControl] = "Термоконтроль";
    s[(int)Str::LayoutTemperature] = "Температура (°C)";
    s[(int)Str::LayoutRenderFailed] = "Штатный кадровый буфер недоступен.";
    s[(int)Str::LayoutElementPower] = "Мощность";
    s[(int)Str::LayoutElementBattery] = "Батарея";
    s[(int)Str::LayoutElementResistance] = "Сопротивление";
    s[(int)Str::LayoutElementDuration] = "Длительность";
    s[(int)Str::LayoutElementPuffs] = "Затяжки";
    s[(int)Str::LayoutElementMode] = "Режим";
    s[(int)Str::LayoutElementStatus] = "Статус";
    s[(int)Str::LayoutElementScore] = "Счёт";
    s[(int)Str::LayoutElementStatistics] = "Статистика";
    s[(int)Str::LayoutElementVoltage] = "Напряжение";
    s[(int)Str::LayoutElementGraph] = "График";
    s[(int)Str::LayoutElementRank] = "Ранг";
    s[(int)Str::LayoutElementModeSelection] = "Выбор режима";
    s[(int)Str::LayoutElementLogo] = "Логотип";
    s[(int)Str::LayoutChangedElements] = "Изменено элементов UI";
    s[(int)Str::EmulatorSupported] = "Нативный предпросмотр UI всех встроенных прошивок DRAG.";
    s[(int)Str::EmulatorNativeNote] = "Штатный код отрисовки с текущими правками. Электрическая часть не эмулируется.";
    s[(int)Str::EmulatorWatts] = "Мощность (Вт)";
    s[(int)Str::EmulatorPuffs] = "Тяги";
    s[(int)Str::EmulatorBattery] = "Заряд (0–5)";
    s[(int)Str::EmulatorResistance] = "Сопротивление (мОм)";
    s[(int)Str::EmulatorDuration] = "Длительность (0,01 с)";
    s[(int)Str::EmulatorLocked] = "Блокировка";
    s[(int)Str::EmulatorCharging] = "Зарядка";
    s[(int)Str::EmulatorScore] = "Всего очков";
    s[(int)Str::EmulatorScoreDisplay] = "Лучший результат";
    s[(int)Str::EmulatorRedraw] = "Перерисовать";
    s[(int)Str::EmulatorLandscape] = "Горизонтально";
    s[(int)Str::EmulatorScreen] = "Экран";
    s[(int)Str::EmulatorMainScreen] = "Основной экран";
    s[(int)Str::FontSystemFonts] = "Системные шрифты";
    s[(int)Str::FontUserFonts] = "Пользовательские шрифты";
    s[(int)Str::EmulatorMenuItem] = "Пункт меню";
    s[(int)Str::EmulatorMaterial] = "Материал";
    s[(int)Str::EmulatorMenuConfirmed] = "Подтверждено";
    s[(int)Str::EmulatorResources] = "Ресурсы, прочитанные прошивкой";
    s[(int)Str::EmulatorNoResources] = "Обращений к ресурсам каталога не зафиксировано.";
    s[(int)Str::EmulatorUidNote] = "Эмулируемый UID MCU, не ID подключённого устройства.";
    s[(int)Str::EmulatorLoaderVersion] = "Версия загрузчика (эмуляция)";
    s[(int)Str::EmulatorLoaderId] = "Байты CHIP-ID (эмуляция)";
    s[(int)Str::EmulatorLoaderNote] = "Данные загрузчика эмулируются, а не считываются из прошивки или устройства.";
    s[(int)Str::TabFirmwarePatches] = "Патчи";
    s[(int)Str::PatchBootLogo] = "Экран запуска";
    s[(int)Str::PatchShutdownLogo] = "Экран выключения";
    s[(int)Str::PatchDeviceInfo] = "Экран CHIP ID / версии";
    s[(int)Str::PatchDeviceInfoNote] = "Только статичное изображение. Заменяет сервисный экран, но не главный экран мощности и батареи.";
    s[(int)Str::PatchLogoNote] = "Независимые изображения 80 × 160 для всех ревизий DRAG S Score / Factory. На устройстве не проверено.";
    s[(int)Str::PatchUnsupported] = "Для патча нужна прошивка DRAG S Score или Factory.";
    s[(int)Str::PatchEnabled] = "Включить свой логотип";
    s[(int)Str::PatchCanvas] = "Композиция · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Нативный предпросмотр экрана";
    s[(int)Str::PatchPreviewNote] = "Нативный предпросмотр показывает применённые настройки, а не черновик композиции.";
    s[(int)Str::PatchFit] = "Вписать";
    s[(int)Str::PatchCover] = "Заполнить";
    s[(int)Str::PatchStretch] = "Растянуть";
    s[(int)Str::PatchScale] = "Масштаб";
    s[(int)Str::PatchAnimation] = "Анимация появления";
    s[(int)Str::PatchNone] = "Нет";
    s[(int)Str::PatchFade] = "Плавно";
    s[(int)Str::PatchDuration] = "Длительность (мс)";
    s[(int)Str::PatchAutoHardware] = "Автоматически";
    s[(int)Str::PatchAnimationPhase] = "Фаза анимации";
    s[(int)Str::PatchPlay] = "Воспроизвести";
    s[(int)Str::PatchBootStages] = "Заменить этап включения";
    s[(int)Str::PatchFirstStage] = "Первый · анимация VOOPOO";
    s[(int)Str::PatchSecondStage] = "Второй · название устройства";
    s[(int)Str::PatchBothStages] = "Оба";
    s[(int)Str::PatchExitAnimation] = "Анимация исчезновения";
    s[(int)Str::PatchHold] = "Пауза (мс)";
    s[(int)Str::PatchResampling] = "Фильтр масштабирования";
    s[(int)Str::PatchNearest] = "Ближайший сосед · пиксель-арт";
    s[(int)Str::PatchBilinear] = "Билинейный";
    s[(int)Str::PatchBicubic] = "Бикубический";
    s[(int)Str::PatchLanczos] = "Lanczos · фотографии";
    s[(int)Str::PatchIris] = "Круг";
    s[(int)Str::PatchDiamond] = "Ромб";
    s[(int)Str::PatchCenterHorizontal] = "Центр / края · горизонтально";
    s[(int)Str::PatchCenterVertical] = "Центр / края · вертикально";
    s[(int)Str::PatchBlinds] = "Жалюзи";
    s[(int)Str::PatchDissolve] = "Растворение";
    s[(int)Str::PatchImportImage] = "Открыть изображение...";
    s[(int)Str::PatchImageFormats] = "PNG, JPEG, BMP, GIF, TIFF, ICO, TGA, PSD, HDR, PNM. Для WebP / HEIF нужен кодек Windows. У анимированных файлов берётся первый кадр.";
    s[(int)Str::PatchPlacement] = "Размещение изображения";
    s[(int)Str::EmulatorModeScreens] = "Экраны";
    s[(int)Str::EmulatorModeDevice] = "Устройство";
    s[(int)Str::DeviceUnsupported] = "Режим устройства поддерживает DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Прошивка исполняется от Reset с виртуальной периферией. Нагрев моделируется; электрическая безопасность не проверяется.";
    s[(int)Str::DeviceReset] = "Перезапуск";
    s[(int)Str::DevicePause] = "Пауза";
    s[(int)Str::DeviceResume] = "Пуск";
    s[(int)Str::DeviceStep] = "Шаг";
    s[(int)Str::DeviceChanged] = "Текущие правки ещё не загружены в устройство.";
    s[(int)Str::DeviceApply] = "Перезапустить с правками";
    s[(int)Str::DeviceButtons] = "Удерживайте кнопки или пробел / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Спираль подключена";
    s[(int)Str::DeviceTime] = "Виртуальное время";
    s[(int)Str::DeviceSleeping] = "Процессор спит";
    s[(int)Str::DeviceCpuRunning] = "Процессор работает";
    s[(int)Str::DeviceLoading] = "Загрузка устройства...";
    s[(int)Str::DeviceLog] = "Обращения к периферии";
    s[(int)Str::DeviceDiagnostics] = "Диагностика";
    s[(int)Str::DeviceCpuRegisters] = "Регистры CPU";
    s[(int)Str::DeviceModeledClock] = "HCLK модели";
    s[(int)Str::DeviceClockNote] = "Частота модели, не измерение устройства или скорости ПК.";
    s[(int)Str::DeviceInstructions] = "Инструкции";
    s[(int)Str::DeviceMappedSram] = "SRAM модели";
    s[(int)Str::DeviceStack] = "Стек от SP";
    s[(int)Str::DeviceUnavailable] = "Недоступно";
    s[(int)Str::DeviceStackEmpty] = "SP на верхней границе SRAM.";
    s[(int)Str::DeviceStackInvalid] = "SP вне SRAM или не выровнен.";
    s[(int)Str::DeviceStackReadFailed] = "Не удалось прочитать стек из SRAM.";
    s[(int)Str::DeviceStackNote] = "До 32 слов: содержимое памяти, не цепочка вызовов.";
    s[(int)Str::DeviceValue] = "Значение";
    s[(int)Str::DeviceAddress] = "Адрес";
    s[(int)Str::DeviceBytes] = "Байты";
    s[(int)Str::BadgeNoFirmware] = "| [Прошивка не открыта — перетащите .zwdf/.xwdf сюда]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu элементов | %zu записей";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Открыть прошивку...";
    s[(int)Str::BtnExportAll] = "Экспорт всех ресурсов в папку...";
    s[(int)Str::BtnImportFolder] = "Импорт папки с PNG...";
    s[(int)Str::BtnBuildFirmware] = " СОБРАТЬ ПРОШИВКУ ";
    s[(int)Str::BtnRevertAllStaged] = "Сбросить все правки";
    s[(int)Str::GroupAll] = "Все группы";
    s[(int)Str::GroupAlerts] = "Оповещения (диалоги и значки)";
    s[(int)Str::GroupFonts] = "Шрифты (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Очки и ранги";
    s[(int)Str::GroupStatusLabels] = "Статус и подписи";
    s[(int)Str::GroupControls] = "Управление и батарея";
    s[(int)Str::GroupScreens] = "Экраны и загрузочные логотипы";
    s[(int)Str::GroupAnimations] = "Анимации";
    s[(int)Str::PanelResourcesHeader] = "Ресурсы";
    s[(int)Str::LabelSearch] = "Поиск";
    s[(int)Str::SearchPlaceholder] = "фильтр по имени, id, адресу...";
    s[(int)Str::NoFirmwareLoadedList] = "Прошивка не загружена.\nФайл > Создать / Открыть";
    s[(int)Str::NoResourcesMatchFilter] = "Ресурсы не найдены по фильтру.";
    s[(int)Str::PanelResourcePreviewHeader] = "Предпросмотр";
    s[(int)Str::SelectResourcePrompt] = "Выберите ресурс из списка слева для просмотра и редактирования.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Группа";
    s[(int)Str::LabelColor] = "Цвет";
    s[(int)Str::LabelColorCustom] = "Пользовательский";
    s[(int)Str::LabelFlashAddress] = "Адрес Flash";
    s[(int)Str::LabelDimensions] = "Размеры";
    s[(int)Str::LabelBytes] = "Байт";
    s[(int)Str::LabelRecords] = "Записей";
    s[(int)Str::StatusModifiedStaged] = "Статус: ИЗМЕНЁН (в памяти)";
    s[(int)Str::StatusOriginal] = "Статус: ОРИГИНАЛ (из прошивки)";
    s[(int)Str::StatusStockOriginal] = "Оригинал (совпадает с эталоном)";
    s[(int)Str::StatusCustomDiffers] = "Модифицирован (отличается от эталона)";
    s[(int)Str::BtnRestoreStock] = "Восстановить из эталона";
    s[(int)Str::LabelCompareWithStock] = "Сравнить с эталоном";
    s[(int)Str::LabelActivePreview] = "Текущий (активный):";
    s[(int)Str::LabelStockReferencePreview] = "Эталон (заводской):";
    s[(int)Str::FilterOnlyCustom] = "Только отличные от эталона";
    s[(int)Str::MsgRestoredToStock] = "Ресурс %s восстановлен из заводского эталона.";
    s[(int)Str::BtnReplaceFromPng] = "Заменить из PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Восстановить оригинал";
    s[(int)Str::BtnExportThisPng] = "Экспорт PNG...";
    s[(int)Str::BtnExportHorizontal] = "Экспорт горизонталь (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Цветовая палитра и рендер (RGB565)";
    s[(int)Str::LabelTheme] = "Тема";
    s[(int)Str::BtnApply] = "Применить";
    s[(int)Str::BtnResetAll] = "Сбросить всё";
    s[(int)Str::LabelRole] = "Роль";
    s[(int)Str::BtnResetRole] = "Сбросить роль";
    s[(int)Str::PromptLoadFwColors] = "Загрузите прошивку для просмотра и изменения цветов.";
    s[(int)Str::LabelPreviewTint] = "Цвет предпросмотра";
    s[(int)Str::InfoDigitsMonochrome] = "Цифры — монохромные маски. Цвет отображения задаётся кодом прошивки, а не хранится в глифе.";
    s[(int)Str::InfoPreviewNotSaved] = "Цвет предпросмотра не сохраняется в прошивку. Экспорт RGB565 PNG включает этот цвет.";
    s[(int)Str::LabelPreviewOnly] = "Только предпросмотр:";
    s[(int)Str::LabelPresets] = "Пресеты...";
    s[(int)Str::BtnResetColor] = "Сбросить цвет";
    s[(int)Str::LabelZoom] = "Масштаб";
    s[(int)Str::LabelPixelGrid] = "Пиксельная сетка";
    s[(int)Str::LabelHorizontalPreview] = "Горизонтальный предпросмотр (136x22):";
    s[(int)Str::FontSec1Title] = "1. Выбор шрифта";
    s[(int)Str::FontSec1WindowsFont] = "Шрифт Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Обзор TTF/OTF файла...";
    s[(int)Str::FontSec1ActiveFont] = "Активный шрифт: %s";
    s[(int)Str::FontSec2Title] = "2. Пресет отображения";
    s[(int)Str::FontSec3Title] = "3. Параметры рендера";
    s[(int)Str::FontSec3Threshold] = "Порог";
    s[(int)Str::FontSec3ThresholdHint] = "Меньше = жирнее, Больше = тоньше";
    s[(int)Str::FontSec3Stretch] = "Растянуть на всю ячейку";
    s[(int)Str::FontSec3Baseline] = "Выравнивание по базовой линии";
    s[(int)Str::FontSec3CenterV] = "Центрировать по вертикали";
    s[(int)Str::FontSec3PaddingX] = "Отступ X";
    s[(int)Str::FontSec3PaddingY] = "Отступ Y";
    s[(int)Str::FontSec3Oversampling] = "Передискретизация";
    s[(int)Str::FontSec4Title] = "4. Интеграция в прошивку";
    s[(int)Str::FontSec4ApplyPreset] = " > ПРИМЕНИТЬ ГЛИФЫ К ОТКРЫТОЙ ПРОШИВКЕ < ";
    s[(int)Str::FontSec4ApplyAll] = "Применить все шрифты";
    s[(int)Str::FontSec5Title] = "5. Экспорт в PNG";
    s[(int)Str::FontSec5OutputFolder] = "Выходная папка";
    s[(int)Str::FontSec5BrowseFolder] = "Обзор папки...";
    s[(int)Str::FontSec5OpenExplorer] = "Открыть в Проводнике";
    s[(int)Str::FontSec5ExportAllPreset] = "Экспорт всех пресетов";
    s[(int)Str::FontSec5ExportButton] = "Экспорт 1-бит PNG на диск";
    s[(int)Str::FontRightLivePreview] = "Предпросмотр шрифта";
    s[(int)Str::FontRightNoGlyphs] = "Глифы не загружены.";
    s[(int)Str::FontPresetInfo] = "Пресет: %s (%dx%d пк, %d глифов, размер: %.1f пк)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Спецификация пакета";
    s[(int)Str::FwInfoNoFirmware] = "Прошивка не загружена. Откройте .zwdf/.xwdf файл на вкладке «Студия ресурсов».";
    s[(int)Str::FwInfoFilePath] = "Путь к файлу: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 исходника: %s";
    s[(int)Str::FwInfoPackageSize] = "Размер пакета: %zu байт";
    s[(int)Str::FwInfoHwRevisions] = "Аппаратные ревизии: %zu записей";
    s[(int)Str::FwInfoRecognizedResources] = "Распознанные ресурсы: %zu элементов (шрифты, значки, ранги, метки, элементы управления, экраны)";
    s[(int)Str::FwInfoRecordsTable] = "Таблица аппаратных записей";
    s[(int)Str::FwInfoColRecord] = "Запись №";
    s[(int)Str::FwInfoColDevCode] = "Код устройства";
    s[(int)Str::FwInfoColPayload] = "Размер данных";
    s[(int)Str::FwInfoColImage] = "Размер образа";
    s[(int)Str::FwInfoColFlashBase] = "Базовый адрес Flash";
    s[(int)Str::FwInfoColMSP] = "Указатель стека (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Вектор сброса";
    s[(int)Str::BtnRevertAllModifications] = "Отменить все изменения";
    s[(int)Str::MsgReadyOpenFw] = "Готово. Откройте .zwdf/.xwdf файл прошивки или сгенерируйте шрифты.";
    s[(int)Str::MsgSuccessOpened] = "Открыто: %s";
    s[(int)Str::MsgOpenError] = "Ошибка открытия: %s";
    s[(int)Str::MsgAllReverted] = "Все изменения отменены, восстановлено исходное состояние.";
    s[(int)Str::MsgExportSuccess] = "Экспортировано %d ресурсов в: %s";
    s[(int)Str::MsgExportError] = "Ошибка экспорта: %s";
    s[(int)Str::MsgImportSuccess] = "Импортировано %d PNG из папки.";
    s[(int)Str::MsgImportError] = "Ошибка импорта: %s";
    s[(int)Str::MsgBuildSuccess] = "Сборка УСПЕШНА: %s";
    s[(int)Str::MsgBuildError] = "Ошибка сборки: %s";
    s[(int)Str::MsgFontNotLoaded] = "Ошибка: шрифт не загружен в генераторе!";
    s[(int)Str::MsgFontInjectedPreset] = "Внедрено %d глифов шрифта %s в память прошивки!";
    s[(int)Str::MsgFontInjectedAll] = "Применено глифов: %d.";
    s[(int)Str::MsgFontExportSuccess] = "Экспортировано %d PNG файлов в: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "Экспортировано %d файлов для %s в: %s";
    s[(int)Str::MsgFontExportError] = "Ошибка экспорта: %s";
    s[(int)Str::ModalBuildReportTitle] = "Отчёт о сборке";
    s[(int)Str::ModalBuildSuccessBanner] = "СБОРКА И ВЕРИФИКАЦИЯ ПРОШИВКИ УСПЕШНЫ!";
    s[(int)Str::ModalSavedFile] = "Сохранённый файл: %s";
    s[(int)Str::ModalOutputSize] = "Размер выходного файла: %zu байт";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Изменённые ресурсы: %zu";
    s[(int)Str::ModalModColorGroups] = "Изменённые группы цветов: %zu";
    s[(int)Str::ModalPatchedRanges] = "Пропатченные диапазоны: %zu в %zu аппаратных записях";
    s[(int)Str::ModalUntouchedBytes] = "Нетронутые байты: %zu байт идентичны оригиналу";
    s[(int)Str::ModalReportNote] = "Подробный отчёт сохранён как .report.json рядом с прошивкой.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Открыть в Проводнике";
    s[(int)Str::BtnCopyClipboard] = "Копировать (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Вставить (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Отменить";
    s[(int)Str::BtnRedo] = "Повторить";
    s[(int)Str::TooltipUndo] = "Отменить: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Повторить: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Нечего отменять (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Нечего повторять (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Инструменты:";
    s[(int)Str::ToolPencil] = "Карандаш";
    s[(int)Str::ToolEraser] = "Ластик";
    s[(int)Str::BtnInvertPixels] = "Инверсия";
    s[(int)Str::BtnClearPixels] = "Очистить";
    s[(int)Str::BtnFlipH] = "Отразить Г";
    s[(int)Str::BtnFlipV] = "Отразить В";
    s[(int)Str::MsgCopiedToClipboard] = "Изображение %s скопировано в буфер.";
    s[(int)Str::MsgPastedFromClipboard] = "Изображение из буфера вставлено для %s!";
    s[(int)Str::MsgClipboardNoImage] = "В буфере обмена нет подходящего изображения.";
    s[(int)Str::MsgUndoPerformed] = "Отменено: %s";
    s[(int)Str::MsgRedoPerformed] = "Повторено: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Прошивка: %s (SHA256: %s... | %zu записей | %zu изм.)";
    s[(int)Str::BadgeProjectSummary] = "| Проект: %s (SHA256: %s... | %zu записей | %zu изм.)";
    s[(int)Str::LabelCategory] = "Категория";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelPreviewRank] = "Ранг для предпросмотра";
    s[(int)Str::LabelPreviewCyanMode] = "Предпросмотр: циан-режим (RBA)";
    s[(int)Str::LabelPreviewBackground] = "Фон в прошивке: %s";
    s[(int)Str::LabelBrushColor] = "Цвет кисти (пиксели RGB565)";
    s[(int)Str::InfoNativeRgb565] = "Цвета хранятся в битмапе. Кисть изменяет сами пиксели.";
    s[(int)Str::InfoFirmwareColors] = "Предпросмотр использует цвет рисунка и фона из прошивки. Цвета темы меняются в палитре.";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Группа: %s  |  Цвет: Пользовательский (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Группа: %s  |  Цвет: %s";
    s[(int)Str::LabelMetaHardware] = "Адрес Flash: 0x%08X  |  Размер: %u x %u пкс  |  Байт: %zu  |  Записей: %zu";
    s[(int)Str::LabelPixelCoord] = "Пиксель: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| ЛКМ: %s  |  ПКМ: Стереть";
    s[(int)Str::LabelDrawAction] = "Рисовать";
    s[(int)Str::LabelEraseAction] = "Стереть";
    s[(int)Str::LabelFontPreset] = "Пресет";
    s[(int)Str::LabelFontPixelGrid] = "Пиксельная сетка";
    s[(int)Str::MsgAppliedColorTheme] = "Применена цветовая тема: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "Импортировано %zu подходящих PNG из папки.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Прошивка открыта через Drag & Drop: %s";
    s[(int)Str::MsgDragDropError] = "Ошибка Drag & Drop: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Шрифт загружен через Drag & Drop: %s";
    s[(int)Str::MsgFailedLoadFont] = "Не удалось загрузить файл шрифта: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Сброшенный PNG совпадает с оригиналом для %s.";
    s[(int)Str::MsgStagedReplacement] = "Подготовлена замена для %s из PNG!";
    s[(int)Str::MsgPngLoadError] = "Ошибка загрузки PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Замена для %s совпадает с оригиналом.";
    s[(int)Str::MsgStagedReplacementCustom] = "Подготовлена замена для %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "Подготовлена RGB565 замена для %s!";
    s[(int)Str::MsgRestoredOriginal] = "Ресурс %s восстановлен к оригиналу прошивки.";
    s[(int)Str::MsgSavedAssetPng] = "Сохранён %s в: %s";
    s[(int)Str::MsgSaveError] = "Ошибка сохранения: %s";
    s[(int)Str::MsgSavedBootLogo] = "Сохранён горизонтальный логотип в: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Ошибка: нельзя перезаписывать исходную прошивку! Выберите другое имя.";
    s[(int)Str::MsgColorPatchError] = "Ошибка патча цветов: %s";
    s[(int)Str::MsgPackError] = "Ошибка упаковки: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Импортировано %zu изменённых PNG (%zu совпали с оригиналом)";

    s[(int)Str::LabelBuiltin] = "встроенная";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Не удалось загрузить встроенную прошивку: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Загружена встроенная прошивка: %s";
    s[(int)Str::ThemeFactoryDefault] = "Заводская по умолчанию";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Оригинальный заводской стиль Voopoo (красный и жёлтый)";
    s[(int)Str::ThemeCyberpunkNeon] = "Киберпанк неон";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Электрический циан, неоновый розовый и фиолетовый";
    s[(int)Str::ThemeMatrixGreen] = "Матрица зелёный";
    s[(int)Str::ThemeMatrixGreenDesc] = "ЭЛТ-терминал фосфор и изумрудный зелёный";
    s[(int)Str::ThemeArcticIceBlue] = "Арктический лёд";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Ледниковый циан, глубокое небо и лазурное свечение";
    s[(int)Str::ThemeAmberFire] = "Янтарный огонь";
    s[(int)Str::ThemeAmberFireDesc] = "Лавовый оранжевый, золотое пламя и уголь";
    s[(int)Str::ThemePurpleNebula] = "Пурпурная туманность";
    s[(int)Str::ThemePurpleNebulaDesc] = "Глубокий фиолетовый, лаванда и неон аметист";
    s[(int)Str::ThemeStealthDarkGray] = "Стелс тёмно-серый";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Матовый графит, сланец и оружейный металл";
    s[(int)Str::RoleAccentRed] = "Акцент / красный режим";
    s[(int)Str::RoleAccentRedScope] = "Основной акцент UI в стандартных режимах — 8 мест";
    s[(int)Str::RoleAccentCyan] = "Акцент / циан режим";
    s[(int)Str::RoleAccentCyanScope] = "Основной акцент UI в режимах SMART/RBA — 7 мест";
    s[(int)Str::RoleHighlightYellow] = "Подсветка / жёлтый";
    s[(int)Str::RoleHighlightYellowScope] = "Полоса очков и сервисные панели — 4 места";
    s[(int)Str::RoleRankBronze] = "Ранг / бронза";
    s[(int)Str::RoleRankBronzeScope] = "Палитра ранга Бронза";
    s[(int)Str::RoleRankGold] = "Ранг / золото";
    s[(int)Str::RoleRankGoldScope] = "Палитра ранга Золото";
    s[(int)Str::RoleRankIce] = "Ранг / платина (лёд)";
    s[(int)Str::RoleRankIceScope] = "Палитра ранга Платина / Лёд";
    s[(int)Str::RoleRankCyan] = "Ранг / циан";
    s[(int)Str::RoleRankCyanScope] = "Палитра ранга Циан";
    s[(int)Str::RoleRankOrange] = "Ранг / оранжевый";
    s[(int)Str::RoleRankOrangeScope] = "Палитра ранга Оранжевый";
    s[(int)Str::RoleRankRed] = "Ранг / красный";
    s[(int)Str::RoleRankRedScope] = "Палитра ранга Красный";
    s[(int)Str::RoleDigitsWattage] = "Цифры мощности";
    s[(int)Str::RoleDigitsWattageScope] = "Крупные цифры мощности на главном экране (19x40)";
    s[(int)Str::RoleDigitsStats] = "Цифры статистики";
    s[(int)Str::RoleDigitsStatsScope] = "Цифры счётчика затяжек (11x20); фон — акцент режима";
    s[(int)Str::ColorPresetWhite] = "Белый (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Основной / Красный (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Циан (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Сервисный / Жёлтый (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Бронза (#B57947)";
    s[(int)Str::ColorPresetGold] = "Золото (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Оранжевый (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Зелёный (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Основные цифры 0-9, 10 файлов)";
    s[(int)Str::FontPreset11x20] = "11x20 (Вторичные цифры 0-9, 10 файлов)";
    s[(int)Str::FontPreset7x13] = "7x13 (Малые статусные 0-9, A-F, 16 файлов)";
    s[(int)Str::FontPreset6x10] = "6x10 (Микро статус 0-9, 10 файлов)";
    s[(int)Str::FontOs1x] = "1x (Прямой)";
    s[(int)Str::FontOs2x] = "2x (Быстрый)";
    s[(int)Str::FontOs4x] = "4x (Рекомендуемый)";
    s[(int)Str::FontOs8x] = "8x (Ультра)";
    s[(int)Str::ToolEyedropper] = "Пипетка";
    s[(int)Str::ToolBucketFill] = "Заливка";
    s[(int)Str::BtnInvertCanvas] = "Инвертировать";
    s[(int)Str::BtnClearCanvas] = "Очистить";
    s[(int)Str::LabelBrushSize] = "Размер:";
}

static void InitUkrainian() {
    auto& s = g_table[(int)Language::Ukrainian];
    s[(int)Str::SupportFull] = "Повністю";
    s[(int)Str::SupportNone] = "Не підтримується";
    s[(int)Str::SupportPalette] = "Колірна палітра";
    s[(int)Str::SupportMonochrome] = "Монохромний екран; колірної палітри немає.";
    s[(int)Str::EmulatorHistoryDay] = "День історії";
    s[(int)Str::EmulatorClockHour] = "Години";
    s[(int)Str::EmulatorClockMinute] = "Хвилини";
    s[(int)Str::EmulatorClockField] = "Вибране поле";
    s[(int)Str::MenuFile] = "Файл";
    s[(int)Str::MenuEdit] = "Редагування";
    s[(int)Str::MenuLanguage] = "Мова";
    s[(int)Str::MenuHelp] = "Довідка";
    s[(int)Str::UpdateTitle] = "Оновлення студії";
    s[(int)Str::UpdateVersion] = "Установлена версія: %s";
    s[(int)Str::UpdateCheck] = "Перевірити оновлення";
    s[(int)Str::UpdateChecking] = "Перевірка GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Установлено останню версію.";
    s[(int)Str::UpdateDownload] = "Завантажити";
    s[(int)Str::UpdateDownloading] = "Завантаження та перевірка...";
    s[(int)Str::UpdateInstall] = "Установити й перезапустити";
    s[(int)Str::UpdateReady] = "Готово до встановлення.";
    s[(int)Str::UpdateChangelog] = "Зміни";
    s[(int)Str::UpdateFailed] = "Помилка оновлення";
    s[(int)Str::UpdateAvailable] = "Доступна версія: %s";
    s[(int)Str::MenuCreate] = "Створити...";
    s[(int)Str::MenuOpen] = "Відкрити...";
    s[(int)Str::MenuImport] = "Імпорт ресурсів...";
    s[(int)Str::MenuExport] = "Експорт ресурсів...";
    s[(int)Str::MenuBuild] = "Зібрати прошивку...";
    s[(int)Str::MenuExit] = "Вихід";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ресурси";
    s[(int)Str::TabFontGenerator] = "Генератор шрифтів";
    s[(int)Str::TabFirmwareInfo] = "Про прошивку";
    s[(int)Str::TabFirmwareComparison] = "Порівняння";
    s[(int)Str::CompareModeTwoFiles] = "Два файли";
    s[(int)Str::CompareModeEditor] = "Редактор і файл";
    s[(int)Str::CompareLeft] = "Ліворуч";
    s[(int)Str::CompareRight] = "Праворуч";
    s[(int)Str::CompareChooseFile] = "Вибрати файл...";
    s[(int)Str::CompareReadOnlyNote] = "Лише перегляд; враховуються зміни в редакторі.";
    s[(int)Str::CompareChooseSources] = "Виберіть обидва джерела для порівняння.";
    s[(int)Str::CompareEditorUnavailable] = "У редакторі не відкрито прошивку.";
    s[(int)Str::CompareChanged] = "Змінено";
    s[(int)Str::CompareAdded] = "Додано";
    s[(int)Str::CompareRemoved] = "Видалено";
    s[(int)Str::CompareEqual] = "Збігається";
    s[(int)Str::CompareIncompatible] = "Незіставний";
    s[(int)Str::CompareDecodeError] = "Помилка декодування";
    s[(int)Str::CompareStatusFilter] = "Статус";
    s[(int)Str::CompareDifferencePreview] = "Різниця";
    s[(int)Str::CompareHighlightPixels] = "Підсвітити відмінності";
    s[(int)Str::CompareDifferentPixels] = "Відрізняються %zu / %zu пікселів";
    s[(int)Str::CompareMissingResource] = "Ресурс відсутній";
    s[(int)Str::CompareIncompatibleHint] = "Різні типи або розміри ресурсів: попіксельне порівняння недоступне.";
    s[(int)Str::CompareImageUnavailable] = "Зображення недоступне";
    s[(int)Str::CompareTextureError] = "Не вдалося створити текстуру перегляду.";
    s[(int)Str::CompareAllStatuses] = "Усі статуси";
    s[(int)Str::TabFirmwareEmulator] = "Емулятор";
    s[(int)Str::TabFirmwareLayout] = "Розташування UI";
    s[(int)Str::LayoutUnsupported] = "Редактор розташування UI недоступний для цієї прошивки.";
    s[(int)Str::LayoutNativeNote] = "Перетягніть елемент або введіть X/Y. Перегляд виконує змінену прошивку; зміни стосуються вибраної апаратної ревізії.";
    s[(int)Str::LayoutRevision] = "Апаратна ревізія";
    s[(int)Str::LayoutCheckingRevisions] = "Перевірка однакових інтерфейсів…";
    s[(int)Str::LayoutAllRevisions] = "Усі ревізії";
    s[(int)Str::LayoutRevisionGroup] = "Однаковий UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Ревізія: %s";
    s[(int)Str::LayoutResetScope] = "Скинути вибрані ревізії";
    s[(int)Str::LayoutSelectionUnavailable] = "Для вибраних ревізій немає сумісного елемента.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Елементи UI";
    s[(int)Str::LayoutNoElements] = "На цьому екрані немає блоків для переміщення.";
    s[(int)Str::LayoutSharedBlock] = "Координати цього блока спільні для кількох екранів.";
    s[(int)Str::LayoutPositionX] = "X (пікселі)";
    s[(int)Str::LayoutPositionY] = "Y (пікселі)";
    s[(int)Str::LayoutOffset] = "Зсув: %+d, %+d пікс.";
    s[(int)Str::LayoutGridStep] = "Крок: %d × %d пікс.";
    s[(int)Str::LayoutGridSpacing] = "Крок сітки (пікс.)";
    s[(int)Str::LayoutGridConstraint] = "Якщо вузли сітки недоступні, пріоритет мають нативний крок і межі координат.";
    s[(int)Str::LayoutSnapGrid] = "Прив’язка до сітки";
    s[(int)Str::LayoutSnapElements] = "Прив’язка до країв і центрів елементів";
    s[(int)Str::LayoutGuides] = "Напрямні переміщення";
    s[(int)Str::LayoutAlign] = "Вирівняти за екраном";
    s[(int)Str::LayoutAlignLeft] = "Ліворуч";
    s[(int)Str::LayoutAlignCenter] = "Центр X";
    s[(int)Str::LayoutAlignRight] = "Праворуч";
    s[(int)Str::LayoutAlignTop] = "Зверху";
    s[(int)Str::LayoutAlignMiddle] = "Центр Y";
    s[(int)Str::LayoutAlignBottom] = "Знизу";
    s[(int)Str::LayoutNudgeHint] = "Стрілки: нативний крок у пікселях; Shift: більший крок. Натисніть на екран для фокуса.";
    s[(int)Str::LayoutOverlays] = "Контури елементів";
    s[(int)Str::LayoutOverlapOverlay] = "Підсвітити перетини";
    s[(int)Str::LayoutOverlaps] = "Елементи, що перетинаються";
    s[(int)Str::LayoutNoOverlaps] = "Перетинів прямокутників немає";
    s[(int)Str::LayoutDrawOrderUnknown] = "Нативний порядок малювання не підтверджено";
    s[(int)Str::LayoutDrawOrderPair] = "%s (№%d) малюється пізніше за %s (№%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Порядок викликів основного малювання, а не постійні шари. Під час оновлень він може відрізнятися; попередній вміст перекривають лише фактично записані пікселі.";
    s[(int)Str::LayoutDrawSequence] = "Штатний порядок малювання";
    s[(int)Str::LayoutDrawInterleaved] = "Виклики чергуються; перекриття залежить від частини кожного елемента.";
    s[(int)Str::EmulatorBefore] = "До (відкрита прошивка)";
    s[(int)Str::EmulatorAfter] = "Після (поточні зміни)";
    s[(int)Str::EmulatorSideBySide] = "Поруч";
    s[(int)Str::EmulatorOriginalError] = "Помилка перегляду відкритої прошивки: %s";
    s[(int)Str::MenuSaveProject] = "Зберегти проєкт";
    s[(int)Str::MenuSaveProjectAs] = "Зберегти проєкт як...";
    s[(int)Str::MsgProjectSaved] = "Проєкт збережено: %s";
    s[(int)Str::MenuBuild] = "Експорт прошивки";
    s[(int)Str::BtnOpenFirmware] = "Відкрити прошивку або проєкт...";
    s[(int)Str::StartupOpenFile] = "Відкрити прошивку або проєкт...";
    s[(int)Str::UnsavedPrompt] = "Зберегти проєкт перед продовженням?";
    s[(int)Str::UnsavedPrompt] = "Зберегти проєкт перед продовженням?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Скинути елемент";
    s[(int)Str::LayoutResetRevision] = "Скинути розташування цієї ревізії";
    s[(int)Str::LayoutResetAll] = "Скинути всі зміни розташування";
    s[(int)Str::LayoutPatchSites] = "Адреси патчів";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Стан перегляду";
    s[(int)Str::LayoutMoveElement] = "Перемістити елемент UI";
    s[(int)Str::LayoutRba] = "Режим RBA";
    s[(int)Str::LayoutTemperatureControl] = "Термоконтроль";
    s[(int)Str::LayoutTemperature] = "Температура (°C)";
    s[(int)Str::LayoutRenderFailed] = "Штатний кадровий буфер недоступний.";
    s[(int)Str::LayoutElementPower] = "Потужність";
    s[(int)Str::LayoutElementBattery] = "Батарея";
    s[(int)Str::LayoutElementResistance] = "Опір";
    s[(int)Str::LayoutElementDuration] = "Тривалість";
    s[(int)Str::LayoutElementPuffs] = "Затяжки";
    s[(int)Str::LayoutElementMode] = "Режим";
    s[(int)Str::LayoutElementStatus] = "Статус";
    s[(int)Str::LayoutElementScore] = "Рахунок";
    s[(int)Str::LayoutElementStatistics] = "Статистика";
    s[(int)Str::LayoutElementVoltage] = "Напруга";
    s[(int)Str::LayoutElementGraph] = "Графік";
    s[(int)Str::LayoutElementRank] = "Ранг";
    s[(int)Str::LayoutElementModeSelection] = "Вибір режиму";
    s[(int)Str::LayoutElementLogo] = "Логотип";
    s[(int)Str::LayoutChangedElements] = "Змінено елементів UI";
    s[(int)Str::EmulatorSupported] = "Нативний перегляд UI усіх вбудованих прошивок DRAG.";
    s[(int)Str::EmulatorNativeNote] = "Штатний код малювання з поточними змінами. Електрична частина не емулюється.";
    s[(int)Str::EmulatorWatts] = "Потужність (Вт)";
    s[(int)Str::EmulatorPuffs] = "Затяжки";
    s[(int)Str::EmulatorBattery] = "Заряд (0–5)";
    s[(int)Str::EmulatorResistance] = "Опір (мОм)";
    s[(int)Str::EmulatorDuration] = "Тривалість (0,01 с)";
    s[(int)Str::EmulatorLocked] = "Блокування";
    s[(int)Str::EmulatorCharging] = "Заряджання";
    s[(int)Str::EmulatorScore] = "Усього балів";
    s[(int)Str::EmulatorScoreDisplay] = "Найкращий результат";
    s[(int)Str::EmulatorRedraw] = "Перемалювати";
    s[(int)Str::EmulatorLandscape] = "Горизонтально";
    s[(int)Str::EmulatorScreen] = "Екран";
    s[(int)Str::EmulatorMainScreen] = "Основний екран";
    s[(int)Str::FontSystemFonts] = "Системні шрифти";
    s[(int)Str::FontUserFonts] = "Користувацькі шрифти";
    s[(int)Str::EmulatorMenuItem] = "Пункт меню";
    s[(int)Str::EmulatorMaterial] = "Матеріал";
    s[(int)Str::EmulatorMenuConfirmed] = "Підтверджено";
    s[(int)Str::EmulatorResources] = "Ресурси, прочитані прошивкою";
    s[(int)Str::EmulatorNoResources] = "Звернень до ресурсів каталогу не зафіксовано.";
    s[(int)Str::EmulatorUidNote] = "Емульований UID MCU, не ID підключеного пристрою.";
    s[(int)Str::EmulatorLoaderVersion] = "Версія завантажувача (емуляція)";
    s[(int)Str::EmulatorLoaderId] = "Байти CHIP-ID (емуляція)";
    s[(int)Str::EmulatorLoaderNote] = "Дані завантажувача емулюються, а не зчитуються з прошивки чи пристрою.";
    s[(int)Str::TabFirmwarePatches] = "Патчі";
    s[(int)Str::PatchBootLogo] = "Екран запуску";
    s[(int)Str::PatchShutdownLogo] = "Екран вимкнення";
    s[(int)Str::PatchDeviceInfo] = "Екран CHIP ID / версії";
    s[(int)Str::PatchDeviceInfoNote] = "Лише статичне зображення. Замінює сервісний екран, а не головний екран потужності й батареї.";
    s[(int)Str::PatchLogoNote] = "Незалежні зображення 80 × 160 для всіх ревізій DRAG S Score / Factory. На пристрої не перевірено.";
    s[(int)Str::PatchUnsupported] = "Для патча потрібна прошивка DRAG S Score або Factory.";
    s[(int)Str::PatchEnabled] = "Увімкнути власний логотип";
    s[(int)Str::PatchCanvas] = "Композиція · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Нативний перегляд екрана";
    s[(int)Str::PatchPreviewNote] = "Нативний перегляд показує застосовані налаштування, а не чернетку композиції.";
    s[(int)Str::PatchFit] = "Вписати";
    s[(int)Str::PatchCover] = "Заповнити";
    s[(int)Str::PatchStretch] = "Розтягнути";
    s[(int)Str::PatchScale] = "Масштаб";
    s[(int)Str::PatchAnimation] = "Анімація появи";
    s[(int)Str::PatchNone] = "Немає";
    s[(int)Str::PatchFade] = "Поява";
    s[(int)Str::PatchDuration] = "Тривалість (мс)";
    s[(int)Str::PatchAutoHardware] = "Автоматично";
    s[(int)Str::PatchAnimationPhase] = "Фаза анімації";
    s[(int)Str::PatchPlay] = "Відтворити";
    s[(int)Str::EmulatorModeScreens] = "Екрани";
    s[(int)Str::EmulatorModeDevice] = "Пристрій";
    s[(int)Str::DeviceUnsupported] = "Режим пристрою підтримує DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Прошивка виконується від Reset із віртуальною периферією. Нагрів моделюється; електрична безпека не перевіряється.";
    s[(int)Str::DeviceReset] = "Перезапуск";
    s[(int)Str::DevicePause] = "Пауза";
    s[(int)Str::DeviceResume] = "Пуск";
    s[(int)Str::DeviceStep] = "Крок";
    s[(int)Str::DeviceChanged] = "Поточні зміни ще не завантажені в пристрій.";
    s[(int)Str::DeviceApply] = "Перезапустити зі змінами";
    s[(int)Str::DeviceButtons] = "Утримуйте кнопки або пробіл / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Спіраль підключена";
    s[(int)Str::DeviceTime] = "Віртуальний час";
    s[(int)Str::DeviceSleeping] = "Процесор спить";
    s[(int)Str::DeviceLoading] = "Завантаження пристрою...";
    s[(int)Str::DeviceLog] = "Звернення до периферії";
    s[(int)Str::BadgeNoFirmware] = "| [Прошивку не відкрито — перетягніть .zwdf/.xwdf сюди]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu елементів | %zu записів";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Відкрити прошивку...";
    s[(int)Str::BtnExportAll] = "Експорт усіх ресурсів до теки...";
    s[(int)Str::BtnImportFolder] = "Імпорт теки з PNG...";
    s[(int)Str::BtnBuildFirmware] = " ЗІБРАТИ ПРОШИВКУ ";
    s[(int)Str::BtnRevertAllStaged] = "Скинути всі правки";
    s[(int)Str::GroupAll] = "Всі групи";
    s[(int)Str::GroupAlerts] = "Сповіщення (діалоги та значки)";
    s[(int)Str::GroupFonts] = "Шрифти (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Очки та ранги";
    s[(int)Str::GroupStatusLabels] = "Статус та підписи";
    s[(int)Str::GroupControls] = "Управління та батарея";
    s[(int)Str::GroupScreens] = "Екрани та логотипи завантаження";
    s[(int)Str::GroupAnimations] = "Анімації";
    s[(int)Str::PanelResourcesHeader] = "Ресурси";
    s[(int)Str::LabelSearch] = "Пошук";
    s[(int)Str::SearchPlaceholder] = "фільтр за назвою, id, адресою...";
    s[(int)Str::NoFirmwareLoadedList] = "Прошивку не завантажено.\nФайл > Створити / Відкрити";
    s[(int)Str::NoResourcesMatchFilter] = "Ресурси не знайдено за фільтром.";
    s[(int)Str::PanelResourcePreviewHeader] = "Попередній перегляд";
    s[(int)Str::SelectResourcePrompt] = "Виберіть ресурс зі списку ліворуч для перегляду та редагування.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Група";
    s[(int)Str::LabelColor] = "Колір";
    s[(int)Str::LabelColorCustom] = "Користувацький";
    s[(int)Str::LabelFlashAddress] = "Адреса Flash";
    s[(int)Str::LabelDimensions] = "Розміри";
    s[(int)Str::LabelBytes] = "Байт";
    s[(int)Str::LabelRecords] = "Записів";
    s[(int)Str::StatusModifiedStaged] = "Статус: ЗМІНЕНО (в пам'яті)";
    s[(int)Str::StatusOriginal] = "Статус: ОРИГІНАЛ (з прошивки)";
    s[(int)Str::StatusStockOriginal] = "Оригінал (збігається з еталоном)";
    s[(int)Str::StatusCustomDiffers] = "Модифікований (відрізняється від еталону)";
    s[(int)Str::BtnRestoreStock] = "Відновити з еталону";
    s[(int)Str::LabelCompareWithStock] = "Порівняти з еталоном";
    s[(int)Str::LabelActivePreview] = "Поточний (активний):";
    s[(int)Str::LabelStockReferencePreview] = "Еталон (заводський):";
    s[(int)Str::FilterOnlyCustom] = "Тільки відмінні від еталону";
    s[(int)Str::MsgRestoredToStock] = "Ресурс %s відновлено із заводського еталону.";
    s[(int)Str::BtnReplaceFromPng] = "Замінити з PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Відновити оригінал";
    s[(int)Str::BtnExportThisPng] = "Експорт PNG...";
    s[(int)Str::BtnExportHorizontal] = "Експорт горизонталь (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Кольорова палітра та рендер (RGB565)";
    s[(int)Str::LabelTheme] = "Тема";
    s[(int)Str::BtnApply] = "Застосувати";
    s[(int)Str::BtnResetAll] = "Скинути все";
    s[(int)Str::LabelRole] = "Роль";
    s[(int)Str::BtnResetRole] = "Скинути роль";
    s[(int)Str::PromptLoadFwColors] = "Завантажте прошивку для перегляду та зміни кольорів.";
    s[(int)Str::LabelPreviewTint] = "Колір попереднього перегляду";
    s[(int)Str::InfoDigitsMonochrome] = "Цифри — монохромні маски. Колір відображення задається кодом прошивки, а не зберігається в гліфі.";
    s[(int)Str::InfoPreviewNotSaved] = "Колір попереднього перегляду не зберігається в прошивку. Експорт RGB565 PNG включає цей колір.";
    s[(int)Str::LabelPreviewOnly] = "Лише попередній перегляд:";
    s[(int)Str::LabelPresets] = "Пресети...";
    s[(int)Str::BtnResetColor] = "Скинути колір";
    s[(int)Str::LabelZoom] = "Масштаб";
    s[(int)Str::LabelPixelGrid] = "Піксельна сітка";
    s[(int)Str::LabelHorizontalPreview] = "Горизонтальний попередній перегляд (136x22):";
    s[(int)Str::FontSec1Title] = "1. Вибір шрифту";
    s[(int)Str::FontSec1WindowsFont] = "Шрифт Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Огляд TTF/OTF файлу...";
    s[(int)Str::FontSec1ActiveFont] = "Активний шрифт: %s";
    s[(int)Str::FontSec2Title] = "2. Пресет відображення";
    s[(int)Str::FontSec3Title] = "3. Параметри рендеру";
    s[(int)Str::FontSec3Threshold] = "Поріг";
    s[(int)Str::FontSec3ThresholdHint] = "Менше = жирніше, Більше = тонше";
    s[(int)Str::FontSec3Stretch] = "Розтягнути на всю комірку";
    s[(int)Str::FontSec3Baseline] = "Вирівнювання по базовій лінії";
    s[(int)Str::FontSec3CenterV] = "Центрувати по вертикалі";
    s[(int)Str::FontSec3PaddingX] = "Відступ X";
    s[(int)Str::FontSec3PaddingY] = "Відступ Y";
    s[(int)Str::FontSec3Oversampling] = "Передискретизація";
    s[(int)Str::FontSec4Title] = "4. Інтеграція в прошивку";
    s[(int)Str::FontSec4ApplyPreset] = " > ЗАСТОСУВАТИ ГЛІФИ ДО ВІДКРИТОЇ ПРОШИВКИ < ";
    s[(int)Str::FontSec4ApplyAll] = "Застосувати всі шрифти";
    s[(int)Str::FontSec5Title] = "5. Експорт у PNG";
    s[(int)Str::FontSec5OutputFolder] = "Вихідна тека";
    s[(int)Str::FontSec5BrowseFolder] = "Огляд теки...";
    s[(int)Str::FontSec5OpenExplorer] = "Відкрити в Провіднику";
    s[(int)Str::FontSec5ExportAllPreset] = "Експорт усіх пресетів";
    s[(int)Str::FontSec5ExportButton] = "Експорт 1-біт PNG на диск";
    s[(int)Str::FontRightLivePreview] = "Попередній перегляд шрифту";
    s[(int)Str::FontRightNoGlyphs] = "Гліфи не завантажено.";
    s[(int)Str::FontPresetInfo] = "Пресет: %s (%dx%d пк, %d гліфів, розмір: %.1f пк)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Специфікація пакету";
    s[(int)Str::FwInfoNoFirmware] = "Прошивку не завантажено. Відкрийте .zwdf/.xwdf файл на вкладці «Студія ресурсів».";
    s[(int)Str::FwInfoFilePath] = "Шлях до файлу: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 джерела: %s";
    s[(int)Str::FwInfoPackageSize] = "Розмір пакету: %zu байт";
    s[(int)Str::FwInfoHwRevisions] = "Апаратні ревізії: %zu записів";
    s[(int)Str::FwInfoRecognizedResources] = "Розпізнані ресурси: %zu елементів (шрифти, значки, ранги, мітки, елементи керування, екрани)";
    s[(int)Str::FwInfoRecordsTable] = "Таблиця апаратних записів";
    s[(int)Str::FwInfoColRecord] = "Запис №";
    s[(int)Str::FwInfoColDevCode] = "Код пристрою";
    s[(int)Str::FwInfoColPayload] = "Розмір даних";
    s[(int)Str::FwInfoColImage] = "Розмір образу";
    s[(int)Str::FwInfoColFlashBase] = "Базова адреса Flash";
    s[(int)Str::FwInfoColMSP] = "Покажчик стеку (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Вектор скидання";
    s[(int)Str::BtnRevertAllModifications] = "Скасувати всі зміни";
    s[(int)Str::MsgReadyOpenFw] = "Готово. Відкрийте .zwdf/.xwdf файл прошивки або згенеруйте шрифти.";
    s[(int)Str::MsgSuccessOpened] = "Відкрито: %s";
    s[(int)Str::MsgOpenError] = "Помилка відкриття: %s";
    s[(int)Str::MsgAllReverted] = "Всі зміни скасовано, відновлено початковий стан.";
    s[(int)Str::MsgExportSuccess] = "Експортовано %d ресурсів до: %s";
    s[(int)Str::MsgExportError] = "Помилка експорту: %s";
    s[(int)Str::MsgImportSuccess] = "Імпортовано %d PNG з теки.";
    s[(int)Str::MsgImportError] = "Помилка імпорту: %s";
    s[(int)Str::MsgBuildSuccess] = "Збірка УСПІШНА: %s";
    s[(int)Str::MsgBuildError] = "Помилка збірки: %s";
    s[(int)Str::MsgFontNotLoaded] = "Помилка: шрифт не завантажено у генераторі!";
    s[(int)Str::MsgFontInjectedPreset] = "Впроваджено %d гліфів шрифту %s до пам'яті прошивки!";
    s[(int)Str::MsgFontInjectedAll] = "Застосовано гліфів: %d.";
    s[(int)Str::MsgFontExportSuccess] = "Експортовано %d PNG файлів до: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "Експортовано %d файлів для %s до: %s";
    s[(int)Str::MsgFontExportError] = "Помилка експорту: %s";
    s[(int)Str::ModalBuildReportTitle] = "Звіт про збірку";
    s[(int)Str::ModalBuildSuccessBanner] = "ЗБІРКА ТА ВЕРИФІКАЦІЯ ПРОШИВКИ УСПІШНІ!";
    s[(int)Str::ModalSavedFile] = "Збережений файл: %s";
    s[(int)Str::ModalOutputSize] = "Розмір вихідного файлу: %zu байт";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Змінені ресурси: %zu";
    s[(int)Str::ModalModColorGroups] = "Змінені групи кольорів: %zu";
    s[(int)Str::ModalPatchedRanges] = "Пропатчені діапазони: %zu у %zu апаратних записах";
    s[(int)Str::ModalUntouchedBytes] = "Незмінні байти: %zu байт ідентичні оригіналу";
    s[(int)Str::ModalReportNote] = "Детальний звіт збережено як .report.json поруч із прошивкою.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Відкрити в Провіднику";
    s[(int)Str::BtnCopyClipboard] = "Копіювати (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Вставити (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Скасувати";
    s[(int)Str::BtnRedo] = "Повторити";
    s[(int)Str::TooltipUndo] = "Скасувати: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Повторити: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Немає дій для скасування (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Немає дій для повторення (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Інструменти:";
    s[(int)Str::ToolPencil] = "Олівець";
    s[(int)Str::ToolEraser] = "Гумка";
    s[(int)Str::BtnInvertPixels] = "Інверсія";
    s[(int)Str::BtnClearPixels] = "Очистити";
    s[(int)Str::BtnFlipH] = "Відбити Г";
    s[(int)Str::BtnFlipV] = "Відбити В";
    s[(int)Str::MsgCopiedToClipboard] = "Зображення %s скопійовано в буфер.";
    s[(int)Str::MsgPastedFromClipboard] = "Зображення з буфера вставлено для %s!";
    s[(int)Str::MsgClipboardNoImage] = "У буфері обміну немає сумісного зображення.";
    s[(int)Str::MsgUndoPerformed] = "Скасовано: %s";
    s[(int)Str::MsgRedoPerformed] = "Повторено: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Прошивка: %s (SHA256: %s... | %zu записів | %zu змін)";
    s[(int)Str::BadgeProjectSummary] = "| Проєкт: %s (SHA256: %s... | %zu записів | %zu змін)";
    s[(int)Str::LabelCategory] = "Категорія";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Група: %s  |  Колір: Користувацький (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Група: %s  |  Колір: %s";
    s[(int)Str::LabelMetaHardware] = "Адреса Flash: 0x%08X  |  Розмір: %u x %u пкс  |  Байт: %zu  |  Записів: %zu";
    s[(int)Str::LabelPixelCoord] = "Піксель: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| ЛКМ: %s  |  ПКМ: Стерти";
    s[(int)Str::LabelDrawAction] = "Малювати";
    s[(int)Str::LabelEraseAction] = "Стерти";
    s[(int)Str::LabelFontPreset] = "Пресет";
    s[(int)Str::LabelFontPixelGrid] = "Піксельна сітка";
    s[(int)Str::MsgAppliedColorTheme] = "Застосовано колірну тему: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "Імпортовано %zu відповідних PNG з теки.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Прошивку відкрито через Drag & Drop: %s";
    s[(int)Str::MsgDragDropError] = "Помилка Drag & Drop: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Шрифт завантажено через Drag & Drop: %s";
    s[(int)Str::MsgFailedLoadFont] = "Не вдалося завантажити файл шрифту: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Скинутий PNG збігається з оригіналом для %s.";
    s[(int)Str::MsgStagedReplacement] = "Підготовлено заміну для %s з PNG!";
    s[(int)Str::MsgPngLoadError] = "Помилка завантаження PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Заміна для %s збігається з оригіналом.";
    s[(int)Str::MsgStagedReplacementCustom] = "Підготовлено заміну для %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "Підготовлено RGB565 заміну для %s!";
    s[(int)Str::MsgRestoredOriginal] = "Ресурс %s відновлено до оригіналу прошивки.";
    s[(int)Str::MsgSavedAssetPng] = "Збережено %s у: %s";
    s[(int)Str::MsgSaveError] = "Помилка збереження: %s";
    s[(int)Str::MsgSavedBootLogo] = "Збережено горизонтальний логотип у: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Помилка: не можна перезаписувати вихідну прошивку! Виберіть інше ім'я.";
    s[(int)Str::MsgColorPatchError] = "Помилка патчу кольорів: %s";
    s[(int)Str::MsgPackError] = "Помилка пакування: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Імпортовано %zu змінених PNG (%zu збіглися з оригіналом)";

    s[(int)Str::LabelBuiltin] = "вбудована";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Не вдалося завантажити вбудовану прошивку: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Завантажено вбудовану прошивку: %s";
    s[(int)Str::ThemeFactoryDefault] = "Заводська за замовчуванням";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Оригінальний заводський стиль Voopoo (червоний та жовтий)";
    s[(int)Str::ThemeCyberpunkNeon] = "Кіберпанк неон";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Електричний ціан, неоновий рожевий та фіолетовий";
    s[(int)Str::ThemeMatrixGreen] = "Матриця зелений";
    s[(int)Str::ThemeMatrixGreenDesc] = "ЕПТ-термінал фосфор та смарагдовий зелений";
    s[(int)Str::ThemeArcticIceBlue] = "Арктичний лід";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Льодовиковий ціан, глибоке небо та блакитне сяйво";
    s[(int)Str::ThemeAmberFire] = "Бурштиновий вогонь";
    s[(int)Str::ThemeAmberFireDesc] = "Лавовий помаранчевий, золоте полум'я та вугілля";
    s[(int)Str::ThemePurpleNebula] = "Пурпурова туманність";
    s[(int)Str::ThemePurpleNebulaDesc] = "Глибокий фіолетовий, лаванда та неон аметист";
    s[(int)Str::ThemeStealthDarkGray] = "Стелс темно-сірий";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Матовий графіт, сланець та збройовий метал";
    s[(int)Str::RoleAccentRed] = "Акцент / червоний режим";
    s[(int)Str::RoleAccentRedScope] = "Основний акцент UI у стандартних режимах — 8 місць";
    s[(int)Str::RoleAccentCyan] = "Акцент / ціан режим";
    s[(int)Str::RoleAccentCyanScope] = "Основний акцент UI у режимах SMART/RBA — 7 місць";
    s[(int)Str::RoleHighlightYellow] = "Підсвічування / жовтий";
    s[(int)Str::RoleHighlightYellowScope] = "Смуга очок та сервісні панелі — 4 місця";
    s[(int)Str::RoleRankBronze] = "Ранг / бронза";
    s[(int)Str::RoleRankBronzeScope] = "Палітра рангу Бронза";
    s[(int)Str::RoleRankGold] = "Ранг / золото";
    s[(int)Str::RoleRankGoldScope] = "Палітра рангу Золото";
    s[(int)Str::RoleRankIce] = "Ранг / платина (лід)";
    s[(int)Str::RoleRankIceScope] = "Палітра рангу Платина / Лід";
    s[(int)Str::RoleRankCyan] = "Ранг / ціан";
    s[(int)Str::RoleRankCyanScope] = "Палітра рангу Ціан";
    s[(int)Str::RoleRankOrange] = "Ранг / помаранчевий";
    s[(int)Str::RoleRankOrangeScope] = "Палітра рангу Помаранчевий";
    s[(int)Str::RoleRankRed] = "Ранг / червоний";
    s[(int)Str::RoleRankRedScope] = "Палітра рангу Червоний";
    s[(int)Str::RoleDigitsWattage] = "Цифри потужності";
    s[(int)Str::RoleDigitsWattageScope] = "Великі цифри потужності на головному екрані (19x40)";
    s[(int)Str::RoleDigitsStats] = "Цифри статистики";
    s[(int)Str::ColorPresetWhite] = "Білий (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Основний / Червоний (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Ціан (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Сервісний / Жовтий (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Бронза (#B57947)";
    s[(int)Str::ColorPresetGold] = "Золото (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Помаранчевий (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Зелений (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Основні цифри 0-9, 10 файлів)";
    s[(int)Str::FontPreset11x20] = "11x20 (Вторинні цифри 0-9, 10 файлів)";
    s[(int)Str::FontPreset7x13] = "7x13 (Малі статусні 0-9, A-F, 16 файлів)";
    s[(int)Str::FontPreset6x10] = "6x10 (Мікро статус 0-9, 10 файлів)";
    s[(int)Str::FontOs1x] = "1x (Прямий)";
    s[(int)Str::FontOs2x] = "2x (Швидкий)";
    s[(int)Str::FontOs4x] = "4x (Рекомендований)";
    s[(int)Str::FontOs8x] = "8x (Ультра)";
}

static void InitPolish() {
    auto& s = g_table[(int)Language::Polish];
    s[(int)Str::SupportFull] = "Pełna";
    s[(int)Str::SupportNone] = "Brak obsługi";
    s[(int)Str::SupportPalette] = "Paleta kolorów";
    s[(int)Str::SupportMonochrome] = "Ekran monochromatyczny; brak palety kolorów.";
    s[(int)Str::EmulatorHistoryDay] = "Dzień historii";
    s[(int)Str::EmulatorClockHour] = "Godziny";
    s[(int)Str::EmulatorClockMinute] = "Minuty";
    s[(int)Str::EmulatorClockField] = "Wybrane pole";
    s[(int)Str::MenuFile] = "Plik";
    s[(int)Str::MenuEdit] = "Edycja";
    s[(int)Str::MenuLanguage] = "Język";
    s[(int)Str::MenuHelp] = "Pomoc";
    s[(int)Str::UpdateTitle] = "Aktualizacje studia";
    s[(int)Str::UpdateVersion] = "Zainstalowana wersja: %s";
    s[(int)Str::UpdateCheck] = "Sprawdź aktualizacje";
    s[(int)Str::UpdateChecking] = "Sprawdzanie GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Masz najnowszą wersję.";
    s[(int)Str::UpdateDownload] = "Pobierz";
    s[(int)Str::UpdateDownloading] = "Pobieranie i weryfikacja...";
    s[(int)Str::UpdateInstall] = "Zainstaluj i uruchom ponownie";
    s[(int)Str::UpdateReady] = "Gotowe do instalacji.";
    s[(int)Str::UpdateChangelog] = "Lista zmian";
    s[(int)Str::UpdateFailed] = "Błąd aktualizacji";
    s[(int)Str::UpdateAvailable] = "Dostępna wersja: %s";
    s[(int)Str::MenuCreate] = "Nowy...";
    s[(int)Str::MenuOpen] = "Otwórz...";
    s[(int)Str::MenuImport] = "Importuj zasoby...";
    s[(int)Str::MenuExport] = "Eksportuj zasoby...";
    s[(int)Str::MenuBuild] = "Zbuduj firmware...";
    s[(int)Str::MenuExit] = "Zakończ";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Zasoby";
    s[(int)Str::TabFontGenerator] = "Generator czcionek";
    s[(int)Str::TabFirmwareInfo] = "O firmware";
    s[(int)Str::TabFirmwareComparison] = "Porównanie";
    s[(int)Str::CompareModeTwoFiles] = "Dwa pliki";
    s[(int)Str::CompareModeEditor] = "Edytor i plik";
    s[(int)Str::CompareLeft] = "Lewa";
    s[(int)Str::CompareRight] = "Prawa";
    s[(int)Str::CompareChooseFile] = "Wybierz plik...";
    s[(int)Str::CompareReadOnlyNote] = "Tylko podgląd; uwzględnia zmiany w edytorze.";
    s[(int)Str::CompareChooseSources] = "Wybierz oba źródła do porównania.";
    s[(int)Str::CompareEditorUnavailable] = "Brak firmware w edytorze.";
    s[(int)Str::CompareChanged] = "Zmieniony";
    s[(int)Str::CompareAdded] = "Dodany";
    s[(int)Str::CompareRemoved] = "Usunięty";
    s[(int)Str::CompareEqual] = "Identyczny";
    s[(int)Str::CompareIncompatible] = "Niezgodny";
    s[(int)Str::CompareDecodeError] = "Błąd dekodowania";
    s[(int)Str::CompareStatusFilter] = "Stan";
    s[(int)Str::CompareDifferencePreview] = "Różnica";
    s[(int)Str::CompareHighlightPixels] = "Podświetl różnice";
    s[(int)Str::CompareDifferentPixels] = "Różne piksele: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Brak zasobu";
    s[(int)Str::CompareIncompatibleHint] = "Różne typy lub rozmiary zasobów: porównanie pikseli niedostępne.";
    s[(int)Str::CompareImageUnavailable] = "Obraz niedostępny";
    s[(int)Str::CompareTextureError] = "Nie można utworzyć tekstury podglądu.";
    s[(int)Str::CompareAllStatuses] = "Wszystkie stany";
    s[(int)Str::TabFirmwareEmulator] = "Emulator";
    s[(int)Str::TabFirmwareLayout] = "Układ interfejsu";
    s[(int)Str::LayoutUnsupported] = "Edycja układu interfejsu jest niedostępna dla tego firmware.";
    s[(int)Str::LayoutNativeNote] = "Przeciągnij element lub wpisz X/Y. Podgląd wykonuje zmodyfikowany firmware; zmiany dotyczą wybranej rewizji sprzętu.";
    s[(int)Str::LayoutRevision] = "Rewizja sprzętu";
    s[(int)Str::LayoutCheckingRevisions] = "Sprawdzanie identycznych interfejsów…";
    s[(int)Str::LayoutAllRevisions] = "Wszystkie rewizje";
    s[(int)Str::LayoutRevisionGroup] = "Identyczny interfejs: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Rewizja: %s";
    s[(int)Str::LayoutResetScope] = "Resetuj wybrane rewizje";
    s[(int)Str::LayoutSelectionUnavailable] = "Brak zgodnego elementu we wszystkich wybranych rewizjach.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Elementy interfejsu";
    s[(int)Str::LayoutNoElements] = "Na tym ekranie nie ma ruchomych bloków.";
    s[(int)Str::LayoutSharedBlock] = "Ten blok współdzieli współrzędne z innymi ekranami.";
    s[(int)Str::LayoutPositionX] = "X (piksele)";
    s[(int)Str::LayoutPositionY] = "Y (piksele)";
    s[(int)Str::LayoutOffset] = "Przesunięcie: %+d, %+d px";
    s[(int)Str::LayoutGridStep] = "Krok: %d × %d px";
    s[(int)Str::LayoutGridSpacing] = "Odstęp siatki (px)";
    s[(int)Str::LayoutGridConstraint] = "Gdy żaden punkt siatki nie jest dostępny, pierwszeństwo mają natywny krok i granice współrzędnych.";
    s[(int)Str::LayoutSnapGrid] = "Przyciągaj do siatki";
    s[(int)Str::LayoutSnapElements] = "Przyciągaj do krawędzi i środków elementów";
    s[(int)Str::LayoutGuides] = "Linie pomocnicze ruchu";
    s[(int)Str::LayoutAlign] = "Wyrównaj do ekranu";
    s[(int)Str::LayoutAlignLeft] = "Lewo";
    s[(int)Str::LayoutAlignCenter] = "Środek X";
    s[(int)Str::LayoutAlignRight] = "Prawo";
    s[(int)Str::LayoutAlignTop] = "Góra";
    s[(int)Str::LayoutAlignMiddle] = "Środek Y";
    s[(int)Str::LayoutAlignBottom] = "Dół";
    s[(int)Str::LayoutNudgeHint] = "Strzałki: natywny krok piksela; Shift: większy krok. Kliknij ekran, aby ustawić fokus.";
    s[(int)Str::LayoutOverlays] = "Obrysy elementów";
    s[(int)Str::LayoutOverlapOverlay] = "Podświetl nakładanie";
    s[(int)Str::LayoutOverlaps] = "Przecinające się elementy";
    s[(int)Str::LayoutNoOverlaps] = "Brak przecięć prostokątów";
    s[(int)Str::LayoutDrawOrderUnknown] = "Natywna kolejność rysowania niepotwierdzona";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) jest rysowany później niż %s (#%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Kolejność wywołań głównego rysowania, nie stałe warstwy. Aktualizacje mogą się różnić; wcześniejszą zawartość pokrywają tylko faktycznie zapisane piksele.";
    s[(int)Str::LayoutDrawSequence] = "Natywna kolejność rysowania";
    s[(int)Str::LayoutDrawInterleaved] = "Wywołania przeplatają się; nakładanie zależy od części elementu.";
    s[(int)Str::EmulatorBefore] = "Przed (otwarte firmware)";
    s[(int)Str::EmulatorAfter] = "Po (bieżące zmiany)";
    s[(int)Str::EmulatorSideBySide] = "Obok siebie";
    s[(int)Str::EmulatorOriginalError] = "Błąd podglądu otwartego firmware: %s";
    s[(int)Str::MenuSaveProject] = "Zapisz projekt";
    s[(int)Str::MenuSaveProjectAs] = "Zapisz projekt jako...";
    s[(int)Str::MsgProjectSaved] = "Zapisano projekt: %s";
    s[(int)Str::MenuBuild] = "Eksportuj firmware";
    s[(int)Str::BtnOpenFirmware] = "Otwórz firmware lub projekt...";
    s[(int)Str::StartupOpenFile] = "Otwórz firmware lub projekt...";
    s[(int)Str::UnsavedPrompt] = "Zapisać projekt przed kontynuowaniem?";
    s[(int)Str::UnsavedPrompt] = "Zapisać projekt przed kontynuowaniem?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Resetuj element";
    s[(int)Str::LayoutResetRevision] = "Resetuj układ tej rewizji";
    s[(int)Str::LayoutResetAll] = "Resetuj wszystkie zmiany układu";
    s[(int)Str::LayoutPatchSites] = "Adresy poprawek";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Stan podglądu";
    s[(int)Str::LayoutMoveElement] = "Przesuń element interfejsu";
    s[(int)Str::LayoutRba] = "Tryb RBA";
    s[(int)Str::LayoutTemperatureControl] = "Kontrola temperatury";
    s[(int)Str::LayoutTemperature] = "Temperatura (°C)";
    s[(int)Str::LayoutRenderFailed] = "Natywny bufor obrazu jest niedostępny.";
    s[(int)Str::LayoutElementPower] = "Moc";
    s[(int)Str::LayoutElementBattery] = "Bateria";
    s[(int)Str::LayoutElementResistance] = "Rezystancja";
    s[(int)Str::LayoutElementDuration] = "Czas trwania";
    s[(int)Str::LayoutElementPuffs] = "Zaciągnięcia";
    s[(int)Str::LayoutElementMode] = "Tryb";
    s[(int)Str::LayoutElementStatus] = "Stan";
    s[(int)Str::LayoutElementScore] = "Wynik";
    s[(int)Str::LayoutElementStatistics] = "Statystyki";
    s[(int)Str::LayoutElementVoltage] = "Napięcie";
    s[(int)Str::LayoutElementGraph] = "Wykres";
    s[(int)Str::LayoutElementRank] = "Ranga";
    s[(int)Str::LayoutElementModeSelection] = "Wybór trybu";
    s[(int)Str::LayoutElementLogo] = "Logo";
    s[(int)Str::LayoutChangedElements] = "Zmienione elementy interfejsu";
    s[(int)Str::EmulatorSupported] = "Natywny podgląd UI wszystkich wbudowanych firmware DRAG.";
    s[(int)Str::EmulatorNativeNote] = "Natywne renderowanie z bieżącymi zmianami. Bez emulacji układu elektrycznego.";
    s[(int)Str::EmulatorWatts] = "Moc (W)";
    s[(int)Str::EmulatorPuffs] = "Zaciągnięcia";
    s[(int)Str::EmulatorBattery] = "Bateria (0–5)";
    s[(int)Str::EmulatorResistance] = "Opór (mΩ)";
    s[(int)Str::EmulatorDuration] = "Czas (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Blokada";
    s[(int)Str::EmulatorCharging] = "Ładowanie";
    s[(int)Str::EmulatorScore] = "Suma punktów";
    s[(int)Str::EmulatorScoreDisplay] = "Najlepszy wynik";
    s[(int)Str::EmulatorRedraw] = "Odśwież";
    s[(int)Str::EmulatorLandscape] = "Poziomo";
    s[(int)Str::EmulatorScreen] = "Ekran";
    s[(int)Str::EmulatorMainScreen] = "Ekran główny";
    s[(int)Str::FontSystemFonts] = "Czcionki systemowe";
    s[(int)Str::FontUserFonts] = "Czcionki użytkownika";
    s[(int)Str::EmulatorMenuItem] = "Pozycja menu";
    s[(int)Str::EmulatorMaterial] = "Materiał";
    s[(int)Str::EmulatorMenuConfirmed] = "Potwierdzono";
    s[(int)Str::EmulatorResources] = "Zasoby odczytane przez firmware";
    s[(int)Str::EmulatorNoResources] = "Brak odczytów zasobów katalogu.";
    s[(int)Str::EmulatorUidNote] = "Emulowany UID MCU, nie identyfikator podłączonego urządzenia.";
    s[(int)Str::EmulatorLoaderVersion] = "Wirtualna wersja bootloadera";
    s[(int)Str::EmulatorLoaderId] = "Wirtualne bajty CHIP-ID";
    s[(int)Str::EmulatorLoaderNote] = "Dane bootloadera są emulowane, nie odczytywane z firmware ani urządzenia.";
    s[(int)Str::TabFirmwarePatches] = "Łatki";
    s[(int)Str::PatchBootLogo] = "Ekran uruchamiania";
    s[(int)Str::PatchShutdownLogo] = "Ekran wyłączania";
    s[(int)Str::PatchDeviceInfo] = "Ekran CHIP ID / wersji";
    s[(int)Str::PatchDeviceInfoNote] = "Tylko obraz statyczny. Zastępuje ekran serwisowy, nie główny ekran mocy i baterii.";
    s[(int)Str::PatchLogoNote] = "Niezależne obrazy 80 × 160 dla wszystkich rewizji DRAG S Score / Factory. Nie sprawdzono na urządzeniu.";
    s[(int)Str::PatchUnsupported] = "Ta łatka wymaga firmware DRAG S Score lub Factory.";
    s[(int)Str::PatchEnabled] = "Włącz własne logo";
    s[(int)Str::PatchCanvas] = "Kompozycja · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Natywny podgląd ekranu";
    s[(int)Str::PatchPreviewNote] = "Natywny podgląd pokazuje zastosowane ustawienia, nie szkic kompozycji.";
    s[(int)Str::PatchFit] = "Dopasuj";
    s[(int)Str::PatchCover] = "Wypełnij";
    s[(int)Str::PatchStretch] = "Rozciągnij";
    s[(int)Str::PatchScale] = "Skala";
    s[(int)Str::PatchAnimation] = "Animacja wejścia";
    s[(int)Str::PatchNone] = "Brak";
    s[(int)Str::PatchFade] = "Przenikanie";
    s[(int)Str::PatchDuration] = "Czas (ms)";
    s[(int)Str::PatchAutoHardware] = "Automatycznie";
    s[(int)Str::PatchAnimationPhase] = "Faza animacji";
    s[(int)Str::PatchPlay] = "Odtwórz";
    s[(int)Str::EmulatorModeScreens] = "Ekrany";
    s[(int)Str::EmulatorModeDevice] = "Urządzenie";
    s[(int)Str::DeviceUnsupported] = "Tryb urządzenia obsługuje DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Firmware działa od Reset z wirtualnymi peryferiami. Grzanie jest symulowane; bezpieczeństwo elektryczne nie jest sprawdzane.";
    s[(int)Str::DeviceReset] = "Restart";
    s[(int)Str::DevicePause] = "Pauza";
    s[(int)Str::DeviceResume] = "Uruchom";
    s[(int)Str::DeviceStep] = "Krok";
    s[(int)Str::DeviceChanged] = "Bieżące zmiany nie są jeszcze załadowane.";
    s[(int)Str::DeviceApply] = "Restart ze zmianami";
    s[(int)Str::DeviceButtons] = "Przytrzymaj przyciski lub spację / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Grzałka podłączona";
    s[(int)Str::DeviceTime] = "Czas wirtualny";
    s[(int)Str::DeviceSleeping] = "Procesor uśpiony";
    s[(int)Str::DeviceLoading] = "Ładowanie urządzenia...";
    s[(int)Str::DeviceLog] = "Dostęp do peryferiów";
    s[(int)Str::BadgeNoFirmware] = "| [Nie otwarto firmware — przeciągnij .zwdf/.xwdf tutaj]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu elementów | %zu rekordów";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Otwórz firmware...";
    s[(int)Str::BtnExportAll] = "Eksportuj zasoby do folderu...";
    s[(int)Str::BtnImportFolder] = "Importuj folder z PNG...";
    s[(int)Str::BtnBuildFirmware] = " ZBUDUJ FIRMWARE ";
    s[(int)Str::BtnRevertAllStaged] = "Odrzuć wszystkie zmiany";
    s[(int)Str::GroupAll] = "Wszystkie grupy";
    s[(int)Str::GroupAlerts] = "Alerty (dialogi i ikony)";
    s[(int)Str::GroupFonts] = "Czcionki (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Wyniki i rangi";
    s[(int)Str::GroupStatusLabels] = "Status i etykiety";
    s[(int)Str::GroupControls] = "Kontrolki i bateria";
    s[(int)Str::GroupScreens] = "Ekrany i loga startowe";
    s[(int)Str::GroupAnimations] = "Animacje";
    s[(int)Str::PanelResourcesHeader] = "Zasoby";
    s[(int)Str::LabelSearch] = "Szukaj";
    s[(int)Str::SearchPlaceholder] = "filtruj po nazwie, id, adresie...";
    s[(int)Str::NoFirmwareLoadedList] = "Nie załadowano firmware.\nPlik > Nowy / Otwórz";
    s[(int)Str::NoResourcesMatchFilter] = "Brak zasobów pasujących do filtra.";
    s[(int)Str::PanelResourcePreviewHeader] = "Podgląd";
    s[(int)Str::SelectResourcePrompt] = "Wybierz zasób z listy po lewej, aby wyświetlić podgląd i edytować.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Grupa";
    s[(int)Str::LabelColor] = "Kolor";
    s[(int)Str::LabelColorCustom] = "Niestandardowy";
    s[(int)Str::LabelFlashAddress] = "Adres Flash";
    s[(int)Str::LabelDimensions] = "Wymiary";
    s[(int)Str::LabelBytes] = "Bajty";
    s[(int)Str::LabelRecords] = "Rekordów";
    s[(int)Str::StatusModifiedStaged] = "Status: ZMODYFIKOWANY (w pamięci)";
    s[(int)Str::StatusOriginal] = "Status: ORYGINALNY (z firmware)";
    s[(int)Str::StatusStockOriginal] = "Oryginał (zgodny ze wzorcem)";
    s[(int)Str::StatusCustomDiffers] = "Zmodyfikowany (różni się od wzorca)";
    s[(int)Str::BtnRestoreStock] = "Przywróć wzorzec";
    s[(int)Str::LabelCompareWithStock] = "Porównaj ze wzorcem";
    s[(int)Str::LabelActivePreview] = "Aktualny (aktywny):";
    s[(int)Str::LabelStockReferencePreview] = "Wzorzec fabryczny:";
    s[(int)Str::FilterOnlyCustom] = "Tylko inne niż wzorzec";
    s[(int)Str::MsgRestoredToStock] = "Przywrócono %s do wzorca fabrycznego.";
    s[(int)Str::BtnReplaceFromPng] = "Zastąp z PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Przywróć oryginał";
    s[(int)Str::BtnExportThisPng] = "Eksportuj PNG...";
    s[(int)Str::BtnExportHorizontal] = "Eksport poziomy (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Paleta kolorów i renderer (RGB565)";
    s[(int)Str::LabelTheme] = "Motyw";
    s[(int)Str::BtnApply] = "Zastosuj";
    s[(int)Str::BtnResetAll] = "Resetuj wszystko";
    s[(int)Str::LabelRole] = "Rola";
    s[(int)Str::BtnResetRole] = "Resetuj rolę";
    s[(int)Str::PromptLoadFwColors] = "Załaduj firmware, aby przeglądać i zmieniać kolory.";
    s[(int)Str::LabelPreviewTint] = "Kolor podglądu";
    s[(int)Str::InfoDigitsMonochrome] = "Cyfry to monochromatyczne maski. Kolor wyświetlania jest ustawiany przez kod firmware, nie jest przechowywany w glifie.";
    s[(int)Str::InfoPreviewNotSaved] = "Kolor podglądu nie jest zapisywany do firmware. Eksport RGB565 PNG zawiera ten kolor.";
    s[(int)Str::LabelPreviewOnly] = "Tylko podgląd:";
    s[(int)Str::LabelPresets] = "Presety...";
    s[(int)Str::BtnResetColor] = "Resetuj kolor";
    s[(int)Str::LabelZoom] = "Powiększenie";
    s[(int)Str::LabelPixelGrid] = "Siatka pikseli";
    s[(int)Str::LabelHorizontalPreview] = "Podgląd poziomy (136x22):";
    s[(int)Str::FontSec1Title] = "1. Wybór czcionki";
    s[(int)Str::FontSec1WindowsFont] = "Czcionka Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Przeglądaj plik TTF/OTF...";
    s[(int)Str::FontSec1ActiveFont] = "Aktywna czcionka: %s";
    s[(int)Str::FontSec2Title] = "2. Preset wyświetlania";
    s[(int)Str::FontSec3Title] = "3. Parametry renderowania";
    s[(int)Str::FontSec3Threshold] = "Próg";
    s[(int)Str::FontSec3ThresholdHint] = "Mniej = grubsza, Więcej = cieńsza";
    s[(int)Str::FontSec3Stretch] = "Rozciągnij do komórki";
    s[(int)Str::FontSec3Baseline] = "Wyrównanie bazowe";
    s[(int)Str::FontSec3CenterV] = "Wyśrodkuj pionowo";
    s[(int)Str::FontSec3PaddingX] = "Margines X";
    s[(int)Str::FontSec3PaddingY] = "Margines Y";
    s[(int)Str::FontSec3Oversampling] = "Nadpróbkowanie";
    s[(int)Str::FontSec4Title] = "4. Integracja z firmware";
    s[(int)Str::FontSec4ApplyPreset] = " > ZASTOSUJ GLIFY DO OTWARTEGO FIRMWARE < ";
    s[(int)Str::FontSec4ApplyAll] = "Zastosuj wszystkie czcionki";
    s[(int)Str::FontSec5Title] = "5. Eksport do PNG";
    s[(int)Str::FontSec5OutputFolder] = "Folder wyjściowy";
    s[(int)Str::FontSec5BrowseFolder] = "Przeglądaj folder...";
    s[(int)Str::FontSec5OpenExplorer] = "Otwórz w Eksploratorze";
    s[(int)Str::FontSec5ExportAllPreset] = "Eksportuj wszystkie presety";
    s[(int)Str::FontSec5ExportButton] = "Eksportuj 1-bitowe PNG na dysk";
    s[(int)Str::FontRightLivePreview] = "Podgląd czcionki na żywo";
    s[(int)Str::FontRightNoGlyphs] = "Nie załadowano glifów.";
    s[(int)Str::FontPresetInfo] = "Preset: %s (%dx%d px, %d glifów, rozmiar: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Specyfikacja pakietu";
    s[(int)Str::FwInfoNoFirmware] = "Nie załadowano firmware. Otwórz plik .zwdf/.xwdf w zakładce Studio zasobów.";
    s[(int)Str::FwInfoFilePath] = "Ścieżka pliku: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 źródła: %s";
    s[(int)Str::FwInfoPackageSize] = "Rozmiar pakietu: %zu bajtów";
    s[(int)Str::FwInfoHwRevisions] = "Rewizje sprzętowe: %zu rekordów";
    s[(int)Str::FwInfoRecognizedResources] = "Rozpoznane zasoby firmware: %zu elementów (czcionki, ikony, rangi, etykiety, kontrolki, ekrany)";
    s[(int)Str::FwInfoRecordsTable] = "Tabela rekordów sprzętowych";
    s[(int)Str::FwInfoColRecord] = "Rekord #";
    s[(int)Str::FwInfoColDevCode] = "Kod urządzenia";
    s[(int)Str::FwInfoColPayload] = "Rozmiar danych";
    s[(int)Str::FwInfoColImage] = "Rozmiar obrazu";
    s[(int)Str::FwInfoColFlashBase] = "Baza Flash";
    s[(int)Str::FwInfoColMSP] = "Wskaźnik stosu (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Wektor resetu";
    s[(int)Str::BtnRevertAllModifications] = "Cofnij wszystkie zmiany";
    s[(int)Str::MsgReadyOpenFw] = "Gotowe. Otwórz plik .zwdf/.xwdf firmware lub wygeneruj czcionki.";
    s[(int)Str::MsgSuccessOpened] = "Otwarto: %s";
    s[(int)Str::MsgOpenError] = "Błąd otwarcia: %s";
    s[(int)Str::MsgAllReverted] = "Wszystkie zmiany cofnięte do stanu oryginalnego firmware.";
    s[(int)Str::MsgExportSuccess] = "Wyeksportowano %d zasobów do: %s";
    s[(int)Str::MsgExportError] = "Błąd eksportu: %s";
    s[(int)Str::MsgImportSuccess] = "Zaimportowano %d PNG z folderu.";
    s[(int)Str::MsgImportError] = "Błąd importu: %s";
    s[(int)Str::MsgBuildSuccess] = "Budowanie SUKCES: %s";
    s[(int)Str::MsgBuildError] = "Błąd budowania: %s";
    s[(int)Str::MsgFontNotLoaded] = "Błąd: Nie załadowano czcionki w generatorze!";
    s[(int)Str::MsgFontInjectedPreset] = "Wstrzyknięto %d glifów czcionki %s do pamięci firmware!";
    s[(int)Str::MsgFontInjectedAll] = "Zastosowano glify: %d.";
    s[(int)Str::MsgFontExportSuccess] = "Wyeksportowano %d plików PNG do: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "Wyeksportowano %d plików dla %s do: %s";
    s[(int)Str::MsgFontExportError] = "Błąd eksportu: %s";
    s[(int)Str::ModalBuildReportTitle] = "Raport budowania";
    s[(int)Str::ModalBuildSuccessBanner] = "BUDOWANIE I WERYFIKACJA FIRMWARE POMYŚLNE!";
    s[(int)Str::ModalSavedFile] = "Zapisany plik: %s";
    s[(int)Str::ModalOutputSize] = "Rozmiar wyjściowy: %zu bajtów";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Zmodyfikowane zasoby: %zu";
    s[(int)Str::ModalModColorGroups] = "Zmodyfikowane grupy kolorów: %zu";
    s[(int)Str::ModalPatchedRanges] = "Zmodyfikowane zakresy bajtów: %zu w %zu rekordach";
    s[(int)Str::ModalUntouchedBytes] = "Niezmodyfikowane bajty: %zu bajtów identycznych z oryginałem";
    s[(int)Str::ModalReportNote] = "Szczegółowy raport zapisano jako .report.json obok firmware.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Otwórz w Eksploratorze";
    s[(int)Str::BtnCopyClipboard] = "Kopiuj (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Wklej (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Cofnij";
    s[(int)Str::BtnRedo] = "Ponów";
    s[(int)Str::TooltipUndo] = "Cofnij: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Ponów: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Brak akcji do cofnięcia (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Brak akcji do ponowienia (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Narzędzia:";
    s[(int)Str::ToolPencil] = "Ołówek";
    s[(int)Str::ToolEraser] = "Gumka";
    s[(int)Str::BtnInvertPixels] = "Odwróć";
    s[(int)Str::BtnClearPixels] = "Wyczyść";
    s[(int)Str::BtnFlipH] = "Obróć poz.";
    s[(int)Str::BtnFlipV] = "Obróć pion.";
    s[(int)Str::MsgCopiedToClipboard] = "Skopiowano obraz %s do schowka.";
    s[(int)Str::MsgPastedFromClipboard] = "Wklejono obraz ze schowka dla %s!";
    s[(int)Str::MsgClipboardNoImage] = "Brak zgodnego obrazu w schowku.";
    s[(int)Str::MsgUndoPerformed] = "Cofnięto: %s";
    s[(int)Str::MsgRedoPerformed] = "Ponowiono: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Oprogramowanie: %s (SHA256: %s... | %zu rekordów | %zu zm.)";
    s[(int)Str::BadgeProjectSummary] = "| Projekt: %s (SHA256: %s... | %zu rekordów | %zu zm.)";
    s[(int)Str::LabelCategory] = "Kategoria";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Grupa: %s  |  Kolor: Własny (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Grupa: %s  |  Kolor: %s";
    s[(int)Str::LabelMetaHardware] = "Adres Flash: 0x%08X  |  Wymiary: %u x %u px  |  Bajtów: %zu  |  Rekordów: %zu";
    s[(int)Str::LabelPixelCoord] = "Piksel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| LPM: %s  |  PPM: Wymaż";
    s[(int)Str::LabelDrawAction] = "Rysuj";
    s[(int)Str::LabelEraseAction] = "Wymaż";
    s[(int)Str::LabelFontPreset] = "Preset";
    s[(int)Str::LabelFontPixelGrid] = "Siatka pikseli";
    s[(int)Str::MsgAppliedColorTheme] = "Zastosowano motyw kolorystyczny: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "Zaimportowano %zu pasujących plików PNG z folderu.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Otwarto oprogramowanie metodą Drag & Drop: %s";
    s[(int)Str::MsgDragDropError] = "Błąd Drag & Drop: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Załadowano czcionkę metodą Drag & Drop: %s";
    s[(int)Str::MsgFailedLoadFont] = "Nie udało się załadować czcionki: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Przeciągnięty PNG jest identyczny z oryginałem dla %s.";
    s[(int)Str::MsgStagedReplacement] = "Przygotowano zamiennik dla %s z PNG!";
    s[(int)Str::MsgPngLoadError] = "Błąd ładowania PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Zamiennik dla %s jest identyczny z oryginałem.";
    s[(int)Str::MsgStagedReplacementCustom] = "Przygotowano zamiennik dla %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "Przygotowano zamiennik RGB565 dla %s!";
    s[(int)Str::MsgRestoredOriginal] = "Przywrócono zasób %s do stanu oryginalnego.";
    s[(int)Str::MsgSavedAssetPng] = "Zapisano %s w: %s";
    s[(int)Str::MsgSaveError] = "Błąd zapisu: %s";
    s[(int)Str::MsgSavedBootLogo] = "Zapisano logo poziome w: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Błąd: nie można nadpisać pliku źródłowego! Wybierz inną nazwę.";
    s[(int)Str::MsgColorPatchError] = "Błąd łatki kolorów: %s";
    s[(int)Str::MsgPackError] = "Błąd pakowania: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Zaimportowano %zu zmienionych PNG (%zu identycznych z oryginałem)";

    s[(int)Str::LabelBuiltin] = "wbudowane";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Nie można załadować wbudowanego oprogramowania: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Załadowano wbudowane oprogramowanie: %s";
    s[(int)Str::ThemeFactoryDefault] = "Domyślny fabryczny";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Oryginalny fabryczny styl Voopoo czerwono-żółty";
    s[(int)Str::ThemeCyberpunkNeon] = "Cyberpunk Neon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Elektryczny cyjan, neonowy róż i fiolet";
    s[(int)Str::ThemeMatrixGreen] = "Matrix Zielony";
    s[(int)Str::ThemeMatrixGreenDesc] = "Fosfor CRT i szmaragdowa zieleń";
    s[(int)Str::ThemeArcticIceBlue] = "Arktyczny lód";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Lodowcowy cyjan, błękit nieba i lazurowa poświata";
    s[(int)Str::ThemeAmberFire] = "Bursztynowy ogień";
    s[(int)Str::ThemeAmberFireDesc] = "Lawowy pomarańcz, złoty płomień i żar";
    s[(int)Str::ThemePurpleNebula] = "Fioletowa mgławica";
    s[(int)Str::ThemePurpleNebulaDesc] = "Głęboki fiolet, lawenda i neonowy ametyst";
    s[(int)Str::ThemeStealthDarkGray] = "Stealth ciemnoszary";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Matowy grafit, łupek i metal broni";
    s[(int)Str::RoleAccentRed] = "Akcent / tryb czerwony";
    s[(int)Str::RoleAccentRedScope] = "Główny akcent UI w trybach standardowych — 8 miejsc";
    s[(int)Str::RoleAccentCyan] = "Akcent / tryb cyjan";
    s[(int)Str::RoleAccentCyanScope] = "Główny akcent UI w trybach SMART/RBA — 7 miejsc";
    s[(int)Str::RoleHighlightYellow] = "Wyróżnienie / żółty";
    s[(int)Str::RoleHighlightYellowScope] = "Pasek punktacji i panele serwisowe — 4 miejsca";
    s[(int)Str::RoleRankBronze] = "Ranga / brąz";
    s[(int)Str::RoleRankBronzeScope] = "Paleta rangi Brąz";
    s[(int)Str::RoleRankGold] = "Ranga / złoto";
    s[(int)Str::RoleRankGoldScope] = "Paleta rangi Złoto";
    s[(int)Str::RoleRankIce] = "Ranga / platyna (lód)";
    s[(int)Str::RoleRankIceScope] = "Paleta rangi Platyna / Lód";
    s[(int)Str::RoleRankCyan] = "Ranga / cyjan";
    s[(int)Str::RoleRankCyanScope] = "Paleta rangi Cyjan";
    s[(int)Str::RoleRankOrange] = "Ranga / pomarańczowy";
    s[(int)Str::RoleRankOrangeScope] = "Paleta rangi Pomarańczowy";
    s[(int)Str::RoleRankRed] = "Ranga / czerwony";
    s[(int)Str::RoleRankRedScope] = "Paleta rangi Czerwony";
    s[(int)Str::RoleDigitsWattage] = "Cyfry mocy";
    s[(int)Str::RoleDigitsWattageScope] = "Duże cyfry mocy na ekranie głównym (19x40)";
    s[(int)Str::RoleDigitsStats] = "Cyfry statystyk";
    s[(int)Str::ColorPresetWhite] = "Biały (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Główny / Czerwony (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Cyjan (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Serwisowy / Żółty (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Brąz (#B57947)";
    s[(int)Str::ColorPresetGold] = "Złoto (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Pomarańczowy (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Zielony (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Główne cyfry 0-9, 10 plików)";
    s[(int)Str::FontPreset11x20] = "11x20 (Drugorzędne cyfry 0-9, 10 plików)";
    s[(int)Str::FontPreset7x13] = "7x13 (Mały status 0-9, A-F, 16 plików)";
    s[(int)Str::FontPreset6x10] = "6x10 (Mikro status 0-9, 10 plików)";
    s[(int)Str::FontOs1x] = "1x (Bezpośredni)";
    s[(int)Str::FontOs2x] = "2x (Szybki)";
    s[(int)Str::FontOs4x] = "4x (Zalecany)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

static void InitKazakh() {
    auto& s = g_table[(int)Language::Kazakh];
    s[(int)Str::SupportFull] = "Толық";
    s[(int)Str::SupportNone] = "Қолдау жоқ";
    s[(int)Str::SupportPalette] = "Түс палитрасы";
    s[(int)Str::SupportMonochrome] = "Монохромды экран; түс палитрасы жоқ.";
    s[(int)Str::EmulatorHistoryDay] = "Тарих күні";
    s[(int)Str::EmulatorClockHour] = "Сағат";
    s[(int)Str::EmulatorClockMinute] = "Минут";
    s[(int)Str::EmulatorClockField] = "Таңдалған өріс";
    s[(int)Str::MenuFile] = "Файл";
    s[(int)Str::MenuEdit] = "Өңдеу";
    s[(int)Str::MenuLanguage] = "Тіл";
    s[(int)Str::MenuHelp] = "Анықтама";
    s[(int)Str::UpdateTitle] = "Студия жаңартулары";
    s[(int)Str::UpdateVersion] = "Орнатылған нұсқа: %s";
    s[(int)Str::UpdateCheck] = "Жаңартуларды тексеру";
    s[(int)Str::UpdateChecking] = "GitHub Releases тексерілуде...";
    s[(int)Str::UpdateCurrent] = "Соңғы нұсқа орнатылған.";
    s[(int)Str::UpdateDownload] = "Жүктеп алу";
    s[(int)Str::UpdateDownloading] = "Жүктелуде және тексерілуде...";
    s[(int)Str::UpdateInstall] = "Орнату және қайта іске қосу";
    s[(int)Str::UpdateReady] = "Орнатуға дайын.";
    s[(int)Str::UpdateChangelog] = "Өзгерістер";
    s[(int)Str::UpdateFailed] = "Жаңарту қатесі";
    s[(int)Str::UpdateAvailable] = "Қолжетімді нұсқа: %s";
    s[(int)Str::MenuCreate] = "Жасау...";
    s[(int)Str::MenuOpen] = "Ашу...";
    s[(int)Str::MenuImport] = "Ресурстарды импорттау...";
    s[(int)Str::MenuExport] = "Ресурстарды экспорттау...";
    s[(int)Str::MenuBuild] = "Микробағдарламаны жинау...";
    s[(int)Str::MenuExit] = "Шығу";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ресурстар";
    s[(int)Str::TabFontGenerator] = "Қаріп генераторы";
    s[(int)Str::TabFirmwareInfo] = "Микробағдарлама туралы";
    s[(int)Str::TabFirmwareComparison] = "Салыстыру";
    s[(int)Str::CompareModeTwoFiles] = "Екі файл";
    s[(int)Str::CompareModeEditor] = "Редактор және файл";
    s[(int)Str::CompareLeft] = "Сол жақ";
    s[(int)Str::CompareRight] = "Оң жақ";
    s[(int)Str::CompareChooseFile] = "Файлды таңдау...";
    s[(int)Str::CompareReadOnlyNote] = "Тек көру; редактордағы өзгерістер ескеріледі.";
    s[(int)Str::CompareChooseSources] = "Салыстыру үшін екі дереккөзді таңдаңыз.";
    s[(int)Str::CompareEditorUnavailable] = "Редакторда микробағдарлама ашылмаған.";
    s[(int)Str::CompareChanged] = "Өзгертілген";
    s[(int)Str::CompareAdded] = "Қосылған";
    s[(int)Str::CompareRemoved] = "Жойылған";
    s[(int)Str::CompareEqual] = "Бірдей";
    s[(int)Str::CompareIncompatible] = "Сәйкес емес";
    s[(int)Str::CompareDecodeError] = "Декодтау қатесі";
    s[(int)Str::CompareStatusFilter] = "Күй";
    s[(int)Str::CompareDifferencePreview] = "Айырмашылық";
    s[(int)Str::CompareHighlightPixels] = "Айырмашылықтарды белгілеу";
    s[(int)Str::CompareDifferentPixels] = "Өзгеше пиксельдер: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Ресурс жоқ";
    s[(int)Str::CompareIncompatibleHint] = "Ресурс түрлері немесе өлшемдері әртүрлі: пиксельдік салыстыру қолжетімсіз.";
    s[(int)Str::CompareImageUnavailable] = "Кескін қолжетімсіз";
    s[(int)Str::CompareTextureError] = "Алдын ала көру текстурасын жасау мүмкін болмады.";
    s[(int)Str::CompareAllStatuses] = "Барлық күйлер";
    s[(int)Str::TabFirmwareEmulator] = "Эмулятор";
    s[(int)Str::TabFirmwareLayout] = "UI орналасуы";
    s[(int)Str::LayoutUnsupported] = "Бұл микробағдарлама үшін UI орналасуын өңдеу қолжетімсіз.";
    s[(int)Str::LayoutNativeNote] = "Элементті сүйреңіз немесе X/Y енгізіңіз. Алдын ала қарау өзгертілген микробағдарламаны орындайды; өзгерістер таңдалған аппараттық ревизияға қатысты.";
    s[(int)Str::LayoutRevision] = "Аппараттық ревизия";
    s[(int)Str::LayoutCheckingRevisions] = "Бірдей интерфейстер тексерілуде…";
    s[(int)Str::LayoutAllRevisions] = "Барлық ревизиялар";
    s[(int)Str::LayoutRevisionGroup] = "Бірдей UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Ревизия: %s";
    s[(int)Str::LayoutResetScope] = "Таңдалған ревизияларды қалпына келтіру";
    s[(int)Str::LayoutSelectionUnavailable] = "Таңдалған ревизиялар үшін үйлесімді элемент жоқ.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI элементтері";
    s[(int)Str::LayoutNoElements] = "Бұл экранда жылжытылатын блоктар жоқ.";
    s[(int)Str::LayoutSharedBlock] = "Бұл блоктың координаттары бірнеше экранға ортақ.";
    s[(int)Str::LayoutPositionX] = "X (пиксель)";
    s[(int)Str::LayoutPositionY] = "Y (пиксель)";
    s[(int)Str::LayoutOffset] = "Ығысу: %+d, %+d пикс.";
    s[(int)Str::LayoutGridStep] = "Қадам: %d × %d пикс.";
    s[(int)Str::LayoutGridSpacing] = "Тор қадамы (пикс.)";
    s[(int)Str::LayoutGridConstraint] = "Тор нүктесі қолжетімсіз болса, бастапқы координат қадамы мен шекаралары басым болады.";
    s[(int)Str::LayoutSnapGrid] = "Торға бекіту";
    s[(int)Str::LayoutSnapElements] = "Элементтердің жиектері мен ортасына бекіту";
    s[(int)Str::LayoutGuides] = "Жылжыту бағыттауыштары";
    s[(int)Str::LayoutAlign] = "Экранға туралау";
    s[(int)Str::LayoutAlignLeft] = "Солға";
    s[(int)Str::LayoutAlignCenter] = "X ортасы";
    s[(int)Str::LayoutAlignRight] = "Оңға";
    s[(int)Str::LayoutAlignTop] = "Жоғары";
    s[(int)Str::LayoutAlignMiddle] = "Y ортасы";
    s[(int)Str::LayoutAlignBottom] = "Төмен";
    s[(int)Str::LayoutNudgeHint] = "Бағыттар: бір пиксель; Shift: үлкен қадам. Фокус үшін экранды басыңыз.";
    s[(int)Str::LayoutOverlays] = "Элемент жиектері";
    s[(int)Str::LayoutOverlapOverlay] = "Қиылысуларды белгілеу";
    s[(int)Str::LayoutOverlaps] = "Қиылысатын элементтер";
    s[(int)Str::LayoutNoOverlaps] = "Тіктөртбұрыштар қиылыспайды";
    s[(int)Str::LayoutDrawOrderUnknown] = "Бастапқы сызу реті расталмаған";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) %s (#%d) элементінен кейін сызылады";
    s[(int)Str::LayoutDrawOrderNote] = "Бұл тұрақты қабаттар емес, негізгі сызу шақыруларының реті. Жаңарту кезінде ол өзгеруі мүмкін; алдыңғы мазмұнды тек жазылған пиксельдер жабады.";
    s[(int)Str::LayoutDrawSequence] = "Бастапқы сызу реті";
    s[(int)Str::LayoutDrawInterleaved] = "Шақырулар кезектеседі; қабаттасу элементтің бөлігіне байланысты.";
    s[(int)Str::EmulatorBefore] = "Бұрын (ашылған микробағдарлама)";
    s[(int)Str::EmulatorAfter] = "Кейін (ағымдағы өзгерістер)";
    s[(int)Str::EmulatorSideBySide] = "Қатар";
    s[(int)Str::EmulatorOriginalError] = "Алдын ала қарау қатесі: %s";
    s[(int)Str::MenuSaveProject] = "Жобаны сақтау";
    s[(int)Str::MenuSaveProjectAs] = "Жобаны басқаша сақтау...";
    s[(int)Str::MsgProjectSaved] = "Жоба сақталды: %s";
    s[(int)Str::MenuBuild] = "Микробағдарламаны экспорттау";
    s[(int)Str::BtnOpenFirmware] = "Микробағдарламаны немесе жобаны ашу...";
    s[(int)Str::StartupOpenFile] = "Микробағдарламаны немесе жобаны ашу...";
    s[(int)Str::UnsavedPrompt] = "Жалғастырмас бұрын жобаны сақтау керек пе?";
    s[(int)Str::UnsavedPrompt] = "Жалғастырмас бұрын жобаны сақтау керек пе?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Элементті қалпына келтіру";
    s[(int)Str::LayoutResetRevision] = "Осы ревизияның орналасуын қалпына келтіру";
    s[(int)Str::LayoutResetAll] = "Орналасудың барлық өзгерістерін жою";
    s[(int)Str::LayoutPatchSites] = "Патч мекенжайлары";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Алдын ала қарау күйі";
    s[(int)Str::LayoutMoveElement] = "UI элементін жылжыту";
    s[(int)Str::LayoutRba] = "RBA режимі";
    s[(int)Str::LayoutTemperatureControl] = "Температураны басқару";
    s[(int)Str::LayoutTemperature] = "Температура (°C)";
    s[(int)Str::LayoutRenderFailed] = "Бастапқы кадр буфері қолжетімсіз.";
    s[(int)Str::LayoutElementPower] = "Қуат";
    s[(int)Str::LayoutElementBattery] = "Батарея";
    s[(int)Str::LayoutElementResistance] = "Кедергі";
    s[(int)Str::LayoutElementDuration] = "Ұзақтық";
    s[(int)Str::LayoutElementPuffs] = "Тартымдар";
    s[(int)Str::LayoutElementMode] = "Режим";
    s[(int)Str::LayoutElementStatus] = "Күй";
    s[(int)Str::LayoutElementScore] = "Ұпай";
    s[(int)Str::LayoutElementStatistics] = "Статистика";
    s[(int)Str::LayoutElementVoltage] = "Кернеу";
    s[(int)Str::LayoutElementGraph] = "График";
    s[(int)Str::LayoutElementRank] = "Дәреже";
    s[(int)Str::LayoutElementModeSelection] = "Режимді таңдау";
    s[(int)Str::LayoutElementLogo] = "Логотип";
    s[(int)Str::LayoutChangedElements] = "Өзгертілген UI элементтері";
    s[(int)Str::EmulatorSupported] = "Барлық кірістірілген DRAG микробағдарламаларының UI алдын ала қарауы.";
    s[(int)Str::EmulatorNativeNote] = "Өзгерістерімен бастапқы сызу коды. Электр бөлігі эмуляцияланбайды.";
    s[(int)Str::EmulatorWatts] = "Қуат (Вт)";
    s[(int)Str::EmulatorPuffs] = "Тартулар";
    s[(int)Str::EmulatorBattery] = "Заряд (0–5)";
    s[(int)Str::EmulatorResistance] = "Кедергі (мОм)";
    s[(int)Str::EmulatorDuration] = "Ұзақтық (0,01 с)";
    s[(int)Str::EmulatorLocked] = "Құлыптау";
    s[(int)Str::EmulatorCharging] = "Зарядтау";
    s[(int)Str::EmulatorScore] = "Жалпы ұпай";
    s[(int)Str::EmulatorScoreDisplay] = "Үздік нәтиже";
    s[(int)Str::EmulatorRedraw] = "Қайта сызу";
    s[(int)Str::EmulatorLandscape] = "Көлденең";
    s[(int)Str::EmulatorScreen] = "Экран";
    s[(int)Str::EmulatorMainScreen] = "Негізгі экран";
    s[(int)Str::FontSystemFonts] = "Жүйелік қаріптер";
    s[(int)Str::FontUserFonts] = "Пайдаланушы қаріптері";
    s[(int)Str::EmulatorMenuItem] = "Мәзір тармағы";
    s[(int)Str::EmulatorMaterial] = "Материал";
    s[(int)Str::EmulatorMenuConfirmed] = "Расталды";
    s[(int)Str::EmulatorResources] = "Микробағдарлама оқыған ресурстар";
    s[(int)Str::EmulatorNoResources] = "Каталог ресурстарын оқу тіркелмеген.";
    s[(int)Str::EmulatorUidNote] = "Эмуляцияланған MCU UID, қосылған құрылғының ID емес.";
    s[(int)Str::EmulatorLoaderVersion] = "Жүктеуші нұсқасы (эмуляция)";
    s[(int)Str::EmulatorLoaderId] = "CHIP-ID байттары (эмуляция)";
    s[(int)Str::EmulatorLoaderNote] = "Жүктеуші деректері эмуляцияланады; микробағдарлама не құрылғыдан оқылмайды.";
    s[(int)Str::TabFirmwarePatches] = "Түзетулер";
    s[(int)Str::PatchBootLogo] = "Іске қосу экраны";
    s[(int)Str::PatchShutdownLogo] = "Өшіру экраны";
    s[(int)Str::PatchDeviceInfo] = "CHIP ID / нұсқа экраны";
    s[(int)Str::PatchDeviceInfoNote] = "Тек статикалық сурет. Қызметтік экранды ауыстырады, негізгі қуат пен батарея экранын өзгертпейді.";
    s[(int)Str::PatchLogoNote] = "Барлық DRAG S Score / Factory нұсқаларына тәуелсіз 80 × 160 суреттер. Құрылғыда тексерілмеген.";
    s[(int)Str::PatchUnsupported] = "Бұл түзетуге DRAG S Score не Factory микробағдарламасы қажет.";
    s[(int)Str::PatchEnabled] = "Өз логотипін қосу";
    s[(int)Str::PatchCanvas] = "Композиция · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Экранның нативті көрінісі";
    s[(int)Str::PatchPreviewNote] = "Нативті көрініс композиция жобасын емес, қолданылған параметрлерді көрсетеді.";
    s[(int)Str::PatchFit] = "Сыйғызу";
    s[(int)Str::PatchCover] = "Толтыру";
    s[(int)Str::PatchStretch] = "Созу";
    s[(int)Str::PatchScale] = "Масштаб";
    s[(int)Str::PatchAnimation] = "Пайда болу анимациясы";
    s[(int)Str::PatchNone] = "Жоқ";
    s[(int)Str::PatchFade] = "Біртіндеп пайда болу";
    s[(int)Str::PatchDuration] = "Ұзақтық (мс)";
    s[(int)Str::PatchAutoHardware] = "Автоматты";
    s[(int)Str::PatchAnimationPhase] = "Анимация кезеңі";
    s[(int)Str::PatchPlay] = "Ойнату";
    s[(int)Str::EmulatorModeScreens] = "Экрандар";
    s[(int)Str::EmulatorModeDevice] = "Құрылғы";
    s[(int)Str::DeviceUnsupported] = "Құрылғы режимі DRAG S Score / HW 3103 қолдайды.";
    s[(int)Str::DeviceNote] = "Микробағдарлама виртуалды перифериямен Reset-тен орындалады. Қыздыру модельденеді; электр қауіпсіздігі тексерілмейді.";
    s[(int)Str::DeviceReset] = "Қайта іске қосу";
    s[(int)Str::DevicePause] = "Үзіліс";
    s[(int)Str::DeviceResume] = "Іске қосу";
    s[(int)Str::DeviceStep] = "Қадам";
    s[(int)Str::DeviceChanged] = "Ағымдағы өзгерістер құрылғыға жүктелмеген.";
    s[(int)Str::DeviceApply] = "Өзгерістермен қайта қосу";
    s[(int)Str::DeviceButtons] = "Түймелерді не бос орын / ↑ / ↓ пернелерін ұстаңыз.";
    s[(int)Str::DeviceAtomizerConnected] = "Спираль қосылған";
    s[(int)Str::DeviceTime] = "Виртуалды уақыт";
    s[(int)Str::DeviceSleeping] = "Процессор ұйқыда";
    s[(int)Str::DeviceLoading] = "Құрылғы жүктелуде...";
    s[(int)Str::DeviceLog] = "Периферияға қатынау";
    s[(int)Str::BadgeNoFirmware] = "| [Микробағдарлама ашылмаған — .zwdf/.xwdf файлын сүйреңіз]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu элемент | %zu жазба";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Микробағдарлама ашу...";
    s[(int)Str::BtnExportAll] = "Барлық ресурстарды қалтаға экспорттау...";
    s[(int)Str::BtnImportFolder] = "PNG қалтасын импорттау...";
    s[(int)Str::BtnBuildFirmware] = " ЖАҢА МИКРОБАҒДАРЛАМА ";
    s[(int)Str::BtnRevertAllStaged] = "Барлық өзгерістерді қайтару";
    s[(int)Str::GroupAll] = "Барлық топтар";
    s[(int)Str::GroupAlerts] = "Ескертулер (диалогтар мен белгілер)";
    s[(int)Str::GroupFonts] = "Қаріптер (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Ұпайлар мен дәрежелер";
    s[(int)Str::GroupStatusLabels] = "Күй және жазулар";
    s[(int)Str::GroupControls] = "Басқару және батарея";
    s[(int)Str::GroupScreens] = "Экрандар мен жүктеу логотиптері";
    s[(int)Str::GroupAnimations] = "Анимациялар";
    s[(int)Str::PanelResourcesHeader] = "Ресурстар";
    s[(int)Str::LabelSearch] = "Іздеу";
    s[(int)Str::SearchPlaceholder] = "атау, id, адрес бойынша сүзу...";
    s[(int)Str::NoFirmwareLoadedList] = "Микробағдарлама жүктелмеген.\nФайл > Жасау / Ашу";
    s[(int)Str::NoResourcesMatchFilter] = "Сүзгіге сәйкес ресурстар табылмады.";
    s[(int)Str::PanelResourcePreviewHeader] = "Алдын ала қарау";
    s[(int)Str::SelectResourcePrompt] = "Алдын ала қарау және өңдеу үшін сол жақтағы тізімнен ресурсты таңдаңыз.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Топ";
    s[(int)Str::LabelColor] = "Түс";
    s[(int)Str::LabelColorCustom] = "Реттелмелі";
    s[(int)Str::LabelFlashAddress] = "Flash адресі";
    s[(int)Str::LabelDimensions] = "Өлшемдер";
    s[(int)Str::LabelBytes] = "Байт";
    s[(int)Str::LabelRecords] = "Жазбалар";
    s[(int)Str::StatusModifiedStaged] = "Күйі: ӨЗГЕРТІЛГЕН (жадта)";
    s[(int)Str::StatusOriginal] = "Күйі: ТҮПНҰСҚА (микробағдарламадан)";
    s[(int)Str::StatusStockOriginal] = "Түпнұсқа (эталонмен сәйкес)";
    s[(int)Str::StatusCustomDiffers] = "Өзгертілген (эталоннан өзгеше)";
    s[(int)Str::BtnRestoreStock] = "Эталоннан қалпына келтіру";
    s[(int)Str::LabelCompareWithStock] = "Эталонмен салыстыру";
    s[(int)Str::LabelActivePreview] = "Ағымдағы (белсенді):";
    s[(int)Str::LabelStockReferencePreview] = "Эталон (зауыттық):";
    s[(int)Str::FilterOnlyCustom] = "Тек эталоннан өзгешелер";
    s[(int)Str::MsgRestoredToStock] = "%s ресурсы зауыттық эталоннан қалпына келтірілді.";
    s[(int)Str::BtnReplaceFromPng] = "PNG-ден ауыстыру...";
    s[(int)Str::BtnRestoreOriginal] = "Түпнұсқаны қалпына келтіру";
    s[(int)Str::BtnExportThisPng] = "PNG экспорттау...";
    s[(int)Str::BtnExportHorizontal] = "Көлденең экспорт (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Түс палитрасы және рендер (RGB565)";
    s[(int)Str::LabelTheme] = "Тақырып";
    s[(int)Str::BtnApply] = "Қолдану";
    s[(int)Str::BtnResetAll] = "Барлығын қалпына келтіру";
    s[(int)Str::LabelRole] = "Рөл";
    s[(int)Str::BtnResetRole] = "Рөлді қалпына келтіру";
    s[(int)Str::PromptLoadFwColors] = "Түстерді көру және өзгерту үшін микробағдарламаны жүктеңіз.";
    s[(int)Str::LabelPreviewTint] = "Алдын ала қарау түсі";
    s[(int)Str::InfoDigitsMonochrome] = "Сандар — монохромды маскалар. Көрсету түсі микробағдарлама кодымен белгіленеді, гліфте сақталмайды.";
    s[(int)Str::InfoPreviewNotSaved] = "Алдын ала қарау түсі микробағдарламаға сақталмайды. RGB565 PNG экспортында бұл түс бар.";
    s[(int)Str::LabelPreviewOnly] = "Тек алдын ала қарау:";
    s[(int)Str::LabelPresets] = "Пресеттер...";
    s[(int)Str::BtnResetColor] = "Түсті қалпына келтіру";
    s[(int)Str::LabelZoom] = "Масштаб";
    s[(int)Str::LabelPixelGrid] = "Пиксель торы";
    s[(int)Str::LabelHorizontalPreview] = "Көлденең алдын ала қарау (136x22):";
    s[(int)Str::FontSec1Title] = "1. Қаріпті таңдау";
    s[(int)Str::FontSec1WindowsFont] = "Windows қаріпі";
    s[(int)Str::FontSec1BrowseTtf] = "TTF/OTF файлын шолу...";
    s[(int)Str::FontSec1ActiveFont] = "Белсенді қаріп: %s";
    s[(int)Str::FontSec2Title] = "2. Көрсету пресеті";
    s[(int)Str::FontSec3Title] = "3. Рендер параметрлері";
    s[(int)Str::FontSec3Threshold] = "Шекті мән";
    s[(int)Str::FontSec3ThresholdHint] = "Кем = жуан, Көп = жіңішке";
    s[(int)Str::FontSec3Stretch] = "Ұяшыққа созу";
    s[(int)Str::FontSec3Baseline] = "Негізгі сызық бойынша туралау";
    s[(int)Str::FontSec3CenterV] = "Тігінен ортаға";
    s[(int)Str::FontSec3PaddingX] = "Шегініс X";
    s[(int)Str::FontSec3PaddingY] = "Шегініс Y";
    s[(int)Str::FontSec3Oversampling] = "Артық сынамалау";
    s[(int)Str::FontSec4Title] = "4. Микробағдарламаға біріктіру";
    s[(int)Str::FontSec4ApplyPreset] = " > АШЫЛҒАН МИКРОБАҒДАРЛАМАҒА ГЛІФТЕРДІ ҚОЛДАНУ < ";
    s[(int)Str::FontSec4ApplyAll] = "Барлық қаріптерді қолдану";
    s[(int)Str::FontSec5Title] = "5. PNG-ге экспорттау";
    s[(int)Str::FontSec5OutputFolder] = "Шығыс қалтасы";
    s[(int)Str::FontSec5BrowseFolder] = "Қалтаны шолу...";
    s[(int)Str::FontSec5OpenExplorer] = "Жетектеуіште ашу";
    s[(int)Str::FontSec5ExportAllPreset] = "Барлық пресеттерді экспорттау";
    s[(int)Str::FontSec5ExportButton] = "1-бит PNG дискіге экспорттау";
    s[(int)Str::FontRightLivePreview] = "Қаріпті тікелей қарау";
    s[(int)Str::FontRightNoGlyphs] = "Гліфтер жүктелмеген.";
    s[(int)Str::FontPresetInfo] = "Пресет: %s (%dx%d пк, %d гліф, өлшем: %.1f пк)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Пакет спецификациясы";
    s[(int)Str::FwInfoNoFirmware] = "Микробағдарлама жүктелмеген. «Ресурстар студиясы» қойындысында .zwdf/.xwdf файлын ашыңыз.";
    s[(int)Str::FwInfoFilePath] = "Файл жолы: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 көзі: %s";
    s[(int)Str::FwInfoPackageSize] = "Пакет өлшемі: %zu байт";
    s[(int)Str::FwInfoHwRevisions] = "Аппараттық нұсқалар: %zu жазба";
    s[(int)Str::FwInfoRecognizedResources] = "Танылған ресурстар: %zu элемент (қаріптер, белгішелер, дәрежелер, жазулар, басқару элементтері, экрандар)";
    s[(int)Str::FwInfoRecordsTable] = "Аппараттық жазбалар кестесі";
    s[(int)Str::FwInfoColRecord] = "Жазба №";
    s[(int)Str::FwInfoColDevCode] = "Құрылғы коды";
    s[(int)Str::FwInfoColPayload] = "Деректер өлшемі";
    s[(int)Str::FwInfoColImage] = "Бейне өлшемі";
    s[(int)Str::FwInfoColFlashBase] = "Flash базалық адресі";
    s[(int)Str::FwInfoColMSP] = "Стек көрсеткіші (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Қалпына келтіру векторы";
    s[(int)Str::BtnRevertAllModifications] = "Барлық өзгерістерді қайтару";
    s[(int)Str::MsgReadyOpenFw] = "Дайын. .zwdf/.xwdf микробағдарлама файлын ашыңыз немесе қаріптерді жасаңыз.";
    s[(int)Str::MsgSuccessOpened] = "Ашылды: %s";
    s[(int)Str::MsgOpenError] = "Ашу қатесі: %s";
    s[(int)Str::MsgAllReverted] = "Барлық өзгерістер қайтарылды, түпнұсқа күй қалпына келтірілді.";
    s[(int)Str::MsgExportSuccess] = "%d ресурс экспортталды: %s";
    s[(int)Str::MsgExportError] = "Экспорт қатесі: %s";
    s[(int)Str::MsgImportSuccess] = "Қалтадан %d PNG импортталды.";
    s[(int)Str::MsgImportError] = "Импорт қатесі: %s";
    s[(int)Str::MsgBuildSuccess] = "Құрастыру СӘТТІ: %s";
    s[(int)Str::MsgBuildError] = "Құрастыру қатесі: %s";
    s[(int)Str::MsgFontNotLoaded] = "Қате: генераторда қаріп жүктелмеген!";
    s[(int)Str::MsgFontInjectedPreset] = "%d гліф %s микробағдарлама жадына тікелей енгізілді!";
    s[(int)Str::MsgFontInjectedAll] = "Қолданылған глифтер: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d PNG файл экспортталды: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d файл %s үшін экспортталды: %s";
    s[(int)Str::MsgFontExportError] = "Экспорт қатесі: %s";
    s[(int)Str::ModalBuildReportTitle] = "Құрастыру есебі";
    s[(int)Str::ModalBuildSuccessBanner] = "МИКРОБАҒДАРЛАМА ҚҰРАСТЫРЫЛДЫ ЖӘНЕ ТЕКСЕРІЛДІ!";
    s[(int)Str::ModalSavedFile] = "Сақталған файл: %s";
    s[(int)Str::ModalOutputSize] = "Шығыс өлшемі: %zu байт";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Өзгертілген ресурстар: %zu";
    s[(int)Str::ModalModColorGroups] = "Өзгертілген түс топтары: %zu";
    s[(int)Str::ModalPatchedRanges] = "Патчталған диапазондар: %zu %zu аппараттық жазбада";
    s[(int)Str::ModalUntouchedBytes] = "Тексерілген байттар: %zu байт түпнұсқамен бірдей";
    s[(int)Str::ModalReportNote] = "Толық есеп .report.json ретінде микробағдарлама жанына сақталды.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Жетектеуіште ашу";
    s[(int)Str::BtnCopyClipboard] = "Көшіру (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Қою (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Болдырмау";
    s[(int)Str::BtnRedo] = "Қайталау";
    s[(int)Str::TooltipUndo] = "Болдырмау: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Қайталау: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Болдырмайтын әрекет жоқ (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Қайталайтын әрекет жоқ (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Құралдар:";
    s[(int)Str::ToolPencil] = "Қарындаш";
    s[(int)Str::ToolEraser] = "Өшіргіш";
    s[(int)Str::BtnInvertPixels] = "Инверсия";
    s[(int)Str::BtnClearPixels] = "Тазалау";
    s[(int)Str::BtnFlipH] = "Көлденең аудару";
    s[(int)Str::BtnFlipV] = "Тігінен аудару";
    s[(int)Str::MsgCopiedToClipboard] = "%s суреті алмасу буферіне көшірілді.";
    s[(int)Str::MsgPastedFromClipboard] = "%s үшін алмасу буферінен сурет қойылды!";
    s[(int)Str::MsgClipboardNoImage] = "Алмасу буферінде жарамды сурет табылмады.";
    s[(int)Str::MsgUndoPerformed] = "Болдырылмады: %s";
    s[(int)Str::MsgRedoPerformed] = "Қайталанды: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Микробағдарлама: %s (SHA256: %s... | %zu жазба | %zu өзг.)";
    s[(int)Str::BadgeProjectSummary] = "| Жоба: %s (SHA256: %s... | %zu жазба | %zu өзг.)";
    s[(int)Str::LabelCategory] = "Санат";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Топ: %s  |  Түс: Таңдамалы (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Топ: %s  |  Түс: %s";
    s[(int)Str::LabelMetaHardware] = "Flash мекенжайы: 0x%08X  |  Өлшемі: %u x %u пкс  |  Байт: %zu  |  Жазбалар: %zu";
    s[(int)Str::LabelPixelCoord] = "Пиксель: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Сол жақ: %s  |  Оң жақ: Өшіру";
    s[(int)Str::LabelDrawAction] = "Сурет салу";
    s[(int)Str::LabelEraseAction] = "Өшіру";
    s[(int)Str::LabelFontPreset] = "Пресет";
    s[(int)Str::LabelFontPixelGrid] = "Пиксельдік тор";
    s[(int)Str::MsgAppliedColorTheme] = "Түс тақырыбы қолданылды: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "Қапшықтан %zu сәйкес PNG импортталды.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Микробағдарлама Drag & Drop арқылы ашылды: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop қатесі: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Қаріп Drag & Drop арқылы жүктелді: %s";
    s[(int)Str::MsgFailedLoadFont] = "Қаріп файлын жүктеу сәтсіз аяқталды: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Тасталған PNG %s үшін түпнұсқамен сәйкес келеді.";
    s[(int)Str::MsgStagedReplacement] = "%s үшін PNG-ден ауыстыру дайындалды!";
    s[(int)Str::MsgPngLoadError] = "PNG жүктеу қатесі: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "%s ауыстыруы түпнұсқамен сәйкес келеді.";
    s[(int)Str::MsgStagedReplacementCustom] = "%s үшін ауыстыру дайындалды!";
    s[(int)Str::MsgStagedRgbReplacement] = "%s үшін RGB565 ауыстыруы дайындалды!";
    s[(int)Str::MsgRestoredOriginal] = "%s ресурсын түпнұсқаға қалпына келтірілді.";
    s[(int)Str::MsgSavedAssetPng] = "%s сақталды: %s";
    s[(int)Str::MsgSaveError] = "Сақтау қатесі: %s";
    s[(int)Str::MsgSavedBootLogo] = "Көлденең жүктеу логотипі сақталды: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Қате: бастапқы микробағдарламаны қайта жазу мүмкін емес! Басқа атау таңдаңыз.";
    s[(int)Str::MsgColorPatchError] = "Түс патчының қатесі: %s";
    s[(int)Str::MsgPackError] = "Буып-түю қатесі: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu өзгертілген PNG импортталды (%zu түпнұсқамен сәйкес)";

    s[(int)Str::LabelBuiltin] = "ендірілген";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Ендірілген бағдарламалық жасақтаманы жүктеу мүмкін болмады: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Ендірілген бағдарламалық жасақтама жүктелді: %s";
    s[(int)Str::ThemeFactoryDefault] = "Зауыттық әдепкі";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Voopoo зауыттық түпнұсқа стилі (қызыл және сары)";
    s[(int)Str::ThemeCyberpunkNeon] = "Киберпанк неон";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Электрлік циан, неонды қызғылт және күлгін";
    s[(int)Str::ThemeMatrixGreen] = "Матрица жасыл";
    s[(int)Str::ThemeMatrixGreenDesc] = "ЭЛТ-терминал фосфоры және зүбәржат жасыл";
    s[(int)Str::ThemeArcticIceBlue] = "Арктикалық мұз";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Мұздық циан, терең аспан және көгілдір жарқыл";
    s[(int)Str::ThemeAmberFire] = "Кәріптас жалын";
    s[(int)Str::ThemeAmberFireDesc] = "Лавалық қызғылт сары, алтын жалын және шоқ";
    s[(int)Str::ThemePurpleNebula] = "Күлгін тұмандық";
    s[(int)Str::ThemePurpleNebulaDesc] = "Терең күлгін, лаванда және неон аметист";
    s[(int)Str::ThemeStealthDarkGray] = "Стелс қою сұр";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Күңгірт графит, тақтатас және қару металы";
    s[(int)Str::RoleAccentRed] = "Экцент / қызыл режим";
    s[(int)Str::RoleAccentRedScope] = "Стандартты режимдердегі UI негізгі екпіні — 8 орын";
    s[(int)Str::RoleAccentCyan] = "Екпін / циан режимі";
    s[(int)Str::RoleAccentCyanScope] = "SMART/RBA күйлеріндегі UI негізгі екпіні — 7 орын";
    s[(int)Str::RoleHighlightYellow] = "Ерекшелеу / сары";
    s[(int)Str::RoleHighlightYellowScope] = "Ұпай жолағы және қызметтік панельдер — 4 орын";
    s[(int)Str::RoleRankBronze] = "Дәреже / қола";
    s[(int)Str::RoleRankBronzeScope] = "Қола дәреже палитрасы";
    s[(int)Str::RoleRankGold] = "Дәреже / алтын";
    s[(int)Str::RoleRankGoldScope] = "Алтын дәреже палитрасы";
    s[(int)Str::RoleRankIce] = "Дәреже / платина (мұз)";
    s[(int)Str::RoleRankIceScope] = "Платина / Мұз дәреже палитрасы";
    s[(int)Str::RoleRankCyan] = "Дәреже / циан";
    s[(int)Str::RoleRankCyanScope] = "Циан дәреже палитрасы";
    s[(int)Str::RoleRankOrange] = "Дәреже / қызғылт сары";
    s[(int)Str::RoleRankOrangeScope] = "Қызғылт сары дәреже палитрасы";
    s[(int)Str::RoleRankRed] = "Дәреже / қызыл";
    s[(int)Str::RoleRankRedScope] = "Қызыл дәреже палитрасы";
    s[(int)Str::RoleDigitsWattage] = "Қуат сандары";
    s[(int)Str::RoleDigitsWattageScope] = "Басты экрандағы ірі қуат көрсеткіші (19x40)";
    s[(int)Str::RoleDigitsStats] = "Статистика сандары";
    s[(int)Str::ColorPresetWhite] = "Ақ (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Негізгі / Қызыл (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Циан (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Қызметтік / Сары (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Қола (#B57947)";
    s[(int)Str::ColorPresetGold] = "Алтын (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Қызғылт сары (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Жасыл (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Негізгі сандар 0-9, 10 файл)";
    s[(int)Str::FontPreset11x20] = "11x20 (Екінші сандар 0-9, 10 файл)";
    s[(int)Str::FontPreset7x13] = "7x13 (Шағын күй 0-9, A-F, 16 файл)";
    s[(int)Str::FontPreset6x10] = "6x10 (Микро күй 0-9, 10 файл)";
    s[(int)Str::FontOs1x] = "1x (Тікелей)";
    s[(int)Str::FontOs2x] = "2x (Жылдам)";
    s[(int)Str::FontOs4x] = "4x (Ұсынылатын)";
    s[(int)Str::FontOs8x] = "8x (Ультра)";
}

static void InitGerman() {
    auto& s = g_table[(int)Language::German];
    s[(int)Str::SupportFull] = "Vollständig";
    s[(int)Str::SupportNone] = "Nicht unterstützt";
    s[(int)Str::SupportPalette] = "Farbpalette";
    s[(int)Str::SupportMonochrome] = "Monochromes Display; keine Farbpalette.";
    s[(int)Str::EmulatorHistoryDay] = "Verlaufstag";
    s[(int)Str::EmulatorClockHour] = "Stunden";
    s[(int)Str::EmulatorClockMinute] = "Minuten";
    s[(int)Str::EmulatorClockField] = "Ausgewähltes Feld";
    s[(int)Str::MenuFile] = "Datei";
    s[(int)Str::MenuEdit] = "Bearbeiten";
    s[(int)Str::MenuLanguage] = "Sprache";
    s[(int)Str::MenuHelp] = "Hilfe";
    s[(int)Str::UpdateTitle] = "Studio-Updates";
    s[(int)Str::UpdateVersion] = "Installierte Version: %s";
    s[(int)Str::UpdateCheck] = "Nach Updates suchen";
    s[(int)Str::UpdateChecking] = "GitHub Releases wird geprüft...";
    s[(int)Str::UpdateCurrent] = "Die neueste Version ist installiert.";
    s[(int)Str::UpdateDownload] = "Herunterladen";
    s[(int)Str::UpdateDownloading] = "Wird heruntergeladen und geprüft...";
    s[(int)Str::UpdateInstall] = "Installieren und neu starten";
    s[(int)Str::UpdateReady] = "Bereit zur Installation.";
    s[(int)Str::UpdateChangelog] = "Änderungen";
    s[(int)Str::UpdateFailed] = "Update fehlgeschlagen";
    s[(int)Str::UpdateAvailable] = "Verfügbare Version: %s";
    s[(int)Str::MenuCreate] = "Neu...";
    s[(int)Str::MenuOpen] = "Öffnen...";
    s[(int)Str::MenuImport] = "Ressourcen importieren...";
    s[(int)Str::MenuExport] = "Ressourcen exportieren...";
    s[(int)Str::MenuBuild] = "Firmware erstellen...";
    s[(int)Str::MenuExit] = "Beenden";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ressourcen";
    s[(int)Str::TabFontGenerator] = "Schriftgenerator";
    s[(int)Str::TabFirmwareInfo] = "Über die Firmware";
    s[(int)Str::TabFirmwareComparison] = "Vergleich";
    s[(int)Str::CompareModeTwoFiles] = "Zwei Dateien";
    s[(int)Str::CompareModeEditor] = "Editor und Datei";
    s[(int)Str::CompareLeft] = "Links";
    s[(int)Str::CompareRight] = "Rechts";
    s[(int)Str::CompareChooseFile] = "Datei wählen...";
    s[(int)Str::CompareReadOnlyNote] = "Nur Ansicht; Änderungen im Editor werden berücksichtigt.";
    s[(int)Str::CompareChooseSources] = "Beide Quellen zum Vergleich auswählen.";
    s[(int)Str::CompareEditorUnavailable] = "Keine Firmware im Editor geöffnet.";
    s[(int)Str::CompareChanged] = "Geändert";
    s[(int)Str::CompareAdded] = "Hinzugefügt";
    s[(int)Str::CompareRemoved] = "Entfernt";
    s[(int)Str::CompareEqual] = "Gleich";
    s[(int)Str::CompareIncompatible] = "Inkompatibel";
    s[(int)Str::CompareDecodeError] = "Dekodierfehler";
    s[(int)Str::CompareStatusFilter] = "Status";
    s[(int)Str::CompareDifferencePreview] = "Differenz";
    s[(int)Str::CompareHighlightPixels] = "Unterschiede hervorheben";
    s[(int)Str::CompareDifferentPixels] = "%zu / %zu Pixel unterscheiden sich";
    s[(int)Str::CompareMissingResource] = "Ressource fehlt";
    s[(int)Str::CompareIncompatibleHint] = "Unterschiedliche Typen oder Größen: Pixelvergleich nicht verfügbar.";
    s[(int)Str::CompareImageUnavailable] = "Bild nicht verfügbar";
    s[(int)Str::CompareTextureError] = "Vorschautextur konnte nicht erstellt werden.";
    s[(int)Str::CompareAllStatuses] = "Alle Status";
    s[(int)Str::TabFirmwareEmulator] = "Emulator";
    s[(int)Str::TabFirmwareLayout] = "UI-Anordnung";
    s[(int)Str::LayoutUnsupported] = "Die UI-Anordnung kann für diese Firmware nicht bearbeitet werden.";
    s[(int)Str::LayoutNativeNote] = "Element ziehen oder X/Y eingeben. Die Vorschau führt die geänderte Firmware aus; Änderungen gelten für die ausgewählte Hardwarerevision.";
    s[(int)Str::LayoutRevision] = "Hardwarerevision";
    s[(int)Str::LayoutCheckingRevisions] = "Identische Oberflächen werden geprüft…";
    s[(int)Str::LayoutAllRevisions] = "Alle Revisionen";
    s[(int)Str::LayoutRevisionGroup] = "Identische Oberfläche: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Revision: %s";
    s[(int)Str::LayoutResetScope] = "Ausgewählte Revisionen zurücksetzen";
    s[(int)Str::LayoutSelectionUnavailable] = "Kein passendes Element in allen ausgewählten Revisionen.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI-Elemente";
    s[(int)Str::LayoutNoElements] = "Keine verschiebbaren Blöcke auf diesem Bildschirm.";
    s[(int)Str::LayoutSharedBlock] = "Dieser Block teilt seine Koordinaten mit anderen Bildschirmen.";
    s[(int)Str::LayoutPositionX] = "X (Pixel)";
    s[(int)Str::LayoutPositionY] = "Y (Pixel)";
    s[(int)Str::LayoutOffset] = "Versatz: %+d, %+d px";
    s[(int)Str::LayoutGridStep] = "Schritt: %d × %d px";
    s[(int)Str::LayoutGridSpacing] = "Rasterabstand (px)";
    s[(int)Str::LayoutGridConstraint] = "Ist kein Rasterpunkt darstellbar, haben native Koordinatenschritte und Grenzen Vorrang.";
    s[(int)Str::LayoutSnapGrid] = "Am Raster ausrichten";
    s[(int)Str::LayoutSnapElements] = "An Elementkanten und -mitten ausrichten";
    s[(int)Str::LayoutGuides] = "Bewegungshilfslinien";
    s[(int)Str::LayoutAlign] = "Am Bildschirm ausrichten";
    s[(int)Str::LayoutAlignLeft] = "Links";
    s[(int)Str::LayoutAlignCenter] = "Mitte X";
    s[(int)Str::LayoutAlignRight] = "Rechts";
    s[(int)Str::LayoutAlignTop] = "Oben";
    s[(int)Str::LayoutAlignMiddle] = "Mitte Y";
    s[(int)Str::LayoutAlignBottom] = "Unten";
    s[(int)Str::LayoutNudgeHint] = "Pfeile: nativer Pixelschritt; Shift: größerer Schritt. Bildschirm zum Fokussieren anklicken.";
    s[(int)Str::LayoutOverlays] = "Elementumrisse";
    s[(int)Str::LayoutOverlapOverlay] = "Überlappungen hervorheben";
    s[(int)Str::LayoutOverlaps] = "Überschneidende Elemente";
    s[(int)Str::LayoutNoOverlaps] = "Keine Rechtecküberschneidungen";
    s[(int)Str::LayoutDrawOrderUnknown] = "Native Zeichenreihenfolge nicht bestätigt";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) wird später als %s (#%d) gezeichnet";
    s[(int)Str::LayoutDrawOrderNote] = "Aufrufreihenfolge beim Haupt-Neuzeichnen, keine festen Ebenen. Bei Live-Aktualisierungen kann sie abweichen; nur tatsächlich geschriebene Pixel überdecken frühere Inhalte.";
    s[(int)Str::LayoutDrawSequence] = "Native Zeichenfolge";
    s[(int)Str::LayoutDrawInterleaved] = "Aufrufe sind verschachtelt; die Überlappung hängt vom Elementteil ab.";
    s[(int)Str::EmulatorBefore] = "Vorher (geöffnete Firmware)";
    s[(int)Str::EmulatorAfter] = "Nachher (aktuelle Änderungen)";
    s[(int)Str::EmulatorSideBySide] = "Nebeneinander";
    s[(int)Str::EmulatorOriginalError] = "Vorschau der geöffneten Firmware fehlgeschlagen: %s";
    s[(int)Str::MenuSaveProject] = "Projekt speichern";
    s[(int)Str::MenuSaveProjectAs] = "Projekt speichern unter...";
    s[(int)Str::MsgProjectSaved] = "Projekt gespeichert: %s";
    s[(int)Str::MenuBuild] = "Firmware exportieren";
    s[(int)Str::BtnOpenFirmware] = "Firmware oder Projekt öffnen...";
    s[(int)Str::StartupOpenFile] = "Firmware oder Projekt öffnen...";
    s[(int)Str::UnsavedPrompt] = "Projekt vor dem Fortfahren speichern?";
    s[(int)Str::UnsavedPrompt] = "Projekt vor dem Fortfahren speichern?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Element zurücksetzen";
    s[(int)Str::LayoutResetRevision] = "Anordnung dieser Revision zurücksetzen";
    s[(int)Str::LayoutResetAll] = "Alle Anordnungsänderungen zurücksetzen";
    s[(int)Str::LayoutPatchSites] = "Patch-Adressen";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Vorschauzustand";
    s[(int)Str::LayoutMoveElement] = "UI-Element verschieben";
    s[(int)Str::LayoutRba] = "RBA-Modus";
    s[(int)Str::LayoutTemperatureControl] = "Temperaturregelung";
    s[(int)Str::LayoutTemperature] = "Temperatur (°C)";
    s[(int)Str::LayoutRenderFailed] = "Nativer Bildpuffer nicht verfügbar.";
    s[(int)Str::LayoutElementPower] = "Leistung";
    s[(int)Str::LayoutElementBattery] = "Akku";
    s[(int)Str::LayoutElementResistance] = "Widerstand";
    s[(int)Str::LayoutElementDuration] = "Dauer";
    s[(int)Str::LayoutElementPuffs] = "Züge";
    s[(int)Str::LayoutElementMode] = "Modus";
    s[(int)Str::LayoutElementStatus] = "Status";
    s[(int)Str::LayoutElementScore] = "Punktzahl";
    s[(int)Str::LayoutElementStatistics] = "Statistik";
    s[(int)Str::LayoutElementVoltage] = "Spannung";
    s[(int)Str::LayoutElementGraph] = "Diagramm";
    s[(int)Str::LayoutElementRank] = "Rang";
    s[(int)Str::LayoutElementModeSelection] = "Modusauswahl";
    s[(int)Str::LayoutElementLogo] = "Logo";
    s[(int)Str::LayoutChangedElements] = "Geänderte UI-Elemente";
    s[(int)Str::EmulatorSupported] = "Native UI-Vorschau für alle mitgelieferten DRAG-Firmwares.";
    s[(int)Str::EmulatorNativeNote] = "Nativer Zeichencode mit aktuellen Änderungen. Keine elektrische Emulation.";
    s[(int)Str::EmulatorWatts] = "Leistung (W)";
    s[(int)Str::EmulatorPuffs] = "Züge";
    s[(int)Str::EmulatorBattery] = "Akku (0–5)";
    s[(int)Str::EmulatorResistance] = "Widerstand (mΩ)";
    s[(int)Str::EmulatorDuration] = "Dauer (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Gesperrt";
    s[(int)Str::EmulatorCharging] = "Laden";
    s[(int)Str::EmulatorScore] = "Gesamtpunkte";
    s[(int)Str::EmulatorScoreDisplay] = "Bestwert";
    s[(int)Str::EmulatorRedraw] = "Neu zeichnen";
    s[(int)Str::EmulatorLandscape] = "Querformat";
    s[(int)Str::EmulatorScreen] = "Bildschirm";
    s[(int)Str::EmulatorMainScreen] = "Hauptbildschirm";
    s[(int)Str::FontSystemFonts] = "Systemschriftarten";
    s[(int)Str::FontUserFonts] = "Benutzerschriftarten";
    s[(int)Str::EmulatorMenuItem] = "Menüpunkt";
    s[(int)Str::EmulatorMaterial] = "Material";
    s[(int)Str::EmulatorMenuConfirmed] = "Bestätigt";
    s[(int)Str::EmulatorResources] = "Von der Firmware gelesene Ressourcen";
    s[(int)Str::EmulatorNoResources] = "Keine Katalogressourcen gelesen.";
    s[(int)Str::EmulatorUidNote] = "Emulierte MCU-UID, keine ID eines angeschlossenen Geräts.";
    s[(int)Str::EmulatorLoaderVersion] = "Virtuelle Bootloader-Version";
    s[(int)Str::EmulatorLoaderId] = "Virtuelle CHIP-ID-Bytes";
    s[(int)Str::EmulatorLoaderNote] = "Bootloader-Daten werden emuliert, nicht aus Firmware oder Gerät gelesen.";
    s[(int)Str::TabFirmwarePatches] = "Patches";
    s[(int)Str::PatchBootLogo] = "Startbildschirm";
    s[(int)Str::PatchShutdownLogo] = "Ausschaltbildschirm";
    s[(int)Str::PatchDeviceInfo] = "CHIP-ID- / Versionsbildschirm";
    s[(int)Str::PatchDeviceInfoNote] = "Nur statisches Bild. Ersetzt den Servicebildschirm, nicht die Hauptanzeige für Leistung und Akku.";
    s[(int)Str::PatchLogoNote] = "Unabhängige Bilder mit 80 × 160 für alle DRAG S Score / Factory-Revisionen. Nicht am Gerät geprüft.";
    s[(int)Str::PatchUnsupported] = "Dieser Patch erfordert DRAG S Score- oder Factory-Firmware.";
    s[(int)Str::PatchEnabled] = "Eigenes Logo aktivieren";
    s[(int)Str::PatchCanvas] = "Komposition · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Native Bildschirmvorschau";
    s[(int)Str::PatchPreviewNote] = "Die native Vorschau zeigt angewendete Einstellungen, nicht den Kompositionsentwurf.";
    s[(int)Str::PatchFit] = "Einpassen";
    s[(int)Str::PatchCover] = "Ausfüllen";
    s[(int)Str::PatchStretch] = "Strecken";
    s[(int)Str::PatchScale] = "Skalierung";
    s[(int)Str::PatchAnimation] = "Einblendanimation";
    s[(int)Str::PatchNone] = "Keine";
    s[(int)Str::PatchFade] = "Überblenden";
    s[(int)Str::PatchDuration] = "Dauer (ms)";
    s[(int)Str::PatchAutoHardware] = "Automatisch";
    s[(int)Str::PatchAnimationPhase] = "Animationsphase";
    s[(int)Str::PatchPlay] = "Abspielen";
    s[(int)Str::EmulatorModeScreens] = "Bildschirme";
    s[(int)Str::EmulatorModeDevice] = "Gerät";
    s[(int)Str::DeviceUnsupported] = "Gerätemodus unterstützt DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Firmware startet ab Reset mit virtueller Peripherie. Heizung wird simuliert; elektrische Sicherheit wird nicht geprüft.";
    s[(int)Str::DeviceReset] = "Neustart";
    s[(int)Str::DevicePause] = "Pause";
    s[(int)Str::DeviceResume] = "Start";
    s[(int)Str::DeviceStep] = "Schritt";
    s[(int)Str::DeviceChanged] = "Aktuelle Änderungen sind noch nicht geladen.";
    s[(int)Str::DeviceApply] = "Mit Änderungen neu starten";
    s[(int)Str::DeviceButtons] = "Tasten oder Leertaste / ↑ / ↓ gedrückt halten.";
    s[(int)Str::DeviceAtomizerConnected] = "Verdampfer verbunden";
    s[(int)Str::DeviceTime] = "Virtuelle Zeit";
    s[(int)Str::DeviceSleeping] = "CPU schläft";
    s[(int)Str::DeviceLoading] = "Gerät wird geladen...";
    s[(int)Str::DeviceLog] = "Peripheriezugriffe";
    s[(int)Str::BadgeNoFirmware] = "| [Keine Firmware geöffnet — .zwdf/.xwdf hierher ziehen]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu Elemente | %zu Datensätze";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Firmware öffnen...";
    s[(int)Str::BtnExportAll] = "Alle Ressourcen exportieren...";
    s[(int)Str::BtnImportFolder] = "PNG-Ordner importieren...";
    s[(int)Str::BtnBuildFirmware] = " FIRMWARE ERSTELLEN ";
    s[(int)Str::BtnRevertAllStaged] = "Alle Änderungen zurücksetzen";
    s[(int)Str::GroupAll] = "Alle Gruppen";
    s[(int)Str::GroupAlerts] = "Warnungen (Dialoge & Symbole)";
    s[(int)Str::GroupFonts] = "Schriftarten (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Punkte & Ränge";
    s[(int)Str::GroupStatusLabels] = "Status & Beschriftungen";
    s[(int)Str::GroupControls] = "Steuerung & Batterie";
    s[(int)Str::GroupScreens] = "Bildschirme & Boot-Logos";
    s[(int)Str::GroupAnimations] = "Animationen";
    s[(int)Str::PanelResourcesHeader] = "Ressourcen";
    s[(int)Str::LabelSearch] = "Suche";
    s[(int)Str::SearchPlaceholder] = "nach Titel, ID, Adresse filtern...";
    s[(int)Str::NoFirmwareLoadedList] = "Keine Firmware geladen.\nDatei > Neu / Öffnen";
    s[(int)Str::NoResourcesMatchFilter] = "Keine Ressourcen entsprechen dem Filter.";
    s[(int)Str::PanelResourcePreviewHeader] = "Vorschau";
    s[(int)Str::SelectResourcePrompt] = "Wählen Sie eine Ressource aus der Liste links zur Vorschau und Bearbeitung.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Gruppe";
    s[(int)Str::LabelColor] = "Farbe";
    s[(int)Str::LabelColorCustom] = "Benutzerdefiniert";
    s[(int)Str::LabelFlashAddress] = "Flash-Adresse";
    s[(int)Str::LabelDimensions] = "Abmessungen";
    s[(int)Str::LabelBytes] = "Bytes";
    s[(int)Str::LabelRecords] = "Datensätze";
    s[(int)Str::StatusModifiedStaged] = "Status: GEÄNDERT (im Speicher)";
    s[(int)Str::StatusOriginal] = "Status: ORIGINAL (aus Firmware)";
    s[(int)Str::StatusStockOriginal] = "Original (entspricht Referenz)";
    s[(int)Str::StatusCustomDiffers] = "Benutzerdefiniert (weicht von Referenz ab)";
    s[(int)Str::BtnRestoreStock] = "Auf Werksreferenz zurücksetzen";
    s[(int)Str::LabelCompareWithStock] = "Mit Referenz vergleichen";
    s[(int)Str::LabelActivePreview] = "Aktuell (aktiv):";
    s[(int)Str::LabelStockReferencePreview] = "Werksreferenz:";
    s[(int)Str::FilterOnlyCustom] = "Nur abweichende Ressourcen";
    s[(int)Str::MsgRestoredToStock] = "%s auf Werksreferenz zurückgesetzt.";
    s[(int)Str::BtnReplaceFromPng] = "Aus PNG ersetzen...";
    s[(int)Str::BtnRestoreOriginal] = "Original wiederherstellen";
    s[(int)Str::BtnExportThisPng] = "PNG exportieren...";
    s[(int)Str::BtnExportHorizontal] = "Horizontal exportieren (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Farbpalette & Renderer (RGB565)";
    s[(int)Str::LabelTheme] = "Thema";
    s[(int)Str::BtnApply] = "Anwenden";
    s[(int)Str::BtnResetAll] = "Alle zurücksetzen";
    s[(int)Str::LabelRole] = "Rolle";
    s[(int)Str::BtnResetRole] = "Rolle zurücksetzen";
    s[(int)Str::PromptLoadFwColors] = "Firmware laden, um Farben anzuzeigen und zu ändern.";
    s[(int)Str::LabelPreviewTint] = "Vorschau-Farbton";
    s[(int)Str::InfoDigitsMonochrome] = "Ziffern sind monochrome Masken. Ihre Anzeigefarbe wird durch den Firmware-Code festgelegt, nicht im Glyph gespeichert.";
    s[(int)Str::InfoPreviewNotSaved] = "Die Vorschau-Farbe wird nicht in die Firmware gespeichert. Der RGB565 PNG-Export enthält diese Farbe.";
    s[(int)Str::LabelPreviewOnly] = "Nur Vorschau:";
    s[(int)Str::LabelPresets] = "Vorlagen...";
    s[(int)Str::BtnResetColor] = "Farbe zurücksetzen";
    s[(int)Str::LabelZoom] = "Vorschau-Zoom";
    s[(int)Str::LabelPixelGrid] = "Pixelraster";
    s[(int)Str::LabelHorizontalPreview] = "Horizontale Vorschau (136x22):";
    s[(int)Str::FontSec1Title] = "1. Schriftauswahl";
    s[(int)Str::FontSec1WindowsFont] = "Windows-Schrift";
    s[(int)Str::FontSec1BrowseTtf] = "TTF/OTF-Datei durchsuchen...";
    s[(int)Str::FontSec1ActiveFont] = "Aktive Schrift: %s";
    s[(int)Str::FontSec2Title] = "2. Anzeigevoreinstellung";
    s[(int)Str::FontSec3Title] = "3. Render-Parameter";
    s[(int)Str::FontSec3Threshold] = "Schwellwert";
    s[(int)Str::FontSec3ThresholdHint] = "Niedriger = fetter, Höher = dünner";
    s[(int)Str::FontSec3Stretch] = "Auf Zelle strecken";
    s[(int)Str::FontSec3Baseline] = "Grundlinienausrichtung";
    s[(int)Str::FontSec3CenterV] = "Vertikal zentrieren";
    s[(int)Str::FontSec3PaddingX] = "Abstand X";
    s[(int)Str::FontSec3PaddingY] = "Abstand Y";
    s[(int)Str::FontSec3Oversampling] = "Überabtastung";
    s[(int)Str::FontSec4Title] = "4. Firmware-Integration";
    s[(int)Str::FontSec4ApplyPreset] = " > GLYPHEN AUF GEÖFFNETE FIRMWARE ANWENDEN < ";
    s[(int)Str::FontSec4ApplyAll] = "Alle Schriften anwenden";
    s[(int)Str::FontSec5Title] = "5. Als PNGs exportieren";
    s[(int)Str::FontSec5OutputFolder] = "Ausgabeordner";
    s[(int)Str::FontSec5BrowseFolder] = "Ordner durchsuchen...";
    s[(int)Str::FontSec5OpenExplorer] = "Im Explorer öffnen";
    s[(int)Str::FontSec5ExportAllPreset] = "Alle Voreinstellungen exportieren";
    s[(int)Str::FontSec5ExportButton] = "1-Bit PNGs auf Festplatte exportieren";
    s[(int)Str::FontRightLivePreview] = "Live-Schriftvorschau";
    s[(int)Str::FontRightNoGlyphs] = "Keine Glyphen geladen.";
    s[(int)Str::FontPresetInfo] = "Voreinstellung: %s (%dx%d px, %d Glyphen, Größe: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Paketspezifikation";
    s[(int)Str::FwInfoNoFirmware] = "Keine Firmware geladen. Öffnen Sie eine .zwdf/.xwdf-Datei im Tab Ressourcen-Studio.";
    s[(int)Str::FwInfoFilePath] = "Dateipfad: %s";
    s[(int)Str::FwInfoSha256] = "Quell-SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Paketgröße: %zu Bytes";
    s[(int)Str::FwInfoHwRevisions] = "Hardware-Revisionen: %zu Datensätze";
    s[(int)Str::FwInfoRecognizedResources] = "Erkannte Firmware-Ressourcen: %zu Elemente (Schriftarten, Symbole, Ränge, Beschriftungen, Steuerungen, Bildschirme)";
    s[(int)Str::FwInfoRecordsTable] = "Hardware-Datensatztabelle";
    s[(int)Str::FwInfoColRecord] = "Datensatz #";
    s[(int)Str::FwInfoColDevCode] = "Gerätecode";
    s[(int)Str::FwInfoColPayload] = "Nutzdatengröße";
    s[(int)Str::FwInfoColImage] = "Image-Größe";
    s[(int)Str::FwInfoColFlashBase] = "Flash-Basisadresse";
    s[(int)Str::FwInfoColMSP] = "Stapelzeiger (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Reset-Vektor";
    s[(int)Str::BtnRevertAllModifications] = "Alle Änderungen rückgängig";
    s[(int)Str::MsgReadyOpenFw] = "Bereit. Öffnen Sie eine .zwdf/.xwdf-Firmware-Datei oder generieren Sie Schriften.";
    s[(int)Str::MsgSuccessOpened] = "Geöffnet: %s";
    s[(int)Str::MsgOpenError] = "Öffnungsfehler: %s";
    s[(int)Str::MsgAllReverted] = "Alle Änderungen auf den Originalzustand der Firmware zurückgesetzt.";
    s[(int)Str::MsgExportSuccess] = "%d Ressourcen exportiert nach: %s";
    s[(int)Str::MsgExportError] = "Exportfehler: %s";
    s[(int)Str::MsgImportSuccess] = "%d PNGs aus Ordner importiert.";
    s[(int)Str::MsgImportError] = "Importfehler: %s";
    s[(int)Str::MsgBuildSuccess] = "Build ERFOLGREICH: %s";
    s[(int)Str::MsgBuildError] = "Build-Fehler: %s";
    s[(int)Str::MsgFontNotLoaded] = "Fehler: Keine Schrift im Generator geladen!";
    s[(int)Str::MsgFontInjectedPreset] = "%d Glyphen von %s direkt in den Firmware-Speicher eingefügt!";
    s[(int)Str::MsgFontInjectedAll] = "Angewendete Glyphen: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d PNG-Dateien erfolgreich exportiert nach: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d Dateien für %s erfolgreich exportiert nach: %s";
    s[(int)Str::MsgFontExportError] = "Exportfehler: %s";
    s[(int)Str::ModalBuildReportTitle] = "Build-Bericht";
    s[(int)Str::ModalBuildSuccessBanner] = "FIRMWARE-BUILD & VERIFIZIERUNG ERFOLGREICH!";
    s[(int)Str::ModalSavedFile] = "Gespeicherte Datei: %s";
    s[(int)Str::ModalOutputSize] = "Ausgabegröße: %zu Bytes";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Geänderte Bitmap-Ressourcen: %zu";
    s[(int)Str::ModalModColorGroups] = "Geänderte Farbgruppen: %zu";
    s[(int)Str::ModalPatchedRanges] = "Gepatchte Bytebereiche: %zu über %zu Hardware-Datensätze";
    s[(int)Str::ModalUntouchedBytes] = "Unveränderte Bytes: %zu Bytes identisch mit Original";
    s[(int)Str::ModalReportNote] = "Ein detaillierter Bericht wurde als .report.json neben der Firmware gespeichert.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Im Explorer öffnen";
    s[(int)Str::BtnCopyClipboard] = "Kopieren (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Einfügen (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Rückgängig";
    s[(int)Str::BtnRedo] = "Wiederholen";
    s[(int)Str::TooltipUndo] = "Rückgängig: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Wiederholen: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Nichts zum Rückgängigmachen (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Nichts zum Wiederholen (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Pixel-Werkzeuge:";
    s[(int)Str::ToolPencil] = "Stift";
    s[(int)Str::ToolEraser] = "Radierer";
    s[(int)Str::BtnInvertPixels] = "Invertieren";
    s[(int)Str::BtnClearPixels] = "Löschen";
    s[(int)Str::BtnFlipH] = "Horiz. spiegeln";
    s[(int)Str::BtnFlipV] = "Vert. spiegeln";
    s[(int)Str::MsgCopiedToClipboard] = "Bild von %s in Zwischenablage kopiert.";
    s[(int)Str::MsgPastedFromClipboard] = "Bild aus Zwischenablage für %s eingefügt!";
    s[(int)Str::MsgClipboardNoImage] = "Kein kompatibles Bild in der Zwischenablage.";
    s[(int)Str::MsgUndoPerformed] = "Rückgängig gemacht: %s";
    s[(int)Str::MsgRedoPerformed] = "Wiederholt: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Firmware: %s (SHA256: %s... | %zu Datensätze | %zu geändert)";
    s[(int)Str::BadgeProjectSummary] = "| Projekt: %s (SHA256: %s... | %zu Datensätze | %zu geändert)";
    s[(int)Str::LabelCategory] = "Kategorie";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Gruppe: %s  |  Farbe: Benutzerdefiniert (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Gruppe: %s  |  Farbe: %s";
    s[(int)Str::LabelMetaHardware] = "Flash-Adresse: 0x%08X  |  Abmessungen: %u x %u px  |  Bytes: %zu  |  Datensätze: %zu";
    s[(int)Str::LabelPixelCoord] = "Pixel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| LMT: %s  |  RMT: Löschen";
    s[(int)Str::LabelDrawAction] = "Zeichnen";
    s[(int)Str::LabelEraseAction] = "Löschen";
    s[(int)Str::LabelFontPreset] = "Voreinstellung";
    s[(int)Str::LabelFontPixelGrid] = "Pixelraster";
    s[(int)Str::MsgAppliedColorTheme] = "Farbdesign angewendet: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "%zu passende PNGs aus Ordner importiert.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Firmware über Drag & Drop geöffnet: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop-Fehler: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Schriftart über Drag & Drop geladen: %s";
    s[(int)Str::MsgFailedLoadFont] = "Fehler beim Laden der Schriftartdatei: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Verworfenes PNG stimmt mit Original für %s überein.";
    s[(int)Str::MsgStagedReplacement] = "Benutzerdefinierter Ersatz für %s aus PNG vorbereitet!";
    s[(int)Str::MsgPngLoadError] = "PNG-Ladefehler: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Ersatz für %s stimmt mit dem Original überein.";
    s[(int)Str::MsgStagedReplacementCustom] = "Ersatz für %s vorbereitet!";
    s[(int)Str::MsgStagedRgbReplacement] = "RGB565-Ersatz für %s vorbereitet!";
    s[(int)Str::MsgRestoredOriginal] = "%s auf Firmware-Original zurückgesetzt.";
    s[(int)Str::MsgSavedAssetPng] = "%s gespeichert unter: %s";
    s[(int)Str::MsgSaveError] = "Speicherfehler: %s";
    s[(int)Str::MsgSavedBootLogo] = "Horizontales Boot-Logo gespeichert unter: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Fehler: Quelldatei darf nicht überschrieben werden! Wählen Sie einen neuen Namen.";
    s[(int)Str::MsgColorPatchError] = "Farb-Patch-Fehler: %s";
    s[(int)Str::MsgPackError] = "Paketierungsfehler: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu geänderte PNGs importiert (%zu stimmten mit Original überein)";

    s[(int)Str::LabelBuiltin] = "integriert";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Integrierte Firmware konnte nicht geladen werden: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Integrierte Firmware geladen: %s";
    s[(int)Str::ThemeFactoryDefault] = "Werkseinstellung";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Originaler Voopoo Rot & Gelb Werksstil";
    s[(int)Str::ThemeCyberpunkNeon] = "Cyberpunk Neon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Elektrisches Cyan, Neonpink und lebendiges Violett";
    s[(int)Str::ThemeMatrixGreen] = "Matrix Grün";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT-Terminal-Phosphor & Smaragdgrün";
    s[(int)Str::ThemeArcticIceBlue] = "Arktisches Eisblau";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Gletscher-Cyan, Himmelblau und Azurleuchten";
    s[(int)Str::ThemeAmberFire] = "Bernsteinfeuer";
    s[(int)Str::ThemeAmberFireDesc] = "Lavaorange, goldene Flamme und Glut";
    s[(int)Str::ThemePurpleNebula] = "Lila Nebel";
    s[(int)Str::ThemePurpleNebulaDesc] = "Tiefes Violett, Lavendel und Neon-Amethyst";
    s[(int)Str::ThemeStealthDarkGray] = "Stealth Dunkelgrau";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Mattes Graphit, Schiefer und Gunmetal";
    s[(int)Str::RoleAccentRed] = "Akzent / roter Modus";
    s[(int)Str::RoleAccentRedScope] = "Haupt-UI-Akzent in Standardmodi — 8 Stellen";
    s[(int)Str::RoleAccentCyan] = "Akzent / Cyan-Modus";
    s[(int)Str::RoleAccentCyanScope] = "Haupt-UI-Akzent in SMART/RBA-Zuständen — 7 Stellen";
    s[(int)Str::RoleHighlightYellow] = "Hervorhebung / gelb";
    s[(int)Str::RoleHighlightYellowScope] = "Punkte-Leiste und Service-Panels — 4 Stellen";
    s[(int)Str::RoleRankBronze] = "Rang / Bronze";
    s[(int)Str::RoleRankBronzeScope] = "Bronze-Rang-Palette";
    s[(int)Str::RoleRankGold] = "Rang / Gold";
    s[(int)Str::RoleRankGoldScope] = "Gold-Rang-Palette";
    s[(int)Str::RoleRankIce] = "Rang / Platin (Eis)";
    s[(int)Str::RoleRankIceScope] = "Platin / Eis-Rang-Palette";
    s[(int)Str::RoleRankCyan] = "Rang / Cyan";
    s[(int)Str::RoleRankCyanScope] = "Cyan-Rang-Palette";
    s[(int)Str::RoleRankOrange] = "Rang / Orange";
    s[(int)Str::RoleRankOrangeScope] = "Orange-Rang-Palette";
    s[(int)Str::RoleRankRed] = "Rang / Rot";
    s[(int)Str::RoleRankRedScope] = "Rote Rang-Palette";
    s[(int)Str::RoleDigitsWattage] = "Watt-Ziffern";
    s[(int)Str::RoleDigitsWattageScope] = "Große Wattanzeige auf dem Hauptbildschirm (19x40)";
    s[(int)Str::RoleDigitsStats] = "Statistik-Ziffern";
    s[(int)Str::ColorPresetWhite] = "Weiß (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Haupt / Rot (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Cyan (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Service / Gelb (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bronze (#B57947)";
    s[(int)Str::ColorPresetGold] = "Gold (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Orange (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Grün (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Hauptziffern 0-9, 10 Dateien)";
    s[(int)Str::FontPreset11x20] = "11x20 (Sekundärziffern 0-9, 10 Dateien)";
    s[(int)Str::FontPreset7x13] = "7x13 (Kleiner Status 0-9, A-F, 16 Dateien)";
    s[(int)Str::FontPreset6x10] = "6x10 (Mikro-Status 0-9, 10 Dateien)";
    s[(int)Str::FontOs1x] = "1x (Direkt)";
    s[(int)Str::FontOs2x] = "2x (Schnell)";
    s[(int)Str::FontOs4x] = "4x (Empfohlen)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

static void InitSpanish() {
    auto& s = g_table[(int)Language::Spanish];
    s[(int)Str::SupportFull] = "Completo";
    s[(int)Str::SupportNone] = "No compatible";
    s[(int)Str::SupportPalette] = "Paleta de colores";
    s[(int)Str::SupportMonochrome] = "Pantalla monocroma; sin paleta de colores.";
    s[(int)Str::EmulatorHistoryDay] = "Día del historial";
    s[(int)Str::EmulatorClockHour] = "Horas";
    s[(int)Str::EmulatorClockMinute] = "Minutos";
    s[(int)Str::EmulatorClockField] = "Campo seleccionado";
    s[(int)Str::MenuFile] = "Archivo";
    s[(int)Str::MenuEdit] = "Edición";
    s[(int)Str::MenuLanguage] = "Idioma";
    s[(int)Str::MenuHelp] = "Ayuda";
    s[(int)Str::UpdateTitle] = "Actualizaciones del estudio";
    s[(int)Str::UpdateVersion] = "Versión instalada: %s";
    s[(int)Str::UpdateCheck] = "Buscar actualizaciones";
    s[(int)Str::UpdateChecking] = "Consultando GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Tienes la última versión.";
    s[(int)Str::UpdateDownload] = "Descargar";
    s[(int)Str::UpdateDownloading] = "Descargando y verificando...";
    s[(int)Str::UpdateInstall] = "Instalar y reiniciar";
    s[(int)Str::UpdateReady] = "Listo para instalar.";
    s[(int)Str::UpdateChangelog] = "Cambios";
    s[(int)Str::UpdateFailed] = "Error de actualización";
    s[(int)Str::UpdateAvailable] = "Versión disponible: %s";
    s[(int)Str::MenuCreate] = "Nuevo...";
    s[(int)Str::MenuOpen] = "Abrir...";
    s[(int)Str::MenuImport] = "Importar recursos...";
    s[(int)Str::MenuExport] = "Exportar recursos...";
    s[(int)Str::MenuBuild] = "Generar firmware...";
    s[(int)Str::MenuExit] = "Salir";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Recursos";
    s[(int)Str::TabFontGenerator] = "Generador de fuentes";
    s[(int)Str::TabFirmwareInfo] = "Acerca del firmware";
    s[(int)Str::TabFirmwareComparison] = "Comparación";
    s[(int)Str::CompareModeTwoFiles] = "Dos archivos";
    s[(int)Str::CompareModeEditor] = "Editor y archivo";
    s[(int)Str::CompareLeft] = "Izquierda";
    s[(int)Str::CompareRight] = "Derecha";
    s[(int)Str::CompareChooseFile] = "Elegir archivo...";
    s[(int)Str::CompareReadOnlyNote] = "Solo lectura; incluye los cambios del editor.";
    s[(int)Str::CompareChooseSources] = "Elige ambas fuentes para comparar.";
    s[(int)Str::CompareEditorUnavailable] = "No hay firmware abierto en el editor.";
    s[(int)Str::CompareChanged] = "Modificado";
    s[(int)Str::CompareAdded] = "Añadido";
    s[(int)Str::CompareRemoved] = "Eliminado";
    s[(int)Str::CompareEqual] = "Igual";
    s[(int)Str::CompareIncompatible] = "Incompatible";
    s[(int)Str::CompareDecodeError] = "Error de decodificación";
    s[(int)Str::CompareStatusFilter] = "Estado";
    s[(int)Str::CompareDifferencePreview] = "Diferencia";
    s[(int)Str::CompareHighlightPixels] = "Resaltar diferencias";
    s[(int)Str::CompareDifferentPixels] = "Difieren %zu / %zu píxeles";
    s[(int)Str::CompareMissingResource] = "Recurso ausente";
    s[(int)Str::CompareIncompatibleHint] = "Tipos o tamaños diferentes: comparación de píxeles no disponible.";
    s[(int)Str::CompareImageUnavailable] = "Imagen no disponible";
    s[(int)Str::CompareTextureError] = "No se pudo crear la textura de vista previa.";
    s[(int)Str::CompareAllStatuses] = "Todos los estados";
    s[(int)Str::TabFirmwareEmulator] = "Emulador";
    s[(int)Str::TabFirmwareLayout] = "Distribución de IU";
    s[(int)Str::LayoutUnsupported] = "La edición de la distribución de IU no está disponible para este firmware.";
    s[(int)Str::LayoutNativeNote] = "Arrastra un elemento o introduce X/Y. La vista previa ejecuta el firmware modificado; los cambios se aplican a la revisión de hardware seleccionada.";
    s[(int)Str::LayoutRevision] = "Revisión de hardware";
    s[(int)Str::LayoutCheckingRevisions] = "Comprobando interfaces idénticas…";
    s[(int)Str::LayoutAllRevisions] = "Todas las revisiones";
    s[(int)Str::LayoutRevisionGroup] = "IU idéntica: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Revisión: %s";
    s[(int)Str::LayoutResetScope] = "Restablecer revisiones seleccionadas";
    s[(int)Str::LayoutSelectionUnavailable] = "No hay un elemento compatible en todas las revisiones seleccionadas.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Elementos de IU";
    s[(int)Str::LayoutNoElements] = "Esta pantalla no tiene bloques desplazables.";
    s[(int)Str::LayoutSharedBlock] = "Este bloque comparte coordenadas con otras pantallas.";
    s[(int)Str::LayoutPositionX] = "X (píxeles)";
    s[(int)Str::LayoutPositionY] = "Y (píxeles)";
    s[(int)Str::LayoutOffset] = "Desplazamiento: %+d, %+d px";
    s[(int)Str::LayoutGridStep] = "Paso: %d × %d px";
    s[(int)Str::LayoutGridSpacing] = "Espaciado de cuadrícula (px)";
    s[(int)Str::LayoutGridConstraint] = "Si no hay ningún punto de cuadrícula representable, tienen prioridad los pasos y límites nativos de coordenadas.";
    s[(int)Str::LayoutSnapGrid] = "Ajustar a la cuadrícula";
    s[(int)Str::LayoutSnapElements] = "Ajustar a bordes y centros de elementos";
    s[(int)Str::LayoutGuides] = "Guías de movimiento";
    s[(int)Str::LayoutAlign] = "Alinear con la pantalla";
    s[(int)Str::LayoutAlignLeft] = "Izquierda";
    s[(int)Str::LayoutAlignCenter] = "Centro X";
    s[(int)Str::LayoutAlignRight] = "Derecha";
    s[(int)Str::LayoutAlignTop] = "Arriba";
    s[(int)Str::LayoutAlignMiddle] = "Centro Y";
    s[(int)Str::LayoutAlignBottom] = "Abajo";
    s[(int)Str::LayoutNudgeHint] = "Flechas: paso de píxel nativo; Shift: paso mayor. Haz clic en la pantalla para enfocarla.";
    s[(int)Str::LayoutOverlays] = "Contornos de elementos";
    s[(int)Str::LayoutOverlapOverlay] = "Resaltar solapamientos";
    s[(int)Str::LayoutOverlaps] = "Elementos que se intersectan";
    s[(int)Str::LayoutNoOverlaps] = "Sin intersecciones de rectángulos";
    s[(int)Str::LayoutDrawOrderUnknown] = "Orden de dibujo nativo sin verificar";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) se dibuja después de %s (#%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Orden de llamadas del redibujado principal, no capas permanentes. Las actualizaciones pueden diferir; solo los píxeles realmente escritos cubren el contenido anterior.";
    s[(int)Str::LayoutDrawSequence] = "Secuencia de dibujo nativa";
    s[(int)Str::LayoutDrawInterleaved] = "Las llamadas se intercalan; el solapamiento depende de la parte de cada elemento.";
    s[(int)Str::EmulatorBefore] = "Antes (firmware abierto)";
    s[(int)Str::EmulatorAfter] = "Después (cambios actuales)";
    s[(int)Str::EmulatorSideBySide] = "En paralelo";
    s[(int)Str::EmulatorOriginalError] = "Error de vista previa del firmware abierto: %s";
    s[(int)Str::MenuSaveProject] = "Guardar proyecto";
    s[(int)Str::MenuSaveProjectAs] = "Guardar proyecto como...";
    s[(int)Str::MsgProjectSaved] = "Proyecto guardado: %s";
    s[(int)Str::MenuBuild] = "Exportar firmware";
    s[(int)Str::BtnOpenFirmware] = "Abrir firmware o proyecto...";
    s[(int)Str::StartupOpenFile] = "Abrir firmware o proyecto...";
    s[(int)Str::UnsavedPrompt] = "¿Guardar el proyecto antes de continuar?";
    s[(int)Str::UnsavedPrompt] = "¿Guardar el proyecto antes de continuar?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Restablecer elemento";
    s[(int)Str::LayoutResetRevision] = "Restablecer distribución de esta revisión";
    s[(int)Str::LayoutResetAll] = "Restablecer todos los cambios de distribución";
    s[(int)Str::LayoutPatchSites] = "Direcciones de parches";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Estado de vista previa";
    s[(int)Str::LayoutMoveElement] = "Mover elemento de IU";
    s[(int)Str::LayoutRba] = "Modo RBA";
    s[(int)Str::LayoutTemperatureControl] = "Control de temperatura";
    s[(int)Str::LayoutTemperature] = "Temperatura (°C)";
    s[(int)Str::LayoutRenderFailed] = "El búfer de imagen nativo no está disponible.";
    s[(int)Str::LayoutElementPower] = "Potencia";
    s[(int)Str::LayoutElementBattery] = "Batería";
    s[(int)Str::LayoutElementResistance] = "Resistencia";
    s[(int)Str::LayoutElementDuration] = "Duración";
    s[(int)Str::LayoutElementPuffs] = "Caladas";
    s[(int)Str::LayoutElementMode] = "Modo";
    s[(int)Str::LayoutElementStatus] = "Estado";
    s[(int)Str::LayoutElementScore] = "Puntuación";
    s[(int)Str::LayoutElementStatistics] = "Estadísticas";
    s[(int)Str::LayoutElementVoltage] = "Voltaje";
    s[(int)Str::LayoutElementGraph] = "Gráfico";
    s[(int)Str::LayoutElementRank] = "Rango";
    s[(int)Str::LayoutElementModeSelection] = "Selección de modo";
    s[(int)Str::LayoutElementLogo] = "Logotipo";
    s[(int)Str::LayoutChangedElements] = "Elementos de IU modificados";
    s[(int)Str::EmulatorSupported] = "Vista nativa de la interfaz de todos los firmwares DRAG incluidos.";
    s[(int)Str::EmulatorNativeNote] = "Renderizado nativo con cambios actuales. Sin emulación eléctrica.";
    s[(int)Str::EmulatorWatts] = "Potencia (W)";
    s[(int)Str::EmulatorPuffs] = "Caladas";
    s[(int)Str::EmulatorBattery] = "Batería (0–5)";
    s[(int)Str::EmulatorResistance] = "Resistencia (mΩ)";
    s[(int)Str::EmulatorDuration] = "Duración (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Bloqueado";
    s[(int)Str::EmulatorCharging] = "Cargando";
    s[(int)Str::EmulatorScore] = "Puntos totales";
    s[(int)Str::EmulatorScoreDisplay] = "Mejor puntuación";
    s[(int)Str::EmulatorRedraw] = "Redibujar";
    s[(int)Str::EmulatorLandscape] = "Horizontal";
    s[(int)Str::EmulatorScreen] = "Pantalla";
    s[(int)Str::EmulatorMainScreen] = "Pantalla principal";
    s[(int)Str::FontSystemFonts] = "Fuentes del sistema";
    s[(int)Str::FontUserFonts] = "Fuentes del usuario";
    s[(int)Str::EmulatorMenuItem] = "Elemento del menú";
    s[(int)Str::EmulatorMaterial] = "Material";
    s[(int)Str::EmulatorMenuConfirmed] = "Confirmado";
    s[(int)Str::EmulatorResources] = "Recursos leídos por el firmware";
    s[(int)Str::EmulatorNoResources] = "No se registraron lecturas del catálogo.";
    s[(int)Str::EmulatorUidNote] = "UID del MCU emulado, no es el ID de un dispositivo conectado.";
    s[(int)Str::EmulatorLoaderVersion] = "Versión virtual del cargador";
    s[(int)Str::EmulatorLoaderId] = "Bytes virtuales de CHIP-ID";
    s[(int)Str::EmulatorLoaderNote] = "Los datos del cargador son emulados, no se leen del firmware ni del dispositivo.";
    s[(int)Str::TabFirmwarePatches] = "Parches";
    s[(int)Str::PatchBootLogo] = "Pantalla de inicio";
    s[(int)Str::PatchShutdownLogo] = "Pantalla de apagado";
    s[(int)Str::PatchDeviceInfo] = "Pantalla CHIP ID / versión";
    s[(int)Str::PatchDeviceInfoNote] = "Solo imagen estática. Sustituye la pantalla de servicio, nunca la pantalla principal de potencia y batería.";
    s[(int)Str::PatchLogoNote] = "Imágenes independientes de 80 × 160 para todas las revisiones DRAG S Score / Factory. No probado en un dispositivo.";
    s[(int)Str::PatchUnsupported] = "Este parche requiere firmware DRAG S Score o Factory.";
    s[(int)Str::PatchEnabled] = "Activar logotipo propio";
    s[(int)Str::PatchCanvas] = "Composición · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Vista previa nativa de pantalla";
    s[(int)Str::PatchPreviewNote] = "La vista nativa muestra los ajustes aplicados, no el borrador de la composición.";
    s[(int)Str::PatchFit] = "Ajustar";
    s[(int)Str::PatchCover] = "Cubrir";
    s[(int)Str::PatchStretch] = "Estirar";
    s[(int)Str::PatchScale] = "Escala";
    s[(int)Str::PatchAnimation] = "Animación de entrada";
    s[(int)Str::PatchNone] = "Ninguna";
    s[(int)Str::PatchFade] = "Fundido";
    s[(int)Str::PatchDuration] = "Duración (ms)";
    s[(int)Str::PatchAutoHardware] = "Automático";
    s[(int)Str::PatchAnimationPhase] = "Fase de animación";
    s[(int)Str::PatchPlay] = "Reproducir";
    s[(int)Str::EmulatorModeScreens] = "Pantallas";
    s[(int)Str::EmulatorModeDevice] = "Dispositivo";
    s[(int)Str::DeviceUnsupported] = "El modo dispositivo admite DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Firmware desde Reset con periféricos virtuales. El calentamiento se simula; no se verifica la seguridad eléctrica.";
    s[(int)Str::DeviceReset] = "Reiniciar";
    s[(int)Str::DevicePause] = "Pausa";
    s[(int)Str::DeviceResume] = "Ejecutar";
    s[(int)Str::DeviceStep] = "Paso";
    s[(int)Str::DeviceChanged] = "Los cambios actuales aún no están cargados.";
    s[(int)Str::DeviceApply] = "Reiniciar con cambios";
    s[(int)Str::DeviceButtons] = "Mantén los botones o espacio / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Resistencia conectada";
    s[(int)Str::DeviceTime] = "Tiempo virtual";
    s[(int)Str::DeviceSleeping] = "CPU dormida";
    s[(int)Str::DeviceLoading] = "Cargando dispositivo...";
    s[(int)Str::DeviceLog] = "Acceso a periféricos";
    s[(int)Str::BadgeNoFirmware] = "| [Sin firmware abierto — arrastra un .zwdf/.xwdf aquí]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu elementos | %zu registros";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Abrir firmware...";
    s[(int)Str::BtnExportAll] = "Exportar todos los recursos...";
    s[(int)Str::BtnImportFolder] = "Importar carpeta de PNGs...";
    s[(int)Str::BtnBuildFirmware] = " CREAR FIRMWARE ";
    s[(int)Str::BtnRevertAllStaged] = "Descartar todos los cambios";
    s[(int)Str::GroupAll] = "Todos los grupos";
    s[(int)Str::GroupAlerts] = "Alertas (diálogos e iconos)";
    s[(int)Str::GroupFonts] = "Fuentes (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Puntuación y rangos";
    s[(int)Str::GroupStatusLabels] = "Estado y etiquetas";
    s[(int)Str::GroupControls] = "Controles y batería";
    s[(int)Str::GroupScreens] = "Pantallas y logos de arranque";
    s[(int)Str::GroupAnimations] = "Animaciones";
    s[(int)Str::PanelResourcesHeader] = "Recursos";
    s[(int)Str::LabelSearch] = "Buscar";
    s[(int)Str::SearchPlaceholder] = "filtrar por título, id, dirección...";
    s[(int)Str::NoFirmwareLoadedList] = "No hay firmware cargado.\nArchivo > Nuevo / Abrir";
    s[(int)Str::NoResourcesMatchFilter] = "No hay recursos que coincidan con el filtro.";
    s[(int)Str::PanelResourcePreviewHeader] = "Vista previa";
    s[(int)Str::SelectResourcePrompt] = "Selecciona un recurso de la lista de la izquierda para previsualizar y editar.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Grupo";
    s[(int)Str::LabelColor] = "Color";
    s[(int)Str::LabelColorCustom] = "Personalizado";
    s[(int)Str::LabelFlashAddress] = "Dirección Flash";
    s[(int)Str::LabelDimensions] = "Dimensiones";
    s[(int)Str::LabelBytes] = "Bytes";
    s[(int)Str::LabelRecords] = "Registros";
    s[(int)Str::StatusModifiedStaged] = "Estado: MODIFICADO (en memoria)";
    s[(int)Str::StatusOriginal] = "Estado: ORIGINAL (del firmware)";
    s[(int)Str::StatusStockOriginal] = "Original (coincide con referencia)";
    s[(int)Str::StatusCustomDiffers] = "Personalizado (difiere de la referencia)";
    s[(int)Str::BtnRestoreStock] = "Restaurar de fábrica";
    s[(int)Str::LabelCompareWithStock] = "Comparar con referencia";
    s[(int)Str::LabelActivePreview] = "Actual (activo):";
    s[(int)Str::LabelStockReferencePreview] = "Referencia de fábrica:";
    s[(int)Str::FilterOnlyCustom] = "Solo diferentes de la referencia";
    s[(int)Str::MsgRestoredToStock] = "Restaurado %s a la referencia de fábrica.";
    s[(int)Str::BtnReplaceFromPng] = "Reemplazar desde PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Restaurar original";
    s[(int)Str::BtnExportThisPng] = "Exportar PNG...";
    s[(int)Str::BtnExportHorizontal] = "Exportar horizontal (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Paleta de colores y renderizado (RGB565)";
    s[(int)Str::LabelTheme] = "Tema";
    s[(int)Str::BtnApply] = "Aplicar";
    s[(int)Str::BtnResetAll] = "Restablecer todo";
    s[(int)Str::LabelRole] = "Rol";
    s[(int)Str::BtnResetRole] = "Restablecer rol";
    s[(int)Str::PromptLoadFwColors] = "Carga firmware para ver y modificar colores.";
    s[(int)Str::LabelPreviewTint] = "Tinte de vista previa";
    s[(int)Str::InfoDigitsMonochrome] = "Los dígitos son máscaras monocromas. Su color de visualización lo establece el código del firmware, no se almacena en el glifo.";
    s[(int)Str::InfoPreviewNotSaved] = "El tinte de vista previa no se guarda en el firmware. La exportación RGB565 PNG incluye este tinte.";
    s[(int)Str::LabelPreviewOnly] = "Solo vista previa:";
    s[(int)Str::LabelPresets] = "Presets...";
    s[(int)Str::BtnResetColor] = "Restablecer color";
    s[(int)Str::LabelZoom] = "Zoom de vista previa";
    s[(int)Str::LabelPixelGrid] = "Cuadrícula de píxeles";
    s[(int)Str::LabelHorizontalPreview] = "Vista previa horizontal (136x22):";
    s[(int)Str::FontSec1Title] = "1. Selección de fuente";
    s[(int)Str::FontSec1WindowsFont] = "Fuente de Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Buscar archivo TTF/OTF...";
    s[(int)Str::FontSec1ActiveFont] = "Fuente activa: %s";
    s[(int)Str::FontSec2Title] = "2. Preset de visualización";
    s[(int)Str::FontSec3Title] = "3. Parámetros de renderizado";
    s[(int)Str::FontSec3Threshold] = "Umbral";
    s[(int)Str::FontSec3ThresholdHint] = "Menor = más grueso, Mayor = más delgado";
    s[(int)Str::FontSec3Stretch] = "Estirar a celda";
    s[(int)Str::FontSec3Baseline] = "Alineación de línea base";
    s[(int)Str::FontSec3CenterV] = "Centrar verticalmente";
    s[(int)Str::FontSec3PaddingX] = "Relleno X";
    s[(int)Str::FontSec3PaddingY] = "Relleno Y";
    s[(int)Str::FontSec3Oversampling] = "Sobremuestreo";
    s[(int)Str::FontSec4Title] = "4. Integración en firmware";
    s[(int)Str::FontSec4ApplyPreset] = " > APLICAR GLIFOS AL FIRMWARE ABIERTO < ";
    s[(int)Str::FontSec4ApplyAll] = "Aplicar todas las fuentes";
    s[(int)Str::FontSec5Title] = "5. Exportar a PNGs";
    s[(int)Str::FontSec5OutputFolder] = "Carpeta de salida";
    s[(int)Str::FontSec5BrowseFolder] = "Buscar carpeta...";
    s[(int)Str::FontSec5OpenExplorer] = "Abrir en Explorador";
    s[(int)Str::FontSec5ExportAllPreset] = "Exportar todos los presets";
    s[(int)Str::FontSec5ExportButton] = "Exportar PNGs 1-Bit al disco";
    s[(int)Str::FontRightLivePreview] = "Vista previa en vivo";
    s[(int)Str::FontRightNoGlyphs] = "No se cargaron glifos.";
    s[(int)Str::FontPresetInfo] = "Preset: %s (%dx%d px, %d glifos, tamaño: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Especificación del paquete";
    s[(int)Str::FwInfoNoFirmware] = "No hay firmware cargado. Abre un archivo .zwdf/.xwdf en la pestaña Estudio de recursos.";
    s[(int)Str::FwInfoFilePath] = "Ruta del archivo: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 fuente: %s";
    s[(int)Str::FwInfoPackageSize] = "Tamaño del paquete: %zu bytes";
    s[(int)Str::FwInfoHwRevisions] = "Revisiones de hardware: %zu registros";
    s[(int)Str::FwInfoRecognizedResources] = "Recursos reconocidos del firmware: %zu elementos (fuentes, iconos, rangos, etiquetas, controles, pantallas)";
    s[(int)Str::FwInfoRecordsTable] = "Tabla de registros de hardware";
    s[(int)Str::FwInfoColRecord] = "Registro #";
    s[(int)Str::FwInfoColDevCode] = "Código de dispositivo";
    s[(int)Str::FwInfoColPayload] = "Tamaño de datos";
    s[(int)Str::FwInfoColImage] = "Tamaño de imagen";
    s[(int)Str::FwInfoColFlashBase] = "Base Flash";
    s[(int)Str::FwInfoColMSP] = "Puntero de pila (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Vector de reinicio";
    s[(int)Str::BtnRevertAllModifications] = "Revertir todas las modificaciones";
    s[(int)Str::MsgReadyOpenFw] = "Listo. Abre un archivo .zwdf/.xwdf o genera fuentes personalizadas.";
    s[(int)Str::MsgSuccessOpened] = "Abierto: %s";
    s[(int)Str::MsgOpenError] = "Error al abrir: %s";
    s[(int)Str::MsgAllReverted] = "Todas las modificaciones revertidas al estado original del firmware.";
    s[(int)Str::MsgExportSuccess] = "Exportados %d recursos a: %s";
    s[(int)Str::MsgExportError] = "Error de exportación: %s";
    s[(int)Str::MsgImportSuccess] = "Importados %d PNGs desde carpeta.";
    s[(int)Str::MsgImportError] = "Error de importación: %s";
    s[(int)Str::MsgBuildSuccess] = "Compilación EXITOSA: %s";
    s[(int)Str::MsgBuildError] = "Error de compilación: %s";
    s[(int)Str::MsgFontNotLoaded] = "¡Error: No hay fuente cargada en el generador!";
    s[(int)Str::MsgFontInjectedPreset] = "¡%d glifos de %s inyectados en la memoria del firmware!";
    s[(int)Str::MsgFontInjectedAll] = "Glifos aplicados: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d archivos PNG exportados a: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d archivos para %s exportados a: %s";
    s[(int)Str::MsgFontExportError] = "Error de exportación: %s";
    s[(int)Str::ModalBuildReportTitle] = "Informe de compilación";
    s[(int)Str::ModalBuildSuccessBanner] = "¡COMPILACIÓN Y VERIFICACIÓN DEL FIRMWARE EXITOSA!";
    s[(int)Str::ModalSavedFile] = "Archivo guardado: %s";
    s[(int)Str::ModalOutputSize] = "Tamaño de salida: %zu bytes";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Recursos bitmap modificados: %zu";
    s[(int)Str::ModalModColorGroups] = "Grupos de color modificados: %zu";
    s[(int)Str::ModalPatchedRanges] = "Rangos de bytes parcheados: %zu en %zu registros";
    s[(int)Str::ModalUntouchedBytes] = "Bytes sin modificar: %zu bytes idénticos al original";
    s[(int)Str::ModalReportNote] = "Un informe detallado se guardó como .report.json junto al firmware.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Abrir en Explorador";
    s[(int)Str::BtnCopyClipboard] = "Copiar (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Pegar (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Deshacer";
    s[(int)Str::BtnRedo] = "Rehacer";
    s[(int)Str::TooltipUndo] = "Deshacer: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Rehacer: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Nada que deshacer (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Nada que rehacer (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Herramientas:";
    s[(int)Str::ToolPencil] = "Lápiz";
    s[(int)Str::ToolEraser] = "Borrador";
    s[(int)Str::BtnInvertPixels] = "Invertir";
    s[(int)Str::BtnClearPixels] = "Limpiar";
    s[(int)Str::BtnFlipH] = "Voltear H";
    s[(int)Str::BtnFlipV] = "Voltear V";
    s[(int)Str::MsgCopiedToClipboard] = "Imagen de %s copiada al portapapeles.";
    s[(int)Str::MsgPastedFromClipboard] = "¡Imagen pegada desde el portapapeles para %s!";
    s[(int)Str::MsgClipboardNoImage] = "No se encontró una imagen compatible en el portapapeles.";
    s[(int)Str::MsgUndoPerformed] = "Deshecho: %s";
    s[(int)Str::MsgRedoPerformed] = "Rehecho: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Firmware: %s (SHA256: %s... | %zu registros | %zu modif.)";
    s[(int)Str::BadgeProjectSummary] = "| Proyecto: %s (SHA256: %s... | %zu registros | %zu modif.)";
    s[(int)Str::LabelCategory] = "Categoría";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Grupo: %s  |  Color: Personalizado (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Grupo: %s  |  Color: %s";
    s[(int)Str::LabelMetaHardware] = "Dirección Flash: 0x%08X  |  Dimensiones: %u x %u px  |  Bytes: %zu  |  Registros: %zu";
    s[(int)Str::LabelPixelCoord] = "Píxel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| BIZQ: %s  |  BDER: Borrar";
    s[(int)Str::LabelDrawAction] = "Dibujar";
    s[(int)Str::LabelEraseAction] = "Borrar";
    s[(int)Str::LabelFontPreset] = "Preajuste";
    s[(int)Str::LabelFontPixelGrid] = "Cuadrícula de píxeles";
    s[(int)Str::MsgAppliedColorTheme] = "Tema de color aplicado: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "Se importaron %zu PNG coincidentes de la carpeta.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Firmware abierto mediante Arrastrar y Soltar: %s";
    s[(int)Str::MsgDragDropError] = "Error de Arrastrar y Soltar: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Fuente cargada mediante Arrastrar y Soltar: %s";
    s[(int)Str::MsgFailedLoadFont] = "Error al cargar el archivo de fuente: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "El PNG coincide con el original para %s.";
    s[(int)Str::MsgStagedReplacement] = "¡Reemplazo preparado para %s desde PNG!";
    s[(int)Str::MsgPngLoadError] = "Error al cargar PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "El reemplazo para %s coincide con el original.";
    s[(int)Str::MsgStagedReplacementCustom] = "¡Reemplazo preparado para %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "¡Reemplazo RGB565 preparado para %s!";
    s[(int)Str::MsgRestoredOriginal] = "Recurso %s restaurado al original del firmware.";
    s[(int)Str::MsgSavedAssetPng] = "%s guardado en: %s";
    s[(int)Str::MsgSaveError] = "Error al guardar: %s";
    s[(int)Str::MsgSavedBootLogo] = "Logotipo de arranque horizontal guardado en: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Error: ¡no se puede sobrescribir el firmware de origen! Elija otro nombre.";
    s[(int)Str::MsgColorPatchError] = "Error en el parche de color: %s";
    s[(int)Str::MsgPackError] = "Error de empaquetado: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Se importaron %zu PNG modificados (%zu coincidieron con el original)";

    s[(int)Str::LabelBuiltin] = "integrado";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "No se pudo cargar el firmware integrado: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Firmware integrado cargado: %s";
    s[(int)Str::ThemeFactoryDefault] = "Predeterminado de fábrica";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Estilo original de fábrica Voopoo rojo y amarillo";
    s[(int)Str::ThemeCyberpunkNeon] = "Cyberpunk Neón";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Cian eléctrico, rosa neón y violeta vívido";
    s[(int)Str::ThemeMatrixGreen] = "Verde Matrix";
    s[(int)Str::ThemeMatrixGreenDesc] = "Fósforo de terminal CRT y verde esmeralda";
    s[(int)Str::ThemeArcticIceBlue] = "Azul hielo ártico";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Cian glaciar, cielo profundo y resplandor azur";
    s[(int)Str::ThemeAmberFire] = "Fuego de ámbar";
    s[(int)Str::ThemeAmberFireDesc] = "Naranja lava, llama dorada y brasa";
    s[(int)Str::ThemePurpleNebula] = "Nebulosa púrpura";
    s[(int)Str::ThemePurpleNebulaDesc] = "Violeta profundo, lavanda y amatista neón";
    s[(int)Str::ThemeStealthDarkGray] = "Gris oscuro Stealth";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Grafito mate, pizarra y metal de pistola";
    s[(int)Str::RoleAccentRed] = "Acento / modo rojo";
    s[(int)Str::RoleAccentRedScope] = "Acento principal de interfaz en modos estándar — 8 lugares";
    s[(int)Str::RoleAccentCyan] = "Acento / modo cian";
    s[(int)Str::RoleAccentCyanScope] = "Acento principal de interfaz en SMART/RBA — 7 lugares";
    s[(int)Str::RoleHighlightYellow] = "Resaltado / amarillo";
    s[(int)Str::RoleHighlightYellowScope] = "Franja de puntuación y paneles de servicio — 4 lugares";
    s[(int)Str::RoleRankBronze] = "Rango / bronce";
    s[(int)Str::RoleRankBronzeScope] = "Paleta de rango Bronce";
    s[(int)Str::RoleRankGold] = "Rango / oro";
    s[(int)Str::RoleRankGoldScope] = "Paleta de rango Oro";
    s[(int)Str::RoleRankIce] = "Rango / platino (hielo)";
    s[(int)Str::RoleRankIceScope] = "Paleta de rango Platino / Hielo";
    s[(int)Str::RoleRankCyan] = "Rango / cian";
    s[(int)Str::RoleRankCyanScope] = "Paleta de rango Cian";
    s[(int)Str::RoleRankOrange] = "Rango / naranja";
    s[(int)Str::RoleRankOrangeScope] = "Paleta de rango Naranja";
    s[(int)Str::RoleRankRed] = "Rango / rojo";
    s[(int)Str::RoleRankRedScope] = "Paleta de rango Rojo";
    s[(int)Str::RoleDigitsWattage] = "Dígitos de potencia";
    s[(int)Str::RoleDigitsWattageScope] = "Lectura grande de vataje en pantalla principal (19x40)";
    s[(int)Str::RoleDigitsStats] = "Dígitos de estadísticas";
    s[(int)Str::ColorPresetWhite] = "Blanco (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Principal / Rojo (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Cian (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Servicio / Amarillo (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bronce (#B57947)";
    s[(int)Str::ColorPresetGold] = "Oro (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Naranja (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Verde (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Dígitos principales 0-9, 10 archivos)";
    s[(int)Str::FontPreset11x20] = "11x20 (Dígitos secundarios 0-9, 10 archivos)";
    s[(int)Str::FontPreset7x13] = "7x13 (Estado pequeño 0-9, A-F, 16 archivos)";
    s[(int)Str::FontPreset6x10] = "6x10 (Micro estado 0-9, 10 archivos)";
    s[(int)Str::FontOs1x] = "1x (Directo)";
    s[(int)Str::FontOs2x] = "2x (Rápido)";
    s[(int)Str::FontOs4x] = "4x (Recomendado)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

static void InitFrench() {
    auto& s = g_table[(int)Language::French];
    s[(int)Str::SupportFull] = "Complète";
    s[(int)Str::SupportNone] = "Non prise en charge";
    s[(int)Str::SupportPalette] = "Palette de couleurs";
    s[(int)Str::SupportMonochrome] = "Écran monochrome ; aucune palette de couleurs.";
    s[(int)Str::EmulatorHistoryDay] = "Jour de l’historique";
    s[(int)Str::EmulatorClockHour] = "Heures";
    s[(int)Str::EmulatorClockMinute] = "Minutes";
    s[(int)Str::EmulatorClockField] = "Champ sélectionné";
    s[(int)Str::MenuFile] = "Fichier";
    s[(int)Str::MenuEdit] = "Édition";
    s[(int)Str::MenuLanguage] = "Langue";
    s[(int)Str::MenuHelp] = "Aide";
    s[(int)Str::UpdateTitle] = "Mises à jour du studio";
    s[(int)Str::UpdateVersion] = "Version installée : %s";
    s[(int)Str::UpdateCheck] = "Rechercher des mises à jour";
    s[(int)Str::UpdateChecking] = "Vérification de GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "La dernière version est installée.";
    s[(int)Str::UpdateDownload] = "Télécharger";
    s[(int)Str::UpdateDownloading] = "Téléchargement et vérification...";
    s[(int)Str::UpdateInstall] = "Installer et redémarrer";
    s[(int)Str::UpdateReady] = "Prêt à installer.";
    s[(int)Str::UpdateChangelog] = "Modifications";
    s[(int)Str::UpdateFailed] = "Échec de la mise à jour";
    s[(int)Str::UpdateAvailable] = "Version disponible : %s";
    s[(int)Str::MenuCreate] = "Nouveau...";
    s[(int)Str::MenuOpen] = "Ouvrir...";
    s[(int)Str::MenuImport] = "Importer les ressources...";
    s[(int)Str::MenuExport] = "Exporter les ressources...";
    s[(int)Str::MenuBuild] = "Générer le firmware...";
    s[(int)Str::MenuExit] = "Quitter";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ressources";
    s[(int)Str::TabFontGenerator] = "Générateur de polices";
    s[(int)Str::TabFirmwareInfo] = "À propos du firmware";
    s[(int)Str::TabFirmwareComparison] = "Comparaison";
    s[(int)Str::CompareModeTwoFiles] = "Deux fichiers";
    s[(int)Str::CompareModeEditor] = "Éditeur et fichier";
    s[(int)Str::CompareLeft] = "Gauche";
    s[(int)Str::CompareRight] = "Droite";
    s[(int)Str::CompareChooseFile] = "Choisir un fichier...";
    s[(int)Str::CompareReadOnlyNote] = "Lecture seule ; inclut les modifications de l’éditeur.";
    s[(int)Str::CompareChooseSources] = "Choisissez les deux sources à comparer.";
    s[(int)Str::CompareEditorUnavailable] = "Aucun firmware ouvert dans l’éditeur.";
    s[(int)Str::CompareChanged] = "Modifié";
    s[(int)Str::CompareAdded] = "Ajouté";
    s[(int)Str::CompareRemoved] = "Supprimé";
    s[(int)Str::CompareEqual] = "Identique";
    s[(int)Str::CompareIncompatible] = "Incompatible";
    s[(int)Str::CompareDecodeError] = "Erreur de décodage";
    s[(int)Str::CompareStatusFilter] = "État";
    s[(int)Str::CompareDifferencePreview] = "Différence";
    s[(int)Str::CompareHighlightPixels] = "Surligner les différences";
    s[(int)Str::CompareDifferentPixels] = "%zu / %zu pixels diffèrent";
    s[(int)Str::CompareMissingResource] = "Ressource absente";
    s[(int)Str::CompareIncompatibleHint] = "Types ou dimensions différents : comparaison des pixels indisponible.";
    s[(int)Str::CompareImageUnavailable] = "Image indisponible";
    s[(int)Str::CompareTextureError] = "Impossible de créer la texture d’aperçu.";
    s[(int)Str::CompareAllStatuses] = "Tous les états";
    s[(int)Str::TabFirmwareEmulator] = "Émulateur";
    s[(int)Str::TabFirmwareLayout] = "Disposition de l’interface";
    s[(int)Str::LayoutUnsupported] = "La modification de la disposition est indisponible pour ce firmware.";
    s[(int)Str::LayoutNativeNote] = "Déplacez un élément ou saisissez X/Y. L’aperçu exécute le firmware modifié ; les changements concernent la révision matérielle sélectionnée.";
    s[(int)Str::LayoutRevision] = "Révision matérielle";
    s[(int)Str::LayoutCheckingRevisions] = "Vérification des interfaces identiques…";
    s[(int)Str::LayoutAllRevisions] = "Toutes les révisions";
    s[(int)Str::LayoutRevisionGroup] = "Interface identique : %s";
    s[(int)Str::LayoutRevisionIndividual] = "Révision : %s";
    s[(int)Str::LayoutResetScope] = "Réinitialiser les révisions sélectionnées";
    s[(int)Str::LayoutSelectionUnavailable] = "Aucun élément compatible dans toutes les révisions sélectionnées.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Éléments d’interface";
    s[(int)Str::LayoutNoElements] = "Aucun bloc déplaçable sur cet écran.";
    s[(int)Str::LayoutSharedBlock] = "Ce bloc partage ses coordonnées avec d’autres écrans.";
    s[(int)Str::LayoutPositionX] = "X (pixels)";
    s[(int)Str::LayoutPositionY] = "Y (pixels)";
    s[(int)Str::LayoutOffset] = "Décalage : %+d, %+d px";
    s[(int)Str::LayoutGridStep] = "Pas : %d × %d px";
    s[(int)Str::LayoutGridSpacing] = "Pas de grille (px)";
    s[(int)Str::LayoutGridConstraint] = "Si aucun point de grille n’est représentable, les pas et limites natifs des coordonnées sont prioritaires.";
    s[(int)Str::LayoutSnapGrid] = "Aligner sur la grille";
    s[(int)Str::LayoutSnapElements] = "Aligner sur les bords et centres des éléments";
    s[(int)Str::LayoutGuides] = "Guides de déplacement";
    s[(int)Str::LayoutAlign] = "Aligner sur l’écran";
    s[(int)Str::LayoutAlignLeft] = "Gauche";
    s[(int)Str::LayoutAlignCenter] = "Centre X";
    s[(int)Str::LayoutAlignRight] = "Droite";
    s[(int)Str::LayoutAlignTop] = "Haut";
    s[(int)Str::LayoutAlignMiddle] = "Centre Y";
    s[(int)Str::LayoutAlignBottom] = "Bas";
    s[(int)Str::LayoutNudgeHint] = "Flèches : pas natif en pixels ; Shift : pas plus grand. Cliquez sur l’écran pour lui donner le focus.";
    s[(int)Str::LayoutOverlays] = "Contours des éléments";
    s[(int)Str::LayoutOverlapOverlay] = "Surligner les chevauchements";
    s[(int)Str::LayoutOverlaps] = "Éléments qui se croisent";
    s[(int)Str::LayoutNoOverlaps] = "Aucune intersection de rectangles";
    s[(int)Str::LayoutDrawOrderUnknown] = "Ordre de dessin natif non vérifié";
    s[(int)Str::LayoutDrawOrderPair] = "%s (n°%d) est dessiné après %s (n°%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Ordre des appels du redessin principal, et non couches permanentes. Les mises à jour peuvent différer ; seuls les pixels réellement écrits recouvrent le contenu précédent.";
    s[(int)Str::LayoutDrawSequence] = "Séquence de dessin native";
    s[(int)Str::LayoutDrawInterleaved] = "Les appels sont entrelacés ; le chevauchement dépend de la partie de chaque élément.";
    s[(int)Str::EmulatorBefore] = "Avant (firmware ouvert)";
    s[(int)Str::EmulatorAfter] = "Après (modifications actuelles)";
    s[(int)Str::EmulatorSideBySide] = "Côte à côte";
    s[(int)Str::EmulatorOriginalError] = "Échec de l’aperçu du firmware ouvert : %s";
    s[(int)Str::MenuSaveProject] = "Enregistrer le projet";
    s[(int)Str::MenuSaveProjectAs] = "Enregistrer le projet sous...";
    s[(int)Str::MsgProjectSaved] = "Projet enregistré : %s";
    s[(int)Str::MenuBuild] = "Exporter le firmware";
    s[(int)Str::BtnOpenFirmware] = "Ouvrir un firmware ou un projet...";
    s[(int)Str::StartupOpenFile] = "Ouvrir un firmware ou un projet...";
    s[(int)Str::UnsavedPrompt] = "Enregistrer le projet avant de continuer ?";
    s[(int)Str::UnsavedPrompt] = "Enregistrer le projet avant de continuer ?";
    s[(int)Str::LayoutBounds] = "X : %d…%d / Y : %d…%d";
    s[(int)Str::LayoutResetElement] = "Réinitialiser l’élément";
    s[(int)Str::LayoutResetRevision] = "Réinitialiser la disposition de cette révision";
    s[(int)Str::LayoutResetAll] = "Réinitialiser toutes les dispositions";
    s[(int)Str::LayoutPatchSites] = "Adresses des correctifs";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "État de l’aperçu";
    s[(int)Str::LayoutMoveElement] = "Déplacer un élément d’interface";
    s[(int)Str::LayoutRba] = "Mode RBA";
    s[(int)Str::LayoutTemperatureControl] = "Contrôle de température";
    s[(int)Str::LayoutTemperature] = "Température (°C)";
    s[(int)Str::LayoutRenderFailed] = "Le tampon d’image natif est indisponible.";
    s[(int)Str::LayoutElementPower] = "Puissance";
    s[(int)Str::LayoutElementBattery] = "Batterie";
    s[(int)Str::LayoutElementResistance] = "Résistance";
    s[(int)Str::LayoutElementDuration] = "Durée";
    s[(int)Str::LayoutElementPuffs] = "Bouffées";
    s[(int)Str::LayoutElementMode] = "Mode";
    s[(int)Str::LayoutElementStatus] = "État";
    s[(int)Str::LayoutElementScore] = "Score";
    s[(int)Str::LayoutElementStatistics] = "Statistiques";
    s[(int)Str::LayoutElementVoltage] = "Tension";
    s[(int)Str::LayoutElementGraph] = "Graphique";
    s[(int)Str::LayoutElementRank] = "Rang";
    s[(int)Str::LayoutElementModeSelection] = "Sélection du mode";
    s[(int)Str::LayoutElementLogo] = "Logo";
    s[(int)Str::LayoutChangedElements] = "Éléments d’interface modifiés";
    s[(int)Str::EmulatorSupported] = "Aperçu natif de l’interface de tous les firmwares DRAG intégrés.";
    s[(int)Str::EmulatorNativeNote] = "Rendu natif avec les modifications actuelles. Sans émulation électrique.";
    s[(int)Str::EmulatorWatts] = "Puissance (W)";
    s[(int)Str::EmulatorPuffs] = "Bouffées";
    s[(int)Str::EmulatorBattery] = "Batterie (0–5)";
    s[(int)Str::EmulatorResistance] = "Résistance (mΩ)";
    s[(int)Str::EmulatorDuration] = "Durée (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Verrouillé";
    s[(int)Str::EmulatorCharging] = "En charge";
    s[(int)Str::EmulatorScore] = "Score total";
    s[(int)Str::EmulatorScoreDisplay] = "Meilleur score";
    s[(int)Str::EmulatorRedraw] = "Redessiner";
    s[(int)Str::EmulatorLandscape] = "Paysage";
    s[(int)Str::EmulatorScreen] = "Écran";
    s[(int)Str::EmulatorMainScreen] = "Écran principal";
    s[(int)Str::FontSystemFonts] = "Polices système";
    s[(int)Str::FontUserFonts] = "Polices utilisateur";
    s[(int)Str::EmulatorMenuItem] = "Élément du menu";
    s[(int)Str::EmulatorMaterial] = "Matériau";
    s[(int)Str::EmulatorMenuConfirmed] = "Confirmé";
    s[(int)Str::EmulatorResources] = "Ressources lues par le firmware";
    s[(int)Str::EmulatorNoResources] = "Aucune lecture des ressources du catalogue.";
    s[(int)Str::EmulatorUidNote] = "UID du MCU émulé, pas celui d’un appareil connecté.";
    s[(int)Str::EmulatorLoaderVersion] = "Version virtuelle du chargeur";
    s[(int)Str::EmulatorLoaderId] = "Octets CHIP-ID virtuels";
    s[(int)Str::EmulatorLoaderNote] = "Les données du chargeur sont émulées, pas lues dans le firmware ou l’appareil.";
    s[(int)Str::TabFirmwarePatches] = "Correctifs";
    s[(int)Str::PatchBootLogo] = "Écran de démarrage";
    s[(int)Str::PatchShutdownLogo] = "Écran d’arrêt";
    s[(int)Str::PatchDeviceInfo] = "Écran CHIP ID / version";
    s[(int)Str::PatchDeviceInfoNote] = "Image statique uniquement. Remplace l’écran de service, jamais l’écran principal de puissance et de batterie.";
    s[(int)Str::PatchLogoNote] = "Images indépendantes de 80 × 160 pour toutes les révisions DRAG S Score / Factory. Non vérifié sur appareil.";
    s[(int)Str::PatchUnsupported] = "Ce correctif nécessite un firmware DRAG S Score ou Factory.";
    s[(int)Str::PatchEnabled] = "Activer le logo personnalisé";
    s[(int)Str::PatchCanvas] = "Composition · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Aperçu natif de l’écran";
    s[(int)Str::PatchPreviewNote] = "L’aperçu natif montre les réglages appliqués, pas le brouillon de composition.";
    s[(int)Str::PatchFit] = "Ajuster";
    s[(int)Str::PatchCover] = "Remplir";
    s[(int)Str::PatchStretch] = "Étirer";
    s[(int)Str::PatchScale] = "Échelle";
    s[(int)Str::PatchAnimation] = "Animation d’entrée";
    s[(int)Str::PatchNone] = "Aucune";
    s[(int)Str::PatchFade] = "Fondu";
    s[(int)Str::PatchDuration] = "Durée (ms)";
    s[(int)Str::PatchAutoHardware] = "Automatique";
    s[(int)Str::PatchAnimationPhase] = "Phase de l’animation";
    s[(int)Str::PatchPlay] = "Lire";
    s[(int)Str::EmulatorModeScreens] = "Écrans";
    s[(int)Str::EmulatorModeDevice] = "Appareil";
    s[(int)Str::DeviceUnsupported] = "Le mode appareil prend en charge DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Firmware exécuté depuis Reset avec périphériques virtuels. Chauffage simulé ; sécurité électrique non vérifiée.";
    s[(int)Str::DeviceReset] = "Redémarrer";
    s[(int)Str::DevicePause] = "Pause";
    s[(int)Str::DeviceResume] = "Exécuter";
    s[(int)Str::DeviceStep] = "Pas";
    s[(int)Str::DeviceChanged] = "Les modifications actuelles ne sont pas encore chargées.";
    s[(int)Str::DeviceApply] = "Redémarrer avec modifications";
    s[(int)Str::DeviceButtons] = "Maintenez les boutons ou espace / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Résistance connectée";
    s[(int)Str::DeviceTime] = "Temps virtuel";
    s[(int)Str::DeviceSleeping] = "CPU en veille";
    s[(int)Str::DeviceLoading] = "Chargement de l’appareil...";
    s[(int)Str::DeviceLog] = "Accès aux périphériques";
    s[(int)Str::BadgeNoFirmware] = "| [Aucun firmware ouvert — glissez un .zwdf/.xwdf ici]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu éléments | %zu enregistrements";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Ouvrir firmware...";
    s[(int)Str::BtnExportAll] = "Exporter toutes les ressources...";
    s[(int)Str::BtnImportFolder] = "Importer un dossier de PNGs...";
    s[(int)Str::BtnBuildFirmware] = " CRÉER FIRMWARE ";
    s[(int)Str::BtnRevertAllStaged] = "Ignorer toutes les modifications";
    s[(int)Str::GroupAll] = "Tous les groupes";
    s[(int)Str::GroupAlerts] = "Alertes (dialogues et icônes)";
    s[(int)Str::GroupFonts] = "Polices (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Scores et rangs";
    s[(int)Str::GroupStatusLabels] = "Statut et libellés";
    s[(int)Str::GroupControls] = "Contrôles et batterie";
    s[(int)Str::GroupScreens] = "Écrans et logos de démarrage";
    s[(int)Str::GroupAnimations] = "Animations";
    s[(int)Str::PanelResourcesHeader] = "Ressources";
    s[(int)Str::LabelSearch] = "Recherche";
    s[(int)Str::SearchPlaceholder] = "filtrer par titre, id, adresse...";
    s[(int)Str::NoFirmwareLoadedList] = "Aucun firmware chargé.\nFichier > Nouveau / Ouvrir";
    s[(int)Str::NoResourcesMatchFilter] = "Aucune ressource ne correspond au filtre.";
    s[(int)Str::PanelResourcePreviewHeader] = "Aperçu";
    s[(int)Str::SelectResourcePrompt] = "Sélectionnez une ressource dans la liste à gauche pour prévisualiser et modifier.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Groupe";
    s[(int)Str::LabelColor] = "Couleur";
    s[(int)Str::LabelColorCustom] = "Personnalisé";
    s[(int)Str::LabelFlashAddress] = "Adresse Flash";
    s[(int)Str::LabelDimensions] = "Dimensions";
    s[(int)Str::LabelBytes] = "Octets";
    s[(int)Str::LabelRecords] = "Enregistrements";
    s[(int)Str::StatusModifiedStaged] = "Statut : MODIFIÉ (en mémoire)";
    s[(int)Str::StatusOriginal] = "Statut : ORIGINAL (du firmware)";
    s[(int)Str::StatusStockOriginal] = "Original (conforme à la référence)";
    s[(int)Str::StatusCustomDiffers] = "Personnalisé (diffère de la référence)";
    s[(int)Str::BtnRestoreStock] = "Restaurer la référence";
    s[(int)Str::LabelCompareWithStock] = "Comparer avec la référence";
    s[(int)Str::LabelActivePreview] = "Actuel (actif) :";
    s[(int)Str::LabelStockReferencePreview] = "Référence d'usine :";
    s[(int)Str::FilterOnlyCustom] = "Uniquement différents de la référence";
    s[(int)Str::MsgRestoredToStock] = "%s restauré à la référence d'usine.";
    s[(int)Str::BtnReplaceFromPng] = "Remplacer depuis PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Restaurer l'original";
    s[(int)Str::BtnExportThisPng] = "Exporter PNG...";
    s[(int)Str::BtnExportHorizontal] = "Exporter horizontal (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Palette de couleurs et rendu (RGB565)";
    s[(int)Str::LabelTheme] = "Thème";
    s[(int)Str::BtnApply] = "Appliquer";
    s[(int)Str::BtnResetAll] = "Tout réinitialiser";
    s[(int)Str::LabelRole] = "Rôle";
    s[(int)Str::BtnResetRole] = "Réinitialiser rôle";
    s[(int)Str::PromptLoadFwColors] = "Chargez le firmware pour inspecter et modifier les couleurs.";
    s[(int)Str::LabelPreviewTint] = "Teinte de l'aperçu";
    s[(int)Str::InfoDigitsMonochrome] = "Les chiffres sont des masques monochromes. Leur couleur d'affichage est définie par le code firmware, pas stockée dans le glyphe.";
    s[(int)Str::InfoPreviewNotSaved] = "La teinte d'aperçu n'est pas enregistrée dans le firmware. L'export PNG RGB565 inclut cette teinte.";
    s[(int)Str::LabelPreviewOnly] = "Aperçu uniquement :";
    s[(int)Str::LabelPresets] = "Préréglages...";
    s[(int)Str::BtnResetColor] = "Réinitialiser couleur";
    s[(int)Str::LabelZoom] = "Zoom aperçu";
    s[(int)Str::LabelPixelGrid] = "Grille de pixels";
    s[(int)Str::LabelHorizontalPreview] = "Aperçu horizontal (136x22) :";
    s[(int)Str::FontSec1Title] = "1. Sélection de police";
    s[(int)Str::FontSec1WindowsFont] = "Police Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Parcourir fichier TTF/OTF...";
    s[(int)Str::FontSec1ActiveFont] = "Police active : %s";
    s[(int)Str::FontSec2Title] = "2. Préréglage d'affichage";
    s[(int)Str::FontSec3Title] = "3. Paramètres de rendu";
    s[(int)Str::FontSec3Threshold] = "Seuil";
    s[(int)Str::FontSec3ThresholdHint] = "Plus bas = plus gras, Plus haut = plus fin";
    s[(int)Str::FontSec3Stretch] = "Étirer pour remplir";
    s[(int)Str::FontSec3Baseline] = "Alignement de base";
    s[(int)Str::FontSec3CenterV] = "Centrer verticalement";
    s[(int)Str::FontSec3PaddingX] = "Marge X";
    s[(int)Str::FontSec3PaddingY] = "Marge Y";
    s[(int)Str::FontSec3Oversampling] = "Suréchantillonnage";
    s[(int)Str::FontSec4Title] = "4. Intégration firmware";
    s[(int)Str::FontSec4ApplyPreset] = " > APPLIQUER LES GLYPHES AU FIRMWARE OUVERT < ";
    s[(int)Str::FontSec4ApplyAll] = "Appliquer toutes les polices";
    s[(int)Str::FontSec5Title] = "5. Exporter en PNGs";
    s[(int)Str::FontSec5OutputFolder] = "Dossier de sortie";
    s[(int)Str::FontSec5BrowseFolder] = "Parcourir dossier...";
    s[(int)Str::FontSec5OpenExplorer] = "Ouvrir dans l'Explorateur";
    s[(int)Str::FontSec5ExportAllPreset] = "Exporter tous les préréglages";
    s[(int)Str::FontSec5ExportButton] = "Exporter PNGs 1-Bit sur le disque";
    s[(int)Str::FontRightLivePreview] = "Aperçu en direct";
    s[(int)Str::FontRightNoGlyphs] = "Aucun glyphe chargé.";
    s[(int)Str::FontPresetInfo] = "Préréglage : %s (%dx%d px, %d glyphes, taille : %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 Spécification du paquet";
    s[(int)Str::FwInfoNoFirmware] = "Aucun firmware chargé. Ouvrez un fichier .zwdf/.xwdf dans l'onglet Studio de ressources.";
    s[(int)Str::FwInfoFilePath] = "Chemin du fichier : %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 source : %s";
    s[(int)Str::FwInfoPackageSize] = "Taille du paquet : %zu octets";
    s[(int)Str::FwInfoHwRevisions] = "Révisions matérielles : %zu enregistrements";
    s[(int)Str::FwInfoRecognizedResources] = "Ressources firmware reconnues : %zu éléments (polices, icônes, rangs, libellés, contrôles, écrans)";
    s[(int)Str::FwInfoRecordsTable] = "Table des enregistrements matériels";
    s[(int)Str::FwInfoColRecord] = "Enregistrement #";
    s[(int)Str::FwInfoColDevCode] = "Code appareil";
    s[(int)Str::FwInfoColPayload] = "Taille des données";
    s[(int)Str::FwInfoColImage] = "Taille de l'image";
    s[(int)Str::FwInfoColFlashBase] = "Base Flash";
    s[(int)Str::FwInfoColMSP] = "Pointeur de pile (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Vecteur de réinitialisation";
    s[(int)Str::BtnRevertAllModifications] = "Annuler toutes les modifications";
    s[(int)Str::MsgReadyOpenFw] = "Prêt. Ouvrez un fichier firmware .zwdf/.xwdf ou générez des polices.";
    s[(int)Str::MsgSuccessOpened] = "Ouvert : %s";
    s[(int)Str::MsgOpenError] = "Erreur d'ouverture : %s";
    s[(int)Str::MsgAllReverted] = "Toutes les modifications annulées, état d'origine du firmware restauré.";
    s[(int)Str::MsgExportSuccess] = "%d ressources exportées dans : %s";
    s[(int)Str::MsgExportError] = "Erreur d'export : %s";
    s[(int)Str::MsgImportSuccess] = "%d PNGs importés depuis le dossier.";
    s[(int)Str::MsgImportError] = "Erreur d'import : %s";
    s[(int)Str::MsgBuildSuccess] = "Build RÉUSSI : %s";
    s[(int)Str::MsgBuildError] = "Erreur de build : %s";
    s[(int)Str::MsgFontNotLoaded] = "Erreur : Aucune police chargée dans le générateur !";
    s[(int)Str::MsgFontInjectedPreset] = "%d glyphes de %s injectés dans la mémoire du firmware !";
    s[(int)Str::MsgFontInjectedAll] = "Glyphes appliqués : %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d fichiers PNG exportés dans : %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d fichiers pour %s exportés dans : %s";
    s[(int)Str::MsgFontExportError] = "Erreur d'export : %s";
    s[(int)Str::ModalBuildReportTitle] = "Rapport de build";
    s[(int)Str::ModalBuildSuccessBanner] = "BUILD ET VÉRIFICATION DU FIRMWARE RÉUSSIS !";
    s[(int)Str::ModalSavedFile] = "Fichier enregistré : %s";
    s[(int)Str::ModalOutputSize] = "Taille de sortie : %zu octets";
    s[(int)Str::ModalSha256] = "SHA-256 : %s";
    s[(int)Str::ModalModBitmaps] = "Ressources bitmap modifiées : %zu";
    s[(int)Str::ModalModColorGroups] = "Groupes de couleurs modifiés : %zu";
    s[(int)Str::ModalPatchedRanges] = "Plages d'octets patchées : %zu sur %zu enregistrements";
    s[(int)Str::ModalUntouchedBytes] = "Octets non modifiés : %zu octets identiques à l'original";
    s[(int)Str::ModalReportNote] = "Un rapport détaillé a été enregistré en .report.json à côté du firmware.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Ouvrir dans l'Explorateur";
    s[(int)Str::BtnCopyClipboard] = "Copier (Ctrl+C)";
    s[(int)Str::BtnPasteClipboard] = "Coller (Ctrl+V)";
    s[(int)Str::BtnUndo] = "Annuler";
    s[(int)Str::BtnRedo] = "Rétablir";
    s[(int)Str::TooltipUndo] = "Annuler: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Rétablir: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Rien à annuler (Ctrl+Z)";
    s[(int)Str::TooltipRedoEmpty] = "Rien à rétablir (Ctrl+Y)";
    s[(int)Str::LabelPixelTools] = "Outils pixel:";
    s[(int)Str::ToolPencil] = "Crayon";
    s[(int)Str::ToolEraser] = "Gomme";
    s[(int)Str::BtnInvertPixels] = "Inverser";
    s[(int)Str::BtnClearPixels] = "Effacer";
    s[(int)Str::BtnFlipH] = "Miroir H";
    s[(int)Str::BtnFlipV] = "Miroir V";
    s[(int)Str::MsgCopiedToClipboard] = "Image de %s copiée dans le presse-papiers.";
    s[(int)Str::MsgPastedFromClipboard] = "Image collée depuis le presse-papiers pour %s !";
    s[(int)Str::MsgClipboardNoImage] = "Aucune image compatible dans le presse-papiers.";
    s[(int)Str::MsgUndoPerformed] = "Annulé: %s";
    s[(int)Str::MsgRedoPerformed] = "Rétabli: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| Firmware: %s (SHA256: %s... | %zu enregistrements | %zu modif.)";
    s[(int)Str::BadgeProjectSummary] = "| Projet: %s (SHA256: %s... | %zu enregistrements | %zu modif.)";
    s[(int)Str::LabelCategory] = "Catégorie";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Groupe: %s  |  Couleur: Personnalisée (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Groupe: %s  |  Couleur: %s";
    s[(int)Str::LabelMetaHardware] = "Adresse Flash: 0x%08X  |  Dimensions: %u x %u px  |  Octets: %zu  |  Enregistrements: %zu";
    s[(int)Str::LabelPixelCoord] = "Pixel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| BGS: %s  |  BDS: Effacer";
    s[(int)Str::LabelDrawAction] = "Dessiner";
    s[(int)Str::LabelEraseAction] = "Effacer";
    s[(int)Str::LabelFontPreset] = "Préréglage";
    s[(int)Str::LabelFontPixelGrid] = "Grille de pixels";
    s[(int)Str::MsgAppliedColorTheme] = "Thème de couleur appliqué: %s";
    s[(int)Str::MsgImportedMatchingPngs] = "%zu fichiers PNG correspondants importés depuis le dossier.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Firmware ouvert par glisser-déposer: %s";
    s[(int)Str::MsgDragDropError] = "Erreur de glisser-déposer: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Police chargée par glisser-déposer: %s";
    s[(int)Str::MsgFailedLoadFont] = "Échec du chargement de la police: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Le PNG déposé correspond à l'original pour %s.";
    s[(int)Str::MsgStagedReplacement] = "Remplacement préparé pour %s depuis le PNG!";
    s[(int)Str::MsgPngLoadError] = "Erreur de chargement PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Le remplacement pour %s correspond à l'original.";
    s[(int)Str::MsgStagedReplacementCustom] = "Remplacement préparé pour %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "Remplacement RGB565 préparé pour %s!";
    s[(int)Str::MsgRestoredOriginal] = "Ressource %s restaurée à l'état d'origine.";
    s[(int)Str::MsgSavedAssetPng] = "%s enregistré dans: %s";
    s[(int)Str::MsgSaveError] = "Erreur d'enregistrement: %s";
    s[(int)Str::MsgSavedBootLogo] = "Logo horizontal enregistré dans: %s";
    s[(int)Str::MsgRefuseOverwrite] = "Erreur: impossible d'écraser le firmware source! Choisissez un nouveau nom.";
    s[(int)Str::MsgColorPatchError] = "Erreur de patch de couleur: %s";
    s[(int)Str::MsgPackError] = "Erreur d'empaquetage: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu PNG modifiés importés (%zu identiques à l'original)";

    s[(int)Str::LabelBuiltin] = "intégré";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Impossible de charger le micrologiciel intégré : %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Micrologiciel intégré chargé : %s";
    s[(int)Str::ThemeFactoryDefault] = "Défaut d'usine";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Style d'usine Voopoo rouge et jaune d'origine";
    s[(int)Str::ThemeCyberpunkNeon] = "Cyberpunk Néon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Cyan électrique, rose néon et violet éclatant";
    s[(int)Str::ThemeMatrixGreen] = "Vert Matrix";
    s[(int)Str::ThemeMatrixGreenDesc] = "Phosphore d'écran CRT et vert émeraude";
    s[(int)Str::ThemeArcticIceBlue] = "Bleu glace arctique";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Cyan glacier, ciel profond et éclat d'azur";
    s[(int)Str::ThemeAmberFire] = "Feu d'ambre";
    s[(int)Str::ThemeAmberFireDesc] = "Orange lave, flamme dorée et braise";
    s[(int)Str::ThemePurpleNebula] = "Nébuleuse pourpre";
    s[(int)Str::ThemePurpleNebulaDesc] = "Violet profond, lavande et améthyste néon";
    s[(int)Str::ThemeStealthDarkGray] = "Gris foncé Stealth";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Graphite mat, ardoise et métal d'armement";
    s[(int)Str::RoleAccentRed] = "Accent / mode rouge";
    s[(int)Str::RoleAccentRedScope] = "Accent principal de l'UI en modes standard — 8 emplacements";
    s[(int)Str::RoleAccentCyan] = "Accent / mode cyan";
    s[(int)Str::RoleAccentCyanScope] = "Accent principal de l'UI en SMART/RBA — 7 emplacements";
    s[(int)Str::RoleHighlightYellow] = "Surbrillance / jaune";
    s[(int)Str::RoleHighlightYellowScope] = "Bandeau de score et panneaux de service — 4 emplacements";
    s[(int)Str::RoleRankBronze] = "Rang / bronze";
    s[(int)Str::RoleRankBronzeScope] = "Palette du rang Bronze";
    s[(int)Str::RoleRankGold] = "Rang / or";
    s[(int)Str::RoleRankGoldScope] = "Palette du rang Or";
    s[(int)Str::RoleRankIce] = "Rang / platine (glace)";
    s[(int)Str::RoleRankIceScope] = "Palette du rang Platine / Glace";
    s[(int)Str::RoleRankCyan] = "Rang / cyan";
    s[(int)Str::RoleRankCyanScope] = "Palette du rang Cyan";
    s[(int)Str::RoleRankOrange] = "Rang / orange";
    s[(int)Str::RoleRankOrangeScope] = "Palette du rang Orange";
    s[(int)Str::RoleRankRed] = "Rang / rouge";
    s[(int)Str::RoleRankRedScope] = "Palette du rang Rouge";
    s[(int)Str::RoleDigitsWattage] = "Chiffres de puissance";
    s[(int)Str::RoleDigitsWattageScope] = "Grand affichage de puissance sur l'écran principal (19x40)";
    s[(int)Str::RoleDigitsStats] = "Chiffres de statistiques";
    s[(int)Str::ColorPresetWhite] = "Blanc (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Principal / Rouge (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Cyan (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Service / Jaune (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bronze (#B57947)";
    s[(int)Str::ColorPresetGold] = "Or (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Orange (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Vert (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Chiffres principaux 0-9, 10 fichiers)";
    s[(int)Str::FontPreset11x20] = "11x20 (Chiffres secondaires 0-9, 10 fichiers)";
    s[(int)Str::FontPreset7x13] = "7x13 (Petit statut 0-9, A-F, 16 fichiers)";
    s[(int)Str::FontPreset6x10] = "6x10 (Micro statut 0-9, 10 fichiers)";
    s[(int)Str::FontOs1x] = "1x (Direct)";
    s[(int)Str::FontOs2x] = "2x (Rapide)";
    s[(int)Str::FontOs4x] = "4x (Recommandé)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

std::string GetConfigPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecA(exePath);
    char testPath[MAX_PATH];
    strcpy_s(testPath, exePath);
    PathAppendA(testPath, "studio_config.ini");
    HANDLE hFile = CreateFileA(testPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return std::string(testPath);
    }
    char appData[MAX_PATH];
    if (GetEnvironmentVariableA("LOCALAPPDATA", appData, MAX_PATH) > 0) {
        std::string dir = std::string(appData) + "\\VoopooFirmwareStudio";
        CreateDirectoryA(dir.c_str(), NULL);
        return dir + "\\studio_config.ini";
    }
    return std::string(testPath);
}

static void InitBelarusian() {
    auto& s = g_table[(int)Language::Belarusian];
    s[(int)Str::SupportFull] = "Цалкам";
    s[(int)Str::SupportNone] = "Не падтрымліваецца";
    s[(int)Str::SupportPalette] = "Колеравая палітра";
    s[(int)Str::SupportMonochrome] = "Манахромны экран; колеравай палітры няма.";
    s[(int)Str::EmulatorHistoryDay] = "Дзень гісторыі";
    s[(int)Str::EmulatorClockHour] = "Гадзіны";
    s[(int)Str::EmulatorClockMinute] = "Хвіліны";
    s[(int)Str::EmulatorClockField] = "Выбранае поле";
    s[(int)Str::MenuFile] = "Файл";
    s[(int)Str::MenuEdit] = "Праўка";
    s[(int)Str::MenuLanguage] = "Мова";
    s[(int)Str::MenuHelp] = "Даведка";
    s[(int)Str::UpdateTitle] = "Абнаўленне студыі";
    s[(int)Str::UpdateVersion] = "Усталяваная версія: %s";
    s[(int)Str::UpdateCheck] = "Праверыць абнаўленні";
    s[(int)Str::UpdateChecking] = "Праверка GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Усталяваная апошняя версія.";
    s[(int)Str::UpdateDownload] = "Спампаваць";
    s[(int)Str::UpdateDownloading] = "Спампоўванне і праверка...";
    s[(int)Str::UpdateInstall] = "Усталяваць і перазапусціць";
    s[(int)Str::UpdateReady] = "Гатова да ўсталявання.";
    s[(int)Str::UpdateChangelog] = "Змены";
    s[(int)Str::UpdateFailed] = "Памылка абнаўлення";
    s[(int)Str::UpdateAvailable] = "Даступная версія: %s";
    s[(int)Str::MenuCreate] = "Стварыць...";
    s[(int)Str::MenuOpen] = "Адкрыць...";
    s[(int)Str::MenuImport] = "Імпарт рэсурсаў...";
    s[(int)Str::MenuExport] = "Экспарт рэсурсаў...";
    s[(int)Str::MenuBuild] = "Сабраць прашыўку...";
    s[(int)Str::MenuExit] = "Выхад";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Рэсурсы";
    s[(int)Str::TabFontGenerator] = "Генератар шрыфтоў";
    s[(int)Str::TabFirmwareInfo] = "Пра прашыўку";
    s[(int)Str::TabFirmwareComparison] = "Параўнанне";
    s[(int)Str::CompareModeTwoFiles] = "Два файлы";
    s[(int)Str::CompareModeEditor] = "Рэдактар і файл";
    s[(int)Str::CompareLeft] = "Злева";
    s[(int)Str::CompareRight] = "Справа";
    s[(int)Str::CompareChooseFile] = "Выбраць файл...";
    s[(int)Str::CompareReadOnlyNote] = "Толькі прагляд; улічваюцца змены ў рэдактары.";
    s[(int)Str::CompareChooseSources] = "Выберыце абедзве крыніцы для параўнання.";
    s[(int)Str::CompareEditorUnavailable] = "У рэдактары не адкрыта прашыўка.";
    s[(int)Str::CompareChanged] = "Зменены";
    s[(int)Str::CompareAdded] = "Дададзены";
    s[(int)Str::CompareRemoved] = "Выдалены";
    s[(int)Str::CompareEqual] = "Супадае";
    s[(int)Str::CompareIncompatible] = "Несупастаўны";
    s[(int)Str::CompareDecodeError] = "Памылка дэкадавання";
    s[(int)Str::CompareStatusFilter] = "Стан";
    s[(int)Str::CompareDifferencePreview] = "Розніца";
    s[(int)Str::CompareHighlightPixels] = "Падсвяціць адрозненні";
    s[(int)Str::CompareDifferentPixels] = "Адрозніваюцца %zu / %zu пікселяў";
    s[(int)Str::CompareMissingResource] = "Рэсурс адсутнічае";
    s[(int)Str::CompareIncompatibleHint] = "Розныя тыпы або памеры рэсурсаў: папіксельнае параўнанне недаступнае.";
    s[(int)Str::CompareImageUnavailable] = "Выява недаступная";
    s[(int)Str::CompareTextureError] = "Не ўдалося стварыць тэкстуру прагляду.";
    s[(int)Str::CompareAllStatuses] = "Усе станы";
    s[(int)Str::TabFirmwareEmulator] = "Эмулятар";
    s[(int)Str::TabFirmwareLayout] = "Размяшчэнне UI";
    s[(int)Str::LayoutUnsupported] = "Рэдактар размяшчэння UI недаступны для гэтай прашыўкі.";
    s[(int)Str::LayoutNativeNote] = "Перацягніце элемент або ўвядзіце X/Y. Папярэдні прагляд выконвае змененую прашыўку; змены датычацца выбранай апаратнай рэвізіі.";
    s[(int)Str::LayoutRevision] = "Апаратная рэвізія";
    s[(int)Str::LayoutCheckingRevisions] = "Праверка аднолькавых інтэрфейсаў…";
    s[(int)Str::LayoutAllRevisions] = "Усе рэвізіі";
    s[(int)Str::LayoutRevisionGroup] = "Аднолькавы UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Рэвізія: %s";
    s[(int)Str::LayoutResetScope] = "Скінуць выбраныя рэвізіі";
    s[(int)Str::LayoutSelectionUnavailable] = "Для выбраных рэвізій няма сумяшчальнага элемента.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Элементы UI";
    s[(int)Str::LayoutNoElements] = "На гэтым экране няма блокаў для перамяшчэння.";
    s[(int)Str::LayoutSharedBlock] = "Каардынаты гэтага блока агульныя для некалькіх экранаў.";
    s[(int)Str::LayoutPositionX] = "X (пікселі)";
    s[(int)Str::LayoutPositionY] = "Y (пікселі)";
    s[(int)Str::LayoutOffset] = "Зрух: %+d, %+d пікс.";
    s[(int)Str::LayoutGridStep] = "Крок: %d × %d пікс.";
    s[(int)Str::LayoutGridSpacing] = "Крок сеткі (пікс.)";
    s[(int)Str::LayoutGridConstraint] = "Калі вузлы сеткі недаступныя, прыярытэт маюць натыўны крок і межы каардынат.";
    s[(int)Str::LayoutSnapGrid] = "Прывязка да сеткі";
    s[(int)Str::LayoutSnapElements] = "Прывязка да краёў і цэнтраў элементаў";
    s[(int)Str::LayoutGuides] = "Накіроўвалыя перамяшчэння";
    s[(int)Str::LayoutAlign] = "Выраўнаваць па экране";
    s[(int)Str::LayoutAlignLeft] = "Злева";
    s[(int)Str::LayoutAlignCenter] = "Цэнтр X";
    s[(int)Str::LayoutAlignRight] = "Справа";
    s[(int)Str::LayoutAlignTop] = "Зверху";
    s[(int)Str::LayoutAlignMiddle] = "Цэнтр Y";
    s[(int)Str::LayoutAlignBottom] = "Знізу";
    s[(int)Str::LayoutNudgeHint] = "Стрэлкі: натыўны крок у пікселях; Shift: большы крок. Націсніце на экран для фокусу.";
    s[(int)Str::LayoutOverlays] = "Контуры элементаў";
    s[(int)Str::LayoutOverlapOverlay] = "Падсвятліць перасячэнні";
    s[(int)Str::LayoutOverlaps] = "Элементы, што перасякаюцца";
    s[(int)Str::LayoutNoOverlaps] = "Перасячэнняў прамавугольнікаў няма";
    s[(int)Str::LayoutDrawOrderUnknown] = "Натыўны парадак малявання не пацверджаны";
    s[(int)Str::LayoutDrawOrderPair] = "%s (№%d) малюецца пазней за %s (№%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Парадак выклікаў асноўнага малявання, а не пастаянныя слаі. Пры абнаўленнях ён можа адрознівацца; папярэдні змест перакрываюць толькі фактычна запісаныя пікселі.";
    s[(int)Str::LayoutDrawSequence] = "Штатны парадак малявання";
    s[(int)Str::LayoutDrawInterleaved] = "Выклікі чаргуюцца; перакрыцце залежыць ад часткі элемента.";
    s[(int)Str::EmulatorBefore] = "Да (адкрытая прашыўка)";
    s[(int)Str::EmulatorAfter] = "Пасля (бягучыя змены)";
    s[(int)Str::EmulatorSideBySide] = "Побач";
    s[(int)Str::EmulatorOriginalError] = "Памылка прагляду адкрытай прашыўкі: %s";
    s[(int)Str::MenuSaveProject] = "Захаваць праект";
    s[(int)Str::MenuSaveProjectAs] = "Захаваць праект як...";
    s[(int)Str::MsgProjectSaved] = "Праект захаваны: %s";
    s[(int)Str::MenuBuild] = "Экспарт прашыўкі";
    s[(int)Str::BtnOpenFirmware] = "Адкрыць прашыўку або праект...";
    s[(int)Str::StartupOpenFile] = "Адкрыць прашыўку або праект...";
    s[(int)Str::UnsavedPrompt] = "Захаваць праект перад працягам?";
    s[(int)Str::UnsavedPrompt] = "Захаваць праект перад працягам?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Скінуць элемент";
    s[(int)Str::LayoutResetRevision] = "Скінуць размяшчэнне гэтай рэвізіі";
    s[(int)Str::LayoutResetAll] = "Скінуць усе змены размяшчэння";
    s[(int)Str::LayoutPatchSites] = "Адрасы патчаў";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Стан папярэдняга прагляду";
    s[(int)Str::LayoutMoveElement] = "Перамясціць элемент UI";
    s[(int)Str::LayoutRba] = "Рэжым RBA";
    s[(int)Str::LayoutTemperatureControl] = "Тэрмакантроль";
    s[(int)Str::LayoutTemperature] = "Тэмпература (°C)";
    s[(int)Str::LayoutRenderFailed] = "Штатны кадравы буфер недаступны.";
    s[(int)Str::LayoutElementPower] = "Магутнасць";
    s[(int)Str::LayoutElementBattery] = "Батарэя";
    s[(int)Str::LayoutElementResistance] = "Супраціў";
    s[(int)Str::LayoutElementDuration] = "Працягласць";
    s[(int)Str::LayoutElementPuffs] = "Зацяжкі";
    s[(int)Str::LayoutElementMode] = "Рэжым";
    s[(int)Str::LayoutElementStatus] = "Стан";
    s[(int)Str::LayoutElementScore] = "Лік";
    s[(int)Str::LayoutElementStatistics] = "Статыстыка";
    s[(int)Str::LayoutElementVoltage] = "Напружанне";
    s[(int)Str::LayoutElementGraph] = "Графік";
    s[(int)Str::LayoutElementRank] = "Ранг";
    s[(int)Str::LayoutElementModeSelection] = "Выбар рэжыму";
    s[(int)Str::LayoutElementLogo] = "Лагатып";
    s[(int)Str::LayoutChangedElements] = "Зменена элементаў UI";
    s[(int)Str::EmulatorSupported] = "Натыўны прагляд UI ўсіх убудаваных прашывак DRAG.";
    s[(int)Str::EmulatorNativeNote] = "Штатны код малявання з бягучымі зменамі. Электрычная частка не эмулюецца.";
    s[(int)Str::EmulatorWatts] = "Магутнасць (Вт)";
    s[(int)Str::EmulatorPuffs] = "Зацяжкі";
    s[(int)Str::EmulatorBattery] = "Зарад (0–5)";
    s[(int)Str::EmulatorResistance] = "Супраціў (мОм)";
    s[(int)Str::EmulatorDuration] = "Працягласць (0,01 с)";
    s[(int)Str::EmulatorLocked] = "Блакіроўка";
    s[(int)Str::EmulatorCharging] = "Зарадка";
    s[(int)Str::EmulatorScore] = "Усяго балаў";
    s[(int)Str::EmulatorScoreDisplay] = "Лепшы вынік";
    s[(int)Str::EmulatorRedraw] = "Перамаляваць";
    s[(int)Str::EmulatorLandscape] = "Гарызантальна";
    s[(int)Str::EmulatorScreen] = "Экран";
    s[(int)Str::EmulatorMainScreen] = "Асноўны экран";
    s[(int)Str::FontSystemFonts] = "Сістэмныя шрыфты";
    s[(int)Str::FontUserFonts] = "Карыстальніцкія шрыфты";
    s[(int)Str::EmulatorMenuItem] = "Пункт меню";
    s[(int)Str::EmulatorMaterial] = "Матэрыял";
    s[(int)Str::EmulatorMenuConfirmed] = "Пацверджана";
    s[(int)Str::EmulatorResources] = "Рэсурсы, прачытаныя прашыўкай";
    s[(int)Str::EmulatorNoResources] = "Зваротаў да рэсурсаў каталога не зафіксавана.";
    s[(int)Str::EmulatorUidNote] = "Эмуляваны UID MCU, не ID падключанай прылады.";
    s[(int)Str::EmulatorLoaderVersion] = "Версія загрузчыка (эмуляцыя)";
    s[(int)Str::EmulatorLoaderId] = "Байты CHIP-ID (эмуляцыя)";
    s[(int)Str::EmulatorLoaderNote] = "Даныя загрузчыка эмулююцца, а не счытваюцца з прашыўкі ці прылады.";
    s[(int)Str::TabFirmwarePatches] = "Патчы";
    s[(int)Str::PatchBootLogo] = "Экран запуску";
    s[(int)Str::PatchShutdownLogo] = "Экран выключэння";
    s[(int)Str::PatchDeviceInfo] = "Экран CHIP ID / версіі";
    s[(int)Str::PatchDeviceInfoNote] = "Толькі статычная выява. Замяняе сэрвісны экран, але не галоўны экран магутнасці і батарэі.";
    s[(int)Str::PatchLogoNote] = "Незалежныя выявы 80 × 160 для ўсіх рэвізій DRAG S Score / Factory. На прыладзе не праверана.";
    s[(int)Str::PatchUnsupported] = "Для патча патрэбна прашыўка DRAG S Score або Factory.";
    s[(int)Str::PatchEnabled] = "Уключыць свой лагатып";
    s[(int)Str::PatchCanvas] = "Кампазіцыя · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Натыўны прагляд экрана";
    s[(int)Str::PatchPreviewNote] = "Натыўны прагляд паказвае ўжытыя налады, а не чарнавік кампазіцыі.";
    s[(int)Str::PatchFit] = "Упісаць";
    s[(int)Str::PatchCover] = "Запоўніць";
    s[(int)Str::PatchStretch] = "Расцягнуць";
    s[(int)Str::PatchScale] = "Маштаб";
    s[(int)Str::PatchAnimation] = "Анімацыя з’яўлення";
    s[(int)Str::PatchNone] = "Няма";
    s[(int)Str::PatchFade] = "Праяўленне";
    s[(int)Str::PatchDuration] = "Працягласць (мс)";
    s[(int)Str::PatchAutoHardware] = "Аўтаматычна";
    s[(int)Str::PatchAnimationPhase] = "Фаза анімацыі";
    s[(int)Str::PatchPlay] = "Прайграць";
    s[(int)Str::EmulatorModeScreens] = "Экраны";
    s[(int)Str::EmulatorModeDevice] = "Прылада";
    s[(int)Str::DeviceUnsupported] = "Рэжым прылады падтрымлівае DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Прашыўка выконваецца ад Reset з віртуальнай перыферыяй. Нагрэў мадэлюецца; электрабяспека не правяраецца.";
    s[(int)Str::DeviceReset] = "Перазапуск";
    s[(int)Str::DevicePause] = "Паўза";
    s[(int)Str::DeviceResume] = "Пуск";
    s[(int)Str::DeviceStep] = "Крок";
    s[(int)Str::DeviceChanged] = "Бягучыя змены яшчэ не загружаны.";
    s[(int)Str::DeviceApply] = "Перазапусціць са зменамі";
    s[(int)Str::DeviceButtons] = "Утрымлівайце кнопкі або прабел / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Спіраль падключана";
    s[(int)Str::DeviceTime] = "Віртуальны час";
    s[(int)Str::DeviceSleeping] = "Працэсар спіць";
    s[(int)Str::DeviceLoading] = "Загрузка прылады...";
    s[(int)Str::DeviceLog] = "Звароты да перыферыі";
    s[(int)Str::BadgeNoFirmware] = "| [Прашыўка не адкрыта - Перацягніце .zwdf/.xwdf сюды]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu элементаў | %zu запісаў";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Адкрыць прашыўку...";
    s[(int)Str::BtnExportAll] = "Экспартаваць усе рэсурсы ў папку...";
    s[(int)Str::BtnImportFolder] = "Імпартаваць папку з PNG...";
    s[(int)Str::BtnBuildFirmware] = " САБРАЦЬ НОВУЮ ПРАШЫЎКУ ";
    s[(int)Str::BtnRevertAllStaged] = "Скінуць усе праўкі";
    s[(int)Str::GroupAll] = "Усе групы";
    s[(int)Str::GroupAlerts] = "Апавяшчэнні (Дыялогі і значкі)";
    s[(int)Str::GroupFonts] = "Шрыфты (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Лік і рангі";
    s[(int)Str::GroupStatusLabels] = "Статус і надпісы UI";
    s[(int)Str::GroupControls] = "Кіраванне і батарэя";
    s[(int)Str::GroupScreens] = "Экраны і лагатыпы загрузкі";
    s[(int)Str::GroupAnimations] = "Анімацыі";
    s[(int)Str::PanelResourcesHeader] = "Рэсурсы";
    s[(int)Str::LabelSearch] = "Пошук";
    s[(int)Str::SearchPlaceholder] = "фільтр па назве, id, адрасе...";
    s[(int)Str::NoFirmwareLoadedList] = "Прашыўка не загружана.\nФайл > Стварыць / Адкрыць";
    s[(int)Str::NoResourcesMatchFilter] = "Няма рэсурсаў, адпаведных фільтру пошуку.";
    s[(int)Str::PanelResourcePreviewHeader] = "Перадпрагляд";
    s[(int)Str::SelectResourcePrompt] = "Выберыце рэсурс са спісу злева для прагляду і рэдагавання.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Група";
    s[(int)Str::LabelColor] = "Колер";
    s[(int)Str::LabelColorCustom] = "Карыстальніцкі";
    s[(int)Str::LabelFlashAddress] = "Адрас Flash";
    s[(int)Str::LabelDimensions] = "Памеры";
    s[(int)Str::LabelBytes] = "Байты";
    s[(int)Str::LabelRecords] = "Запісы";
    s[(int)Str::StatusModifiedStaged] = "Статус: ЗМЕНЕНА (падрыхтавана ў памяці)";
    s[(int)Str::StatusOriginal] = "Статус: АРЫГІНАЛ (з прашыўкі)";
    s[(int)Str::StatusStockOriginal] = "Статус: ЗАВОДСКІ АРЫГІНАЛ (адпавядае эталону)";
    s[(int)Str::StatusCustomDiffers] = "Статус: КАСТАМНЫ / ЗМЕНЕНЫ (адрозніваецца ад заводскага)";
    s[(int)Str::BtnRestoreStock] = "Аднавіць заводскі арыгінал";
    s[(int)Str::LabelCompareWithStock] = "Параўнанне з заводскай эталоннай прашыўкай:";
    s[(int)Str::LabelActivePreview] = "Бягучы актыўны:";
    s[(int)Str::LabelStockReferencePreview] = "Заводскі эталон:";
    s[(int)Str::FilterOnlyCustom] = "Паказваць толькі змененыя / кастамныя рэсурсы";
    s[(int)Str::MsgRestoredToStock] = "Адноўлена %s да заводскага эталону.";
    s[(int)Str::BtnReplaceFromPng] = "Замяніць з PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Аднавіць арыгінал";
    s[(int)Str::BtnExportThisPng] = "Экспартаваць гэты PNG...";
    s[(int)Str::BtnExportHorizontal] = "Экспартаваць гарызантальна (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Каляровая палітра і рэндэрэр (RGB565)";
    s[(int)Str::LabelTheme] = "Тэма";
    s[(int)Str::BtnApply] = "Ужыць";
    s[(int)Str::BtnResetAll] = "Скінуць усё";
    s[(int)Str::LabelRole] = "Роля";
    s[(int)Str::BtnResetRole] = "Скінуць ролю";
    s[(int)Str::PromptLoadFwColors] = "Загрузіце прашыўку для прагляду і змены колераў.";
    s[(int)Str::LabelPreviewTint] = "Адценне перадпрагляду";
    s[(int)Str::InfoDigitsMonochrome] = "Лічбы з'яўляюцца манахромнымі маскамі. Іх колер задаецца кодам прашыўкі, а не захоўваецца ў гліфе.";
    s[(int)Str::InfoPreviewNotSaved] = "Адценне перадпрагляду не захоўваецца ў прашыўку. Экспарт у RGB565 PNG уключае гэтае адценне.";
    s[(int)Str::LabelPreviewOnly] = "Толькі перадпрагляд:";
    s[(int)Str::LabelPresets] = "Прэсеты...";
    s[(int)Str::BtnResetColor] = "Скінуць колер";
    s[(int)Str::LabelZoom] = "Маштаб перадпрагляду";
    s[(int)Str::LabelPixelGrid] = "Сетка пікселяў";
    s[(int)Str::LabelHorizontalPreview] = "Гарызантальны перадпрагляд для чытання (136x22):";
    s[(int)Str::FontSec1Title] = "1. Выбар шрыфта";
    s[(int)Str::FontSec1WindowsFont] = "Шрыфт Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Агляд файла TTF/OTF...";
    s[(int)Str::FontSec1ActiveFont] = "Актыўны шрыфт: %s";
    s[(int)Str::FontSec2Title] = "2. Прэсет адлюстравання";
    s[(int)Str::FontSec3Title] = "3. Параметры рэндэрынгу";
    s[(int)Str::FontSec3Threshold] = "Парог яркасці";
    s[(int)Str::FontSec3ThresholdHint] = "Менш = паўтлусты/тоўсты, Больш = тонкі";
    s[(int)Str::FontSec3Stretch] = "Расцягнуць па памеры вочка";
    s[(int)Str::FontSec3Baseline] = "Выраўноўванне па базавай лініі";
    s[(int)Str::FontSec3CenterV] = "Цэнтраваць па вертыкалі";
    s[(int)Str::FontSec3PaddingX] = "Водступ X";
    s[(int)Str::FontSec3PaddingY] = "Водступ Y";
    s[(int)Str::FontSec3Oversampling] = "Суперсэмплінг";
    s[(int)Str::FontSec4Title] = "4. Інтэграцыя ў прашыўку";
    s[(int)Str::FontSec4ApplyPreset] = " > УЖЫЦЬ ГЛІФЫ ДА АДКРЫТАЙ ПРАШЫЎКІ < ";
    s[(int)Str::FontSec4ApplyAll] = "Ужыць усе шрыфты";
    s[(int)Str::FontSec5Title] = "5. Экспарт у PNG";
    s[(int)Str::FontSec5OutputFolder] = "Папка для экспарту";
    s[(int)Str::FontSec5BrowseFolder] = "Агляд папкі...";
    s[(int)Str::FontSec5OpenExplorer] = "Адкрыць у правадніку";
    s[(int)Str::FontSec5ExportAllPreset] = "Экспарт усіх прэсетаў";
    s[(int)Str::FontSec5ExportButton] = "Экспартаваць 1-бітныя PNG на дыск";
    s[(int)Str::FontRightLivePreview] = "Жывы перадпрагляд шрыфта";
    s[(int)Str::FontRightNoGlyphs] = "Гліфы не загружаны.";
    s[(int)Str::FontPresetInfo] = "Прэсет: %s (%dx%d пкс, %d гліфаў, памер: %.1f пкс)";
    s[(int)Str::FwInfoSpecTitle] = "Спецыфікацыя пакета VOOPOO DRAG S / DRAG X / DRAG 2";
    s[(int)Str::FwInfoNoFirmware] = "Прашыўка не загружана. Адкрыйце файл .zwdf/.xwdf на ўкладцы 'Студыя рэсурсаў'.";
    s[(int)Str::FwInfoFilePath] = "Шлях да файла: %s";
    s[(int)Str::FwInfoSha256] = "Зыходны SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Памер пакета: %zu байт";
    s[(int)Str::FwInfoHwRevisions] = "Рэвізіі апаратнага забеспячэння: %zu запісаў";
    s[(int)Str::FwInfoRecognizedResources] = "Распазнаныя рэсурсы прашыўкі: %zu (шрыфты, значкі, рангі, надпісы, кнопкі, экраны)";
    s[(int)Str::FwInfoRecordsTable] = "Табліца апаратных запісаў";
    s[(int)Str::FwInfoColRecord] = "Запіс #";
    s[(int)Str::FwInfoColDevCode] = "Код прылады";
    s[(int)Str::FwInfoColPayload] = "Памер нагрузкі";
    s[(int)Str::FwInfoColImage] = "Памер вобраза";
    s[(int)Str::FwInfoColFlashBase] = "Базавы адрас Flash";
    s[(int)Str::FwInfoColMSP] = "Паказальнік стэка (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Вектар скіду";
    s[(int)Str::BtnRevertAllModifications] = "Скінуць усе змены";
    s[(int)Str::MsgReadyOpenFw] = "Гатова. Адкрыйце файл .zwdf/.xwdf або згенеруйце карыстальніцкія шрыфты.";
    s[(int)Str::MsgSuccessOpened] = "Адкрыта: %s";
    s[(int)Str::MsgOpenError] = "Памылка адкрыцця: %s";
    s[(int)Str::MsgAllReverted] = "Усе падрыхтаваныя змены скінуты да зыходнага стану прашыўкі.";
    s[(int)Str::MsgExportSuccess] = "Экспартавана %d рэсурсаў у: %s";
    s[(int)Str::MsgExportError] = "Памылка экспарту: %s";
    s[(int)Str::MsgImportSuccess] = "Імпартавана %d PNG з папкі.";
    s[(int)Str::MsgImportError] = "Памылка імпарту: %s";
    s[(int)Str::MsgBuildSuccess] = "Зборка ПАСПЯХОВАЯ: %s";
    s[(int)Str::MsgBuildError] = "Памылка зборкі: %s";
    s[(int)Str::MsgFontNotLoaded] = "Памылка: Шрыфт не загружаны ў генератар!";
    s[(int)Str::MsgFontInjectedPreset] = "Напрамую ўкаранёна %d гліфаў памеру %s у памяць прашыўкі!";
    s[(int)Str::MsgFontInjectedAll] = "Ужыта гліфаў: %d.";
    s[(int)Str::MsgFontExportSuccess] = "Паспяхова экспартавана %d файлаў PNG у: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "Паспяхова экспартавана %d файлаў для %s у: %s";
    s[(int)Str::MsgFontExportError] = "Памылка экспарту: %s";
    s[(int)Str::ModalBuildReportTitle] = "Справаздача аб зборцы";
    s[(int)Str::ModalBuildSuccessBanner] = "ЗБОРКА І ПРАВЕРКА ПРАШЫЎКІ ПАСПЯХОВЫЯ!";
    s[(int)Str::ModalSavedFile] = "Захаваны файл: %s";
    s[(int)Str::ModalOutputSize] = "Выніковы памер: %zu байт";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Змененых растравых рэсурсаў: %zu";
    s[(int)Str::ModalModColorGroups] = "Змененых каляровых груп: %zu";
    s[(int)Str::ModalPatchedRanges] = "Прапатчаных дыяпазонаў байт: %zu у %zu апаратных запісах";
    s[(int)Str::ModalUntouchedBytes] = "Правераных нязмененых байт: %zu (ідэнтычныя арыгіналу)";
    s[(int)Str::ModalReportNote] = "Падрабязная справаздача захавана як .report.json побач з прашыўкай.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Адкрыць у правадніку";
    s[(int)Str::BtnCopyClipboard] = "Капіяваць выяву";
    s[(int)Str::BtnPasteClipboard] = "Уставіць выяву";
    s[(int)Str::BtnUndo] = "Адмяніць";
    s[(int)Str::BtnRedo] = "Паўтарыць";
    s[(int)Str::TooltipUndo] = "Адмяніць: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Паўтарыць: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Няма чаго адмяняць";
    s[(int)Str::TooltipRedoEmpty] = "Няма чаго паўтараць";
    s[(int)Str::LabelPixelTools] = "Інструменты пікселяў:";
    s[(int)Str::ToolPencil] = "Аловак";
    s[(int)Str::ToolEraser] = "Ласцік";
    s[(int)Str::BtnInvertPixels] = "Інвертаваць";
    s[(int)Str::BtnClearPixels] = "Ачысціць";
    s[(int)Str::BtnFlipH] = "Адлюстраваць Г";
    s[(int)Str::BtnFlipV] = "Адлюстраваць В";
    s[(int)Str::MsgCopiedToClipboard] = "Выява %s скапіявана ў буфер абмену.";
    s[(int)Str::MsgPastedFromClipboard] = "Выява з буфера абмену ўстаўлена для %s!";
    s[(int)Str::MsgClipboardNoImage] = "У буферы абмену няма выявы.";
    s[(int)Str::MsgUndoPerformed] = "Адменена: %s";
    s[(int)Str::MsgRedoPerformed] = "Паўторана: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu запісаў | %zu правак";
    s[(int)Str::BadgeProjectSummary] = "| Праект: %s (%s) | %zu запісаў | %zu правак";
    s[(int)Str::LabelCategory] = "Катэгорыя";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Група: %s  |  Колер: Карыстальніцкі (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Група: %s  |  Колер: %s";
    s[(int)Str::LabelMetaHardware] = "Адрас Flash: 0x%08X  |  Памеры: %u x %u пкс  |  Байты: %zu  |  Запісы: %zu";
    s[(int)Str::LabelPixelCoord] = "Піксель: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| ЛКМ: %s  |  ПКМ: Сцерці";
    s[(int)Str::LabelDrawAction] = "Маляваць";
    s[(int)Str::LabelEraseAction] = "Сцерці";
    s[(int)Str::LabelFontPreset] = "Прэсет";
    s[(int)Str::LabelFontPixelGrid] = "Сетка пікселяў";
    s[(int)Str::MsgAppliedColorTheme] = "Ужыта каляровая тэма '%s' для ўсіх адпаведных рэсурсаў.";
    s[(int)Str::MsgImportedMatchingPngs] = "Імпартавана %zu адпаведных PNG з папкі.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Паспяхова адкрыта прашыўка праз Drag & Drop: %s";
    s[(int)Str::MsgDragDropError] = "Памылка Drag & Drop: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Загружаны шрыфт праз Drag & Drop: %s";
    s[(int)Str::MsgFailedLoadFont] = "Не ўдалося загрузіць файл шрыфта: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Абраны PNG для %s супадае з арыгіналам (без змен).";
    s[(int)Str::MsgStagedReplacement] = "Падрыхтавана замена PNG для %s!";
    s[(int)Str::MsgPngLoadError] = "Памылка загрузкі PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Заменены рэсурс %s супадае з арыгіналам (без змен).";
    s[(int)Str::MsgStagedReplacementCustom] = "Падрыхтавана карыстальніцкая замена PNG для %s!";
    s[(int)Str::MsgStagedRgbReplacement] = "Падрыхтавана карыстальніцкая замена RGB565 PNG для %s!";
    s[(int)Str::MsgRestoredOriginal] = "Адноўлены арыгінальны рэсурс прашыўкі для %s.";
    s[(int)Str::MsgSavedAssetPng] = "Захавана %s як %s";
    s[(int)Str::MsgSaveError] = "Памылка захавання: %s";
    s[(int)Str::MsgSavedBootLogo] = "Гарызантальны лагатып захаваны як %s";
    s[(int)Str::MsgRefuseOverwrite] = "Забаронена перазапісваць загружаны файл. Калі ласка, абярыце іншы шлях.";
    s[(int)Str::MsgColorPatchError] = "Памылка палітры колераў: %s";
    s[(int)Str::MsgPackError] = "Памылка пакавання: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Імпартавана %zu змененых PNG (%zu супала з арыгіналам).";
    s[(int)Str::LabelBuiltin] = "убудаваная";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Не ўдалося загрузіць убудаваную прашыўку: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Загружана ўбудаваная прашыўка: %s";
    s[(int)Str::ThemeFactoryDefault] = "Заводская па змаўчанні";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Арыгінальны заводскі стыль Voopoo (чырвоны і жоўты)";
    s[(int)Str::ThemeCyberpunkNeon] = "Кіберпанк неон";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Электрычны цыян, неонавы ружовы і фіялетавы";
    s[(int)Str::ThemeMatrixGreen] = "Матрыца зялёны";
    s[(int)Str::ThemeMatrixGreenDesc] = "ЭЛТ-тэрмінал фосфар і смарагдавы зялёны";
    s[(int)Str::ThemeArcticIceBlue] = "Арктычны лёд";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Ледавіковы цыян, глыбокае неба і блакітнае ззянне";
    s[(int)Str::ThemeAmberFire] = "Бурштынавы агонь";
    s[(int)Str::ThemeAmberFireDesc] = "Лававы аранжавы, залатое полымя і вугаль";
    s[(int)Str::ThemePurpleNebula] = "Пурпурная туманнасць";
    s[(int)Str::ThemePurpleNebulaDesc] = "Глыбокі фіялетавы, лаванда і неон аметыст";
    s[(int)Str::ThemeStealthDarkGray] = "Стэлус цёмна-шэры";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Матавы графіт, сланцы і зброевы метал";
    s[(int)Str::RoleAccentRed] = "Акцэнт / чырвоны рэжым";
    s[(int)Str::RoleAccentRedScope] = "Асноўны акцэнт UI у стандартных рэжымах — 8 месцаў";
    s[(int)Str::RoleAccentCyan] = "Акцэнт / цыян рэжым";
    s[(int)Str::RoleAccentCyanScope] = "Асноўны акцэнт UI у рэжымах SMART/RBA — 7 месцаў";
    s[(int)Str::RoleHighlightYellow] = "Падсветка / жоўты";
    s[(int)Str::RoleHighlightYellowScope] = "Паласа ачкоў і сэрвісныя панэлі — 4 месцы";
    s[(int)Str::RoleRankBronze] = "Ранг / бронза";
    s[(int)Str::RoleRankBronzeScope] = "Палітра рангу Бронза";
    s[(int)Str::RoleRankGold] = "Ранг / золата";
    s[(int)Str::RoleRankGoldScope] = "Палітра рангу Золата";
    s[(int)Str::RoleRankIce] = "Ранг / плаціна (лёд)";
    s[(int)Str::RoleRankIceScope] = "Палітра рангу Плаціна / Лёд";
    s[(int)Str::RoleRankCyan] = "Ранг / цыян";
    s[(int)Str::RoleRankCyanScope] = "Палітра рангу Цыян";
    s[(int)Str::RoleRankOrange] = "Ранг / аранжавы";
    s[(int)Str::RoleRankOrangeScope] = "Палітра рангу Аранжавы";
    s[(int)Str::RoleRankRed] = "Ранг / чырвоны";
    s[(int)Str::RoleRankRedScope] = "Палітра рангу Чырвоны";
    s[(int)Str::RoleDigitsWattage] = "Лічбы магутнасці";
    s[(int)Str::RoleDigitsWattageScope] = "Вялікія лічбы магутнасці на галоўным экране (19x40)";
    s[(int)Str::RoleDigitsStats] = "Лічбы статыстыкі";
    s[(int)Str::ColorPresetWhite] = "Белы (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Асноўны / Чырвоны (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Цыян (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Сэрвісны / Жоўты (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Бронза (#B57947)";
    s[(int)Str::ColorPresetGold] = "Золата (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Аранжавы (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Зялёны (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Асноўныя лічбы 0-9, 10 файлаў)";
    s[(int)Str::FontPreset11x20] = "11x20 (Другасныя лічбы 0-9, 10 файлаў)";
    s[(int)Str::FontPreset7x13] = "7x13 (Малыя статусныя 0-9, A-F, 16 файлаў)";
    s[(int)Str::FontPreset6x10] = "6x10 (Мікра статус 0-9, 10 файлаў)";
    s[(int)Str::FontOs1x] = "1x (Прамы)";
    s[(int)Str::FontOs2x] = "2x (Хуткі)";
    s[(int)Str::FontOs4x] = "4x (Рэкамендаваны)";
    s[(int)Str::FontOs8x] = "8x (Ультра)";
}

static void InitUzbek() {
    auto& s = g_table[(int)Language::Uzbek];
    s[(int)Str::SupportFull] = "To‘liq";
    s[(int)Str::SupportNone] = "Qo‘llab-quvvatlanmaydi";
    s[(int)Str::SupportPalette] = "Ranglar palitrasi";
    s[(int)Str::SupportMonochrome] = "Monoxrom ekran; ranglar palitrasi yo‘q.";
    s[(int)Str::EmulatorHistoryDay] = "Tarix kuni";
    s[(int)Str::EmulatorClockHour] = "Soat";
    s[(int)Str::EmulatorClockMinute] = "Daqiqa";
    s[(int)Str::EmulatorClockField] = "Tanlangan maydon";
    s[(int)Str::MenuFile] = "Fayl";
    s[(int)Str::MenuEdit] = "Tahrirlash";
    s[(int)Str::MenuLanguage] = "Til";
    s[(int)Str::MenuHelp] = "Yordam";
    s[(int)Str::UpdateTitle] = "Studiya yangilanishlari";
    s[(int)Str::UpdateVersion] = "Oʻrnatilgan versiya: %s";
    s[(int)Str::UpdateCheck] = "Yangilanishlarni tekshirish";
    s[(int)Str::UpdateChecking] = "GitHub Releases tekshirilmoqda...";
    s[(int)Str::UpdateCurrent] = "Eng yangi versiya oʻrnatilgan.";
    s[(int)Str::UpdateDownload] = "Yuklab olish";
    s[(int)Str::UpdateDownloading] = "Yuklanmoqda va tekshirilmoqda...";
    s[(int)Str::UpdateInstall] = "Oʻrnatish va qayta ishga tushirish";
    s[(int)Str::UpdateReady] = "Oʻrnatishga tayyor.";
    s[(int)Str::UpdateChangelog] = "Oʻzgarishlar";
    s[(int)Str::UpdateFailed] = "Yangilash xatosi";
    s[(int)Str::UpdateAvailable] = "Mavjud versiya: %s";
    s[(int)Str::MenuCreate] = "Yaratish...";
    s[(int)Str::MenuOpen] = "Ochish...";
    s[(int)Str::MenuImport] = "Resurslarni import qilish...";
    s[(int)Str::MenuExport] = "Resurslarni eksport qilish...";
    s[(int)Str::MenuBuild] = "Mikrodasturni yig‘ish...";
    s[(int)Str::MenuExit] = "Chiqish";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Resurslar";
    s[(int)Str::TabFontGenerator] = "Shrift generatori";
    s[(int)Str::TabFirmwareInfo] = "Mikrodastur haqida";
    s[(int)Str::TabFirmwareComparison] = "Taqqoslash";
    s[(int)Str::CompareModeTwoFiles] = "Ikki fayl";
    s[(int)Str::CompareModeEditor] = "Muharrir va fayl";
    s[(int)Str::CompareLeft] = "Chap";
    s[(int)Str::CompareRight] = "Oʻng";
    s[(int)Str::CompareChooseFile] = "Fayl tanlash...";
    s[(int)Str::CompareReadOnlyNote] = "Faqat koʻrish; muharrirdagi oʻzgarishlar hisobga olinadi.";
    s[(int)Str::CompareChooseSources] = "Taqqoslash uchun ikkala manbani tanlang.";
    s[(int)Str::CompareEditorUnavailable] = "Muharrirda mikrodastur ochilmagan.";
    s[(int)Str::CompareChanged] = "Oʻzgargan";
    s[(int)Str::CompareAdded] = "Qoʻshilgan";
    s[(int)Str::CompareRemoved] = "Oʻchirilgan";
    s[(int)Str::CompareEqual] = "Bir xil";
    s[(int)Str::CompareIncompatible] = "Mos emas";
    s[(int)Str::CompareDecodeError] = "Dekodlash xatosi";
    s[(int)Str::CompareStatusFilter] = "Holat";
    s[(int)Str::CompareDifferencePreview] = "Farq";
    s[(int)Str::CompareHighlightPixels] = "Farqlarni ajratish";
    s[(int)Str::CompareDifferentPixels] = "Farqli piksellar: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Resurs yoʻq";
    s[(int)Str::CompareIncompatibleHint] = "Resurs turlari yoki oʻlchamlari turlicha: piksellarni taqqoslab boʻlmaydi.";
    s[(int)Str::CompareImageUnavailable] = "Tasvir mavjud emas";
    s[(int)Str::CompareTextureError] = "Koʻrish teksturasini yaratib boʻlmadi.";
    s[(int)Str::CompareAllStatuses] = "Barcha holatlar";
    s[(int)Str::TabFirmwareEmulator] = "Emulyator";
    s[(int)Str::TabFirmwareLayout] = "UI joylashuvi";
    s[(int)Str::LayoutUnsupported] = "Bu mikrodastur uchun UI joylashuvini tahrirlash mavjud emas.";
    s[(int)Str::LayoutNativeNote] = "Elementni sudrang yoki X/Y kiriting. Koʻrish oynasi oʻzgartirilgan mikrodasturni bajaradi; oʻzgarishlar tanlangan apparat reviziyasiga tegishli.";
    s[(int)Str::LayoutRevision] = "Apparat reviziyasi";
    s[(int)Str::LayoutCheckingRevisions] = "Bir xil interfeyslar tekshirilmoqda…";
    s[(int)Str::LayoutAllRevisions] = "Barcha reviziyalar";
    s[(int)Str::LayoutRevisionGroup] = "Bir xil UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Reviziya: %s";
    s[(int)Str::LayoutResetScope] = "Tanlangan reviziyalarni tiklash";
    s[(int)Str::LayoutSelectionUnavailable] = "Tanlangan reviziyalarning barchasida mos element yoʻq.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI elementlari";
    s[(int)Str::LayoutNoElements] = "Bu ekranda siljitiladigan bloklar yo‘q.";
    s[(int)Str::LayoutSharedBlock] = "Bu blok koordinatalari bir nechta ekran uchun umumiy.";
    s[(int)Str::LayoutPositionX] = "X (piksel)";
    s[(int)Str::LayoutPositionY] = "Y (piksel)";
    s[(int)Str::LayoutOffset] = "Siljish: %+d, %+d piksel";
    s[(int)Str::LayoutGridStep] = "Qadam: %d × %d piksel";
    s[(int)Str::LayoutGridSpacing] = "Toʻr qadami (piksel)";
    s[(int)Str::LayoutGridConstraint] = "Toʻr nuqtasini ifodalab boʻlmasa, asl koordinata qadami va chegaralari ustun turadi.";
    s[(int)Str::LayoutSnapGrid] = "Toʻrga biriktirish";
    s[(int)Str::LayoutSnapElements] = "Element chetlari va markazlariga biriktirish";
    s[(int)Str::LayoutGuides] = "Harakat yoʻriqnomalari";
    s[(int)Str::LayoutAlign] = "Ekranga tekislash";
    s[(int)Str::LayoutAlignLeft] = "Chap";
    s[(int)Str::LayoutAlignCenter] = "X markaz";
    s[(int)Str::LayoutAlignRight] = "Oʻng";
    s[(int)Str::LayoutAlignTop] = "Yuqori";
    s[(int)Str::LayoutAlignMiddle] = "Y markaz";
    s[(int)Str::LayoutAlignBottom] = "Past";
    s[(int)Str::LayoutNudgeHint] = "Strelkalar: asl piksel qadami; Shift: kattaroq qadam. Fokus uchun ekranni bosing.";
    s[(int)Str::LayoutOverlays] = "Element chegaralari";
    s[(int)Str::LayoutOverlapOverlay] = "Kesishuvlarni ajratish";
    s[(int)Str::LayoutOverlaps] = "Kesishuvchi elementlar";
    s[(int)Str::LayoutNoOverlaps] = "Toʻrtburchaklar kesishmaydi";
    s[(int)Str::LayoutDrawOrderUnknown] = "Asl chizish tartibi tasdiqlanmagan";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) %s (#%d) dan keyin chiziladi";
    s[(int)Str::LayoutDrawOrderNote] = "Bu doimiy qatlamlar emas, asosiy chizish chaqiruvlari tartibi. Yangilanishlarda u farq qilishi mumkin; avvalgi tarkibni faqat yozilgan piksellar qoplaydi.";
    s[(int)Str::LayoutDrawSequence] = "Asl chizish tartibi";
    s[(int)Str::LayoutDrawInterleaved] = "Chaqiruvlar almashadi; qoplanish element qismiga bogʻliq.";
    s[(int)Str::EmulatorBefore] = "Oldin (ochilgan mikrodastur)";
    s[(int)Str::EmulatorAfter] = "Keyin (joriy oʻzgarishlar)";
    s[(int)Str::EmulatorSideBySide] = "Yonma-yon";
    s[(int)Str::EmulatorOriginalError] = "Ochilgan mikrodastur koʻrinishi xatosi: %s";
    s[(int)Str::MenuSaveProject] = "Loyihani saqlash";
    s[(int)Str::MenuSaveProjectAs] = "Loyihani boshqacha saqlash...";
    s[(int)Str::MsgProjectSaved] = "Loyiha saqlandi: %s";
    s[(int)Str::MenuBuild] = "Mikrodasturni eksport qilish";
    s[(int)Str::BtnOpenFirmware] = "Mikrodastur yoki loyihani ochish...";
    s[(int)Str::StartupOpenFile] = "Mikrodastur yoki loyihani ochish...";
    s[(int)Str::UnsavedPrompt] = "Davom etishdan oldin loyihani saqlaysizmi?";
    s[(int)Str::UnsavedPrompt] = "Davom etishdan oldin loyihani saqlaysizmi?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Elementni tiklash";
    s[(int)Str::LayoutResetRevision] = "Bu reviziya joylashuvini tiklash";
    s[(int)Str::LayoutResetAll] = "Barcha joylashuv oʻzgarishlarini tiklash";
    s[(int)Str::LayoutPatchSites] = "Yamoq manzillari";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Koʻrish holati";
    s[(int)Str::LayoutMoveElement] = "UI elementini koʻchirish";
    s[(int)Str::LayoutRba] = "RBA rejimi";
    s[(int)Str::LayoutTemperatureControl] = "Harorat nazorati";
    s[(int)Str::LayoutTemperature] = "Harorat (°C)";
    s[(int)Str::LayoutRenderFailed] = "Asl kadr buferi mavjud emas.";
    s[(int)Str::LayoutElementPower] = "Quvvat";
    s[(int)Str::LayoutElementBattery] = "Batareya";
    s[(int)Str::LayoutElementResistance] = "Qarshilik";
    s[(int)Str::LayoutElementDuration] = "Davomiylik";
    s[(int)Str::LayoutElementPuffs] = "Tortishlar";
    s[(int)Str::LayoutElementMode] = "Rejim";
    s[(int)Str::LayoutElementStatus] = "Holat";
    s[(int)Str::LayoutElementScore] = "Ball";
    s[(int)Str::LayoutElementStatistics] = "Statistika";
    s[(int)Str::LayoutElementVoltage] = "Kuchlanish";
    s[(int)Str::LayoutElementGraph] = "Grafik";
    s[(int)Str::LayoutElementRank] = "Daraja";
    s[(int)Str::LayoutElementModeSelection] = "Rejim tanlash";
    s[(int)Str::LayoutElementLogo] = "Logotip";
    s[(int)Str::LayoutChangedElements] = "Oʻzgartirilgan UI elementlari";
    s[(int)Str::EmulatorSupported] = "Barcha o‘rnatilgan DRAG mikrodasturlari interfeysining asl ko‘rinishi.";
    s[(int)Str::EmulatorNativeNote] = "Joriy oʻzgarishlar bilan asl chizish kodi. Elektr qismi emulyatsiya qilinmaydi.";
    s[(int)Str::EmulatorWatts] = "Quvvat (W)";
    s[(int)Str::EmulatorPuffs] = "Tortishlar";
    s[(int)Str::EmulatorBattery] = "Zaryad (0–5)";
    s[(int)Str::EmulatorResistance] = "Qarshilik (mΩ)";
    s[(int)Str::EmulatorDuration] = "Davomiylik (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Qulflangan";
    s[(int)Str::EmulatorCharging] = "Zaryadlash";
    s[(int)Str::EmulatorScore] = "Jami ball";
    s[(int)Str::EmulatorScoreDisplay] = "Eng yaxshi natija";
    s[(int)Str::EmulatorRedraw] = "Qayta chizish";
    s[(int)Str::EmulatorLandscape] = "Gorizontal";
    s[(int)Str::EmulatorScreen] = "Ekran";
    s[(int)Str::EmulatorMainScreen] = "Asosiy ekran";
    s[(int)Str::FontSystemFonts] = "Tizim shriftlari";
    s[(int)Str::FontUserFonts] = "Foydalanuvchi shriftlari";
    s[(int)Str::EmulatorMenuItem] = "Menyu bandi";
    s[(int)Str::EmulatorMaterial] = "Material";
    s[(int)Str::EmulatorMenuConfirmed] = "Tasdiqlangan";
    s[(int)Str::EmulatorResources] = "Mikrodastur oʻqigan resurslar";
    s[(int)Str::EmulatorNoResources] = "Katalog resurslari oʻqilishi qayd etilmadi.";
    s[(int)Str::EmulatorUidNote] = "Emulyatsiyadagi MCU UID, ulangan qurilmaning ID raqami emas.";
    s[(int)Str::EmulatorLoaderVersion] = "Virtual yuklagich versiyasi";
    s[(int)Str::EmulatorLoaderId] = "Virtual CHIP-ID baytlari";
    s[(int)Str::EmulatorLoaderNote] = "Yuklagich ma’lumotlari emulyatsiya qilinadi, mikrodastur yoki qurilmadan o‘qilmaydi.";
    s[(int)Str::TabFirmwarePatches] = "Yamoqlar";
    s[(int)Str::PatchBootLogo] = "Ishga tushirish ekrani";
    s[(int)Str::PatchShutdownLogo] = "O‘chirish ekrani";
    s[(int)Str::PatchDeviceInfo] = "CHIP ID / versiya ekrani";
    s[(int)Str::PatchDeviceInfoNote] = "Faqat statik rasm. Xizmat ekranini almashtiradi, asosiy quvvat va batareya ekranini emas.";
    s[(int)Str::PatchLogoNote] = "Barcha DRAG S Score / Factory tahrirlari uchun mustaqil 80 × 160 rasmlar. Qurilmada tekshirilmagan.";
    s[(int)Str::PatchUnsupported] = "Bu yamoq DRAG S Score yoki Factory mikrodasturini talab qiladi.";
    s[(int)Str::PatchEnabled] = "Shaxsiy logotipni yoqish";
    s[(int)Str::PatchCanvas] = "Kompozitsiya · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Ekranning nativ ko‘rinishi";
    s[(int)Str::PatchPreviewNote] = "Nativ ko‘rinish kompozitsiya qoralamasini emas, qo‘llangan sozlamalarni ko‘rsatadi.";
    s[(int)Str::PatchFit] = "Sig‘dirish";
    s[(int)Str::PatchCover] = "To‘ldirish";
    s[(int)Str::PatchStretch] = "Cho‘zish";
    s[(int)Str::PatchScale] = "Masshtab";
    s[(int)Str::PatchAnimation] = "Kirish animatsiyasi";
    s[(int)Str::PatchNone] = "Yo‘q";
    s[(int)Str::PatchFade] = "Asta paydo bo‘lish";
    s[(int)Str::PatchDuration] = "Davomiylik (ms)";
    s[(int)Str::PatchAutoHardware] = "Avtomatik";
    s[(int)Str::PatchAnimationPhase] = "Animatsiya bosqichi";
    s[(int)Str::PatchPlay] = "Ijro etish";
    s[(int)Str::EmulatorModeScreens] = "Ekranlar";
    s[(int)Str::EmulatorModeDevice] = "Qurilma";
    s[(int)Str::DeviceUnsupported] = "Qurilma rejimi DRAG S Score / HW 3103 ni qo‘llaydi.";
    s[(int)Str::DeviceNote] = "Mikrodastur virtual periferiya bilan Reset dan bajariladi. Qizish modellashtiriladi; elektr xavfsizligi tekshirilmaydi.";
    s[(int)Str::DeviceReset] = "Qayta ishga tushirish";
    s[(int)Str::DevicePause] = "Pauza";
    s[(int)Str::DeviceResume] = "Ishga tushirish";
    s[(int)Str::DeviceStep] = "Qadam";
    s[(int)Str::DeviceChanged] = "Joriy o‘zgarishlar hali yuklanmagan.";
    s[(int)Str::DeviceApply] = "O‘zgarishlar bilan qayta boshlash";
    s[(int)Str::DeviceButtons] = "Tugmalar yoki bo‘sh joy / ↑ / ↓ ni ushlang.";
    s[(int)Str::DeviceAtomizerConnected] = "Spiral ulangan";
    s[(int)Str::DeviceTime] = "Virtual vaqt";
    s[(int)Str::DeviceSleeping] = "Protsessor uyquda";
    s[(int)Str::DeviceLoading] = "Qurilma yuklanmoqda...";
    s[(int)Str::DeviceLog] = "Periferiya murojaatlari";
    s[(int)Str::BadgeNoFirmware] = "| [Mikrodastur ochilmagan - .zwdf/.xwdf faylini bu yerga tashlang]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu ta element | %zu ta yozuv";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Mikrodasturni ochish...";
    s[(int)Str::BtnExportAll] = "Barcha resurslarni jildga eksport qilish...";
    s[(int)Str::BtnImportFolder] = "PNG jildini import qilish...";
    s[(int)Str::BtnBuildFirmware] = " YANGI MIKRODASTUR YARATISH ";
    s[(int)Str::BtnRevertAllStaged] = "Barcha oʻzgarishlarni bekor qilish";
    s[(int)Str::GroupAll] = "Barcha guruhlar";
    s[(int)Str::GroupAlerts] = "Bildirishnomalar (Dialoglar va belgilar)";
    s[(int)Str::GroupFonts] = "Shriftlar (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Hisob va darajalar";
    s[(int)Str::GroupStatusLabels] = "Holat va UI yozuvlari";
    s[(int)Str::GroupControls] = "Boshqaruv va batareya";
    s[(int)Str::GroupScreens] = "Ekranlar va yuklash logotiplari";
    s[(int)Str::GroupAnimations] = "Animatsiyalar";
    s[(int)Str::PanelResourcesHeader] = "Resurslar";
    s[(int)Str::LabelSearch] = "Qidiruv";
    s[(int)Str::SearchPlaceholder] = "nomi, id, manzili boʻyicha...";
    s[(int)Str::NoFirmwareLoadedList] = "Mikrodastur yuklanmagan.\nFayl > Yaratish / Ochish";
    s[(int)Str::NoResourcesMatchFilter] = "Qidiruv filtriga mos resurslar topilmadi.";
    s[(int)Str::PanelResourcePreviewHeader] = "Koʻrib chiqish";
    s[(int)Str::SelectResourcePrompt] = "Koʻrish va tahrirlash uchun chapdagi roʻyxatdan resursni tanlang.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Guruh";
    s[(int)Str::LabelColor] = "Rang";
    s[(int)Str::LabelColorCustom] = "Maxsus";
    s[(int)Str::LabelFlashAddress] = "Flash manzili";
    s[(int)Str::LabelDimensions] = "Oʻlchamlari";
    s[(int)Str::LabelBytes] = "Baytlar";
    s[(int)Str::LabelRecords] = "Yozuvlar";
    s[(int)Str::StatusModifiedStaged] = "Holat: OʻZGARTIRILGAN (xotirada tayyorlangan)";
    s[(int)Str::StatusOriginal] = "Holat: ASL NUSXA (mikrodasturdan)";
    s[(int)Str::StatusStockOriginal] = "Holat: ZAVOD ASLI (A050/E050 etaloniga mos)";
    s[(int)Str::StatusCustomDiffers] = "Holat: OʻZGARTIRILGAN (zavod standarti emas)";
    s[(int)Str::BtnRestoreStock] = "Zavod holatiga qaytarish";
    s[(int)Str::LabelCompareWithStock] = "Zavod mikrodasturi bilan taqqoslash:";
    s[(int)Str::LabelActivePreview] = "Hozirgi faol:";
    s[(int)Str::LabelStockReferencePreview] = "Zavod etaloni:";
    s[(int)Str::FilterOnlyCustom] = "Faqat oʻzgartirilgan resurslarni koʻrsatish";
    s[(int)Str::MsgRestoredToStock] = "'%s' zavod etalon holatiga qaytarildi.";
    s[(int)Str::BtnReplaceFromPng] = "PNG orqali almashtirish...";
    s[(int)Str::BtnRestoreOriginal] = "Asl holatga qaytarish";
    s[(int)Str::BtnExportThisPng] = "Ushbu PNG ni eksport qilish...";
    s[(int)Str::BtnExportHorizontal] = "Gorizontal eksport (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Rang palitrasi va render (RGB565)";
    s[(int)Str::LabelTheme] = "Mavzu";
    s[(int)Str::BtnApply] = "Qoʻllash";
    s[(int)Str::BtnResetAll] = "Hammasini qaytarish";
    s[(int)Str::LabelRole] = "Rol";
    s[(int)Str::BtnResetRole] = "Rolni tiklash";
    s[(int)Str::PromptLoadFwColors] = "Ranglarni tahrirlash uchun mikrodasturni yuklang.";
    s[(int)Str::LabelPreviewTint] = "Koʻrib chiqish tusi";
    s[(int)Str::InfoDigitsMonochrome] = "Raqamlar monoxrom niqoblardir. Ularning rangi mikrodastur kodi bilan belgilanadi.";
    s[(int)Str::InfoPreviewNotSaved] = "Koʻrib chiqish tusi mikrodasturga saqlanmaydi. RGB565 PNG eksportiga bu tus qoʻshiladi.";
    s[(int)Str::LabelPreviewOnly] = "Faqat koʻrib chiqish:";
    s[(int)Str::LabelPresets] = "Pretselar...";
    s[(int)Str::BtnResetColor] = "Rangni tiklash";
    s[(int)Str::LabelZoom] = "Kattalashtirish";
    s[(int)Str::LabelPixelGrid] = "Piksel toʻri";
    s[(int)Str::LabelHorizontalPreview] = "Oʻqish uchun qulay gorizontal koʻrinish (136x22):";
    s[(int)Str::FontSec1Title] = "1. Shriftni tanlash";
    s[(int)Str::FontSec1WindowsFont] = "Windows shrifti";
    s[(int)Str::FontSec1BrowseTtf] = "TTF/OTF faylni tanlash...";
    s[(int)Str::FontSec1ActiveFont] = "Faol shrift: %s";
    s[(int)Str::FontSec2Title] = "2. Koʻrsatish qolipi";
    s[(int)Str::FontSec3Title] = "3. Renderlash parametrlari";
    s[(int)Str::FontSec3Threshold] = "Yorqinlik chegarasi";
    s[(int)Str::FontSec3ThresholdHint] = "Kichik = qalinroq, Katta = ingichkaroq";
    s[(int)Str::FontSec3Stretch] = "Katak boʻylab choʻzish";
    s[(int)Str::FontSec3Baseline] = "Asosiy chiziq boʻyicha tekislash";
    s[(int)Str::FontSec3CenterV] = "Vertikal markazlashtirish";
    s[(int)Str::FontSec3PaddingX] = "Gorizontal oraliq X";
    s[(int)Str::FontSec3PaddingY] = "Vertikal oraliq Y";
    s[(int)Str::FontSec3Oversampling] = "Oversampling";
    s[(int)Str::FontSec4Title] = "4. Mikrodasturga kiritish";
    s[(int)Str::FontSec4ApplyPreset] = " > GLIFLARNI OCHILGAN MIKRODASTURGA QOʻLLASH < ";
    s[(int)Str::FontSec4ApplyAll] = "Barcha shriftlarni qoʻllash";
    s[(int)Str::FontSec5Title] = "5. PNG formatida eksport qilish";
    s[(int)Str::FontSec5OutputFolder] = "Chiqish jildi";
    s[(int)Str::FontSec5BrowseFolder] = "Jildni tanlash...";
    s[(int)Str::FontSec5OpenExplorer] = "Fayl boshqaruvchisida ochish";
    s[(int)Str::FontSec5ExportAllPreset] = "Barcha qoliplarni eksport qilish";
    s[(int)Str::FontSec5ExportButton] = "1-bitli PNG fayllarni diskka saqlash";
    s[(int)Str::FontRightLivePreview] = "Shriftni jonli koʻrish";
    s[(int)Str::FontRightNoGlyphs] = "Gliflar yuklanmagan.";
    s[(int)Str::FontPresetInfo] = "Qolip: %s (%dx%d px, %d ta glif, hajmi: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 paket texnik tavsifi";
    s[(int)Str::FwInfoNoFirmware] = "Mikrodastur yuklanmagan. 'Resurslar studiyasi' boʻlimidan .zwdf/.xwdf faylini oching.";
    s[(int)Str::FwInfoFilePath] = "Fayl yoʻli: %s";
    s[(int)Str::FwInfoSha256] = "Dastlabki SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Paket hajmi: %zu bayt";
    s[(int)Str::FwInfoHwRevisions] = "Apparat versiyalari: %zu ta yozuv";
    s[(int)Str::FwInfoRecognizedResources] = "Aniqlangan resurslar: %zu ta (shriftlar, belgilar, unvonlar, yozuvlar, ekranlar)";
    s[(int)Str::FwInfoRecordsTable] = "Apparat yozuvlari jadvali";
    s[(int)Str::FwInfoColRecord] = "Yozuv #";
    s[(int)Str::FwInfoColDevCode] = "Qurilma kodi";
    s[(int)Str::FwInfoColPayload] = "Yuklama hajmi";
    s[(int)Str::FwInfoColImage] = "Tasvir hajmi";
    s[(int)Str::FwInfoColFlashBase] = "Flash asosiy manzili";
    s[(int)Str::FwInfoColMSP] = "Stek koʻrsatkichi (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Qayta ishga tushirish vektori";
    s[(int)Str::BtnRevertAllModifications] = "Barcha oʻzgarishlarni bekor qilish";
    s[(int)Str::MsgReadyOpenFw] = "Tayyor. .zwdf/.xwdf faylini oching yoki shaxsiy shriftlarni yarating.";
    s[(int)Str::MsgSuccessOpened] = "Ochildi: %s";
    s[(int)Str::MsgOpenError] = "Ochishda xatolik: %s";
    s[(int)Str::MsgAllReverted] = "Barcha kiritilgan oʻzgarishlar asl holatiga qaytarildi.";
    s[(int)Str::MsgExportSuccess] = "%d ta resurs eksport qilindi: %s";
    s[(int)Str::MsgExportError] = "Eksportda xatolik: %s";
    s[(int)Str::MsgImportSuccess] = "Jilddan %d ta PNG import qilindi.";
    s[(int)Str::MsgImportError] = "Importda xatolik: %s";
    s[(int)Str::MsgBuildSuccess] = "Yaratish MUVAFFAQIYATLI: %s";
    s[(int)Str::MsgBuildError] = "Yaratishda xatolik: %s";
    s[(int)Str::MsgFontNotLoaded] = "Xatolik: Shrift generatorga yuklanmagan!";
    s[(int)Str::MsgFontInjectedPreset] = "%d ta glif (%s oʻlcham) mikrodastur xotirasiga kiritildi!";
    s[(int)Str::MsgFontInjectedAll] = "Qoʻllangan gliflar: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d ta PNG fayl muvaffaqiyatli eksport qilindi: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d ta fayl (%s uchun) muvaffaqiyatli eksport qilindi: %s";
    s[(int)Str::MsgFontExportError] = "Eksportda xatolik: %s";
    s[(int)Str::ModalBuildReportTitle] = "Yaratish hisoboti";
    s[(int)Str::ModalBuildSuccessBanner] = "MIKRODASTUR YARATILDI VA TEKSHIRILDI!";
    s[(int)Str::ModalSavedFile] = "Saqlangan fayl: %s";
    s[(int)Str::ModalOutputSize] = "Chiqish hajmi: %zu bayt";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Oʻzgartirilgan rasm resurslari: %zu";
    s[(int)Str::ModalModColorGroups] = "Oʻzgartirilgan rang guruhlari: %zu";
    s[(int)Str::ModalPatchedRanges] = "Tahrirlangan bayt diapazonlari: %zu ta (%zu ta apparat yozuvida)";
    s[(int)Str::ModalUntouchedBytes] = "Tegilmagan toza baytlar: %zu (asliga aynan bir xil)";
    s[(int)Str::ModalReportNote] = "Batafsil hisobot mikrodastur yonida .report.json fayliga saqlandi.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Fayl boshqaruvchisida ochish";
    s[(int)Str::BtnCopyClipboard] = "Rasmdan nusxa olish";
    s[(int)Str::BtnPasteClipboard] = "Rasmni joylashtirish";
    s[(int)Str::BtnUndo] = "Bekor qilish";
    s[(int)Str::BtnRedo] = "Qaytarish";
    s[(int)Str::TooltipUndo] = "Bekor qilish: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Qaytarish: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Bekor qilinadigan amal yoʻq";
    s[(int)Str::TooltipRedoEmpty] = "Qaytariladigan amal yoʻq";
    s[(int)Str::LabelPixelTools] = "Piksel asboblari:";
    s[(int)Str::ToolPencil] = "Qalam";
    s[(int)Str::ToolEraser] = "Oʻchirgʻich";
    s[(int)Str::BtnInvertPixels] = "Inversiya";
    s[(int)Str::BtnClearPixels] = "Tozalash";
    s[(int)Str::BtnFlipH] = "Gorizontal aylantirish";
    s[(int)Str::BtnFlipV] = "Vertikal aylantirish";
    s[(int)Str::MsgCopiedToClipboard] = "%s tasviri buferga nusxalandi.";
    s[(int)Str::MsgPastedFromClipboard] = "Buferdagi rasm %s uchun joylashtirildi!";
    s[(int)Str::MsgClipboardNoImage] = "Xotira buferida rasm topilmadi.";
    s[(int)Str::MsgUndoPerformed] = "Bekor qilindi: %s";
    s[(int)Str::MsgRedoPerformed] = "Qaytarildi: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu ta yozuv | %zu ta oʻzgarish";
    s[(int)Str::BadgeProjectSummary] = "| Loyiha: %s (%s) | %zu ta yozuv | %zu ta oʻzgarish";
    s[(int)Str::LabelCategory] = "Kategoriya";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Guruh: %s  |  Rang: Maxsus (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Guruh: %s  |  Rang: %s";
    s[(int)Str::LabelMetaHardware] = "Flash manzili: 0x%08X  |  Oʻlchamlari: %u x %u px  |  Baytlar: %zu  |  Yozuvlar: %zu";
    s[(int)Str::LabelPixelCoord] = "Piksel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Chap tugma: %s  |  Oʻng tugma: Oʻchirish";
    s[(int)Str::LabelDrawAction] = "Chizish";
    s[(int)Str::LabelEraseAction] = "Oʻchirish";
    s[(int)Str::LabelFontPreset] = "Qolip";
    s[(int)Str::LabelFontPixelGrid] = "Piksel toʻri";
    s[(int)Str::MsgAppliedColorTheme] = "'%s' rang mavzusi barcha mos resurslarga qoʻllandi.";
    s[(int)Str::MsgImportedMatchingPngs] = "Jilddan %zu ta mos PNG import qilindi.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Mikrodastur Drag & Drop orqali muvaffaqiyatli ochildi: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop xatosi: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Shrift Drag & Drop orqali yuklandi: %s";
    s[(int)Str::MsgFailedLoadFont] = "Shrift faylini yuklab boʻlmadi: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "%s uchun tanlangan PNG asliga mos (oʻzgartirilmagan).";
    s[(int)Str::MsgStagedReplacement] = "%s uchun PNG almashtirishi tayyorlandi!";
    s[(int)Str::MsgPngLoadError] = "PNG yuklash xatosi: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "%s almashtirilgan resurs asliga mos keldi (oʻzgarmadi).";
    s[(int)Str::MsgStagedReplacementCustom] = "%s uchun maxsus PNG almashtirishi tayyorlandi!";
    s[(int)Str::MsgStagedRgbReplacement] = "%s uchun maxsus RGB565 PNG almashtirishi tayyorlandi!";
    s[(int)Str::MsgRestoredOriginal] = "%s uchun asl mikrodastur resursi qaytarildi.";
    s[(int)Str::MsgSavedAssetPng] = "%s fayli %s sifatida saqlandi";
    s[(int)Str::MsgSaveError] = "Saqlashda xatolik: %s";
    s[(int)Str::MsgSavedBootLogo] = "Gorizontal logotip %s sifatida saqlandi";
    s[(int)Str::MsgRefuseOverwrite] = "Ochilgan asl faylni ustidan yozish taqiqlandi. Boshqa yoʻlni tanlang.";
    s[(int)Str::MsgColorPatchError] = "Ranglar palitrasi xatosi: %s";
    s[(int)Str::MsgPackError] = "Paketlash xatosi: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu ta oʻzgartirilgan PNG import qilindi (%zu ta asliga mos keldi).";
    s[(int)Str::LabelBuiltin] = "oʻrnatilgan";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Oʻrnatilgan mikrodasturni yuklab boʻlmadi: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Oʻrnatilgan mikrodastur yuklandi: %s";
    s[(int)Str::ThemeFactoryDefault] = "Zavod standarti";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Voopoo qizil va sariq asl zavod uslubi";
    s[(int)Str::ThemeCyberpunkNeon] = "Kiberpank neon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Elektr sian, neon pushti va yorqin binafsha";
    s[(int)Str::ThemeMatrixGreen] = "Matritsa yashil";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT terminal fosfor va zumrad yashil";
    s[(int)Str::ThemeArcticIceBlue] = "Arktika muzi";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Muzlik sian, chuqur osmon va moviy jilo";
    s[(int)Str::ThemeAmberFire] = "Qahrabo olov";
    s[(int)Str::ThemeAmberFireDesc] = "Lava toʻq sariq, oltin olov va choʻgʻ";
    s[(int)Str::ThemePurpleNebula] = "Binafsha tumanlik";
    s[(int)Str::ThemePurpleNebulaDesc] = "Toʻq binafsha, lavanda va neon ametist";
    s[(int)Str::ThemeStealthDarkGray] = "Stels toʻq kulrang";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Xira grafit, slanets va qurol metalli";
    s[(int)Str::RoleAccentRed] = "Urgʻu / qizil rejim";
    s[(int)Str::RoleAccentRedScope] = "Standart rejimlardagi asosiy UI urgʻusi — 8 ta joy";
    s[(int)Str::RoleAccentCyan] = "Urgʻu / sian rejimi";
    s[(int)Str::RoleAccentCyanScope] = "SMART/RBA holatlaridagi asosiy UI urgʻusi — 7 ta joy";
    s[(int)Str::RoleHighlightYellow] = "Ajratish / sariq";
    s[(int)Str::RoleHighlightYellowScope] = "Hisob chizigʻi va xizmat panellari — 4 ta joy";
    s[(int)Str::RoleRankBronze] = "Daraja / bronza";
    s[(int)Str::RoleRankBronzeScope] = "Bronza darajasi palitrasi";
    s[(int)Str::RoleRankGold] = "Daraja / oltin";
    s[(int)Str::RoleRankGoldScope] = "Oltin darajasi palitrasi";
    s[(int)Str::RoleRankIce] = "Daraja / platina (muz)";
    s[(int)Str::RoleRankIceScope] = "Platina / Muz darajasi palitrasi";
    s[(int)Str::RoleRankCyan] = "Daraja / sian";
    s[(int)Str::RoleRankCyanScope] = "Sian darajasi palitrasi";
    s[(int)Str::RoleRankOrange] = "Daraja / toʻq sariq";
    s[(int)Str::RoleRankOrangeScope] = "Toʻq sariq darajasi palitrasi";
    s[(int)Str::RoleRankRed] = "Daraja / qizil";
    s[(int)Str::RoleRankRedScope] = "Qizil darajasi palitrasi";
    s[(int)Str::RoleDigitsWattage] = "Quvvat raqamlari";
    s[(int)Str::RoleDigitsWattageScope] = "Asosiy ekrandagi katta quvvat koʻrsatkichi (19x40)";
    s[(int)Str::RoleDigitsStats] = "Statistika raqamlari";
    s[(int)Str::ColorPresetWhite] = "Oq (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Asosiy / Qizil (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Sian (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Xizmat / Sariq (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bronza (#B57947)";
    s[(int)Str::ColorPresetGold] = "Oltin (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Toʻq sariq (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Yashil (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Asosiy raqamlar 0-9, 10 ta fayl)";
    s[(int)Str::FontPreset11x20] = "11x20 (Ikkilamchi raqamlar 0-9, 10 ta fayl)";
    s[(int)Str::FontPreset7x13] = "7x13 (Kichik holat 0-9, A-F, 16 ta fayl)";
    s[(int)Str::FontPreset6x10] = "6x10 (Mikro holat 0-9, 10 ta fayl)";
    s[(int)Str::FontOs1x] = "1x (Toʻgʻridan-toʻgʻri)";
    s[(int)Str::FontOs2x] = "2x (Tez)";
    s[(int)Str::FontOs4x] = "4x (Tavsiya etilgan)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

static void InitAzerbaijani() {
    auto& s = g_table[(int)Language::Azerbaijani];
    s[(int)Str::SupportFull] = "Tam";
    s[(int)Str::SupportNone] = "Dəstəklənmir";
    s[(int)Str::SupportPalette] = "Rəng palitrası";
    s[(int)Str::SupportMonochrome] = "Monoxrom ekran; rəng palitrası yoxdur.";
    s[(int)Str::EmulatorHistoryDay] = "Tarixçə günü";
    s[(int)Str::EmulatorClockHour] = "Saat";
    s[(int)Str::EmulatorClockMinute] = "Dəqiqə";
    s[(int)Str::EmulatorClockField] = "Seçilmiş sahə";
    s[(int)Str::MenuFile] = "Fayl";
    s[(int)Str::MenuEdit] = "Redaktə";
    s[(int)Str::MenuLanguage] = "Dil";
    s[(int)Str::MenuHelp] = "Kömək";
    s[(int)Str::UpdateTitle] = "Studiya yeniləmələri";
    s[(int)Str::UpdateVersion] = "Quraşdırılmış versiya: %s";
    s[(int)Str::UpdateCheck] = "Yeniləmələri yoxla";
    s[(int)Str::UpdateChecking] = "GitHub Releases yoxlanılır...";
    s[(int)Str::UpdateCurrent] = "Ən son versiya quraşdırılıb.";
    s[(int)Str::UpdateDownload] = "Endir";
    s[(int)Str::UpdateDownloading] = "Endirilir və yoxlanılır...";
    s[(int)Str::UpdateInstall] = "Quraşdır və yenidən başlat";
    s[(int)Str::UpdateReady] = "Quraşdırmağa hazırdır.";
    s[(int)Str::UpdateChangelog] = "Dəyişikliklər";
    s[(int)Str::UpdateFailed] = "Yeniləmə xətası";
    s[(int)Str::UpdateAvailable] = "Mövcud versiya: %s";
    s[(int)Str::MenuCreate] = "Yarat...";
    s[(int)Str::MenuOpen] = "Aç...";
    s[(int)Str::MenuImport] = "Resursları idxal et...";
    s[(int)Str::MenuExport] = "Resursları ixrac et...";
    s[(int)Str::MenuBuild] = "Mikroproqramı yığ...";
    s[(int)Str::MenuExit] = "Çıxış";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Resurslar";
    s[(int)Str::TabFontGenerator] = "Şrift Generatoru";
    s[(int)Str::TabFirmwareInfo] = "Mikroproqram haqqında";
    s[(int)Str::TabFirmwareComparison] = "Müqayisə";
    s[(int)Str::CompareModeTwoFiles] = "İki fayl";
    s[(int)Str::CompareModeEditor] = "Redaktor və fayl";
    s[(int)Str::CompareLeft] = "Sol";
    s[(int)Str::CompareRight] = "Sağ";
    s[(int)Str::CompareChooseFile] = "Fayl seç...";
    s[(int)Str::CompareReadOnlyNote] = "Yalnız baxış; redaktordakı dəyişikliklər nəzərə alınır.";
    s[(int)Str::CompareChooseSources] = "Müqayisə üçün hər iki mənbəni seçin.";
    s[(int)Str::CompareEditorUnavailable] = "Redaktorda mikroproqram açılmayıb.";
    s[(int)Str::CompareChanged] = "Dəyişdirilib";
    s[(int)Str::CompareAdded] = "Əlavə edilib";
    s[(int)Str::CompareRemoved] = "Silinib";
    s[(int)Str::CompareEqual] = "Eynidir";
    s[(int)Str::CompareIncompatible] = "Uyğun deyil";
    s[(int)Str::CompareDecodeError] = "Dekodlaşdırma xətası";
    s[(int)Str::CompareStatusFilter] = "Vəziyyət";
    s[(int)Str::CompareDifferencePreview] = "Fərq";
    s[(int)Str::CompareHighlightPixels] = "Fərqləri vurğula";
    s[(int)Str::CompareDifferentPixels] = "Fərqli piksellər: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Resurs yoxdur";
    s[(int)Str::CompareIncompatibleHint] = "Resurs növləri və ya ölçüləri fərqlidir: piksel müqayisəsi mümkün deyil.";
    s[(int)Str::CompareImageUnavailable] = "Şəkil mövcud deyil";
    s[(int)Str::CompareTextureError] = "Önizləmə teksturası yaradıla bilmədi.";
    s[(int)Str::CompareAllStatuses] = "Bütün vəziyyətlər";
    s[(int)Str::TabFirmwareEmulator] = "Emulyator";
    s[(int)Str::TabFirmwareLayout] = "UI düzülüşü";
    s[(int)Str::LayoutUnsupported] = "Bu mikroproqram üçün UI düzülüşünü redaktə etmək mümkün deyil.";
    s[(int)Str::LayoutNativeNote] = "Elementi sürükləyin və ya X/Y daxil edin. Önizləmə dəyişdirilmiş mikroproqramı icra edir; dəyişikliklər seçilmiş aparat reviziyasına aiddir.";
    s[(int)Str::LayoutRevision] = "Aparat reviziyası";
    s[(int)Str::LayoutCheckingRevisions] = "Eyni interfeyslər yoxlanılır…";
    s[(int)Str::LayoutAllRevisions] = "Bütün reviziyalar";
    s[(int)Str::LayoutRevisionGroup] = "Eyni UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Reviziya: %s";
    s[(int)Str::LayoutResetScope] = "Seçilmiş reviziyaları sıfırla";
    s[(int)Str::LayoutSelectionUnavailable] = "Seçilmiş reviziyaların hamısında uyğun element yoxdur.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI elementləri";
    s[(int)Str::LayoutNoElements] = "Bu ekranda hərəkət etdirilən blok yoxdur.";
    s[(int)Str::LayoutSharedBlock] = "Bu blokun koordinatları bir neçə ekran üçün ortaqdır.";
    s[(int)Str::LayoutPositionX] = "X (piksel)";
    s[(int)Str::LayoutPositionY] = "Y (piksel)";
    s[(int)Str::LayoutOffset] = "Sürüşmə: %+d, %+d piksel";
    s[(int)Str::LayoutGridStep] = "Addım: %d × %d piksel";
    s[(int)Str::LayoutGridSpacing] = "Şəbəkə addımı (piksel)";
    s[(int)Str::LayoutGridConstraint] = "Şəbəkə nöqtəsi mümkün olmadıqda yerli koordinat addımları və sərhədləri üstün tutulur.";
    s[(int)Str::LayoutSnapGrid] = "Şəbəkəyə bağla";
    s[(int)Str::LayoutSnapElements] = "Element kənarlarına və mərkəzlərinə bağla";
    s[(int)Str::LayoutGuides] = "Hərəkət bələdçiləri";
    s[(int)Str::LayoutAlign] = "Ekrana düzlə";
    s[(int)Str::LayoutAlignLeft] = "Sol";
    s[(int)Str::LayoutAlignCenter] = "X mərkəz";
    s[(int)Str::LayoutAlignRight] = "Sağ";
    s[(int)Str::LayoutAlignTop] = "Yuxarı";
    s[(int)Str::LayoutAlignMiddle] = "Y mərkəz";
    s[(int)Str::LayoutAlignBottom] = "Aşağı";
    s[(int)Str::LayoutNudgeHint] = "Oxlar: yerli piksel addımı; Shift: böyük addım. Fokus üçün ekrana klikləyin.";
    s[(int)Str::LayoutOverlays] = "Element konturları";
    s[(int)Str::LayoutOverlapOverlay] = "Kəsişmələri vurğula";
    s[(int)Str::LayoutOverlaps] = "Kəsişən elementlər";
    s[(int)Str::LayoutNoOverlaps] = "Düzbucaqlılar kəsişmir";
    s[(int)Str::LayoutDrawOrderUnknown] = "Yerli çəkilmə ardıcıllığı təsdiqlənməyib";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) %s (#%d) elementindən sonra çəkilir";
    s[(int)Str::LayoutDrawOrderNote] = "Bu, daimi qatlar deyil, əsas yenidən çəkmə çağırışlarının ardıcıllığıdır. Canlı yeniləmələr fərqli ola bilər; əvvəlki məzmunu yalnız yazılmış piksellər örtür.";
    s[(int)Str::LayoutDrawSequence] = "Yerli çəkmə ardıcıllığı";
    s[(int)Str::LayoutDrawInterleaved] = "Çağırışlar növbələşir; örtülmə elementin hissəsindən asılıdır.";
    s[(int)Str::EmulatorBefore] = "Əvvəl (açılan mikroproqram)";
    s[(int)Str::EmulatorAfter] = "Sonra (cari dəyişikliklər)";
    s[(int)Str::EmulatorSideBySide] = "Yan-yana";
    s[(int)Str::EmulatorOriginalError] = "Açılan mikroproqramın önizləməsi alınmadı: %s";
    s[(int)Str::MenuSaveProject] = "Layihəni saxla";
    s[(int)Str::MenuSaveProjectAs] = "Layihəni fərqli saxla...";
    s[(int)Str::MsgProjectSaved] = "Layihə saxlandı: %s";
    s[(int)Str::MenuBuild] = "Mikroproqramı ixrac et";
    s[(int)Str::BtnOpenFirmware] = "Mikroproqram və ya layihə aç...";
    s[(int)Str::StartupOpenFile] = "Mikroproqram və ya layihə aç...";
    s[(int)Str::UnsavedPrompt] = "Davam etmədən layihə saxlanılsın?";
    s[(int)Str::UnsavedPrompt] = "Davam etmədən layihə saxlanılsın?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Elementi sıfırla";
    s[(int)Str::LayoutResetRevision] = "Bu reviziyanın düzülüşünü sıfırla";
    s[(int)Str::LayoutResetAll] = "Bütün düzülüş dəyişikliklərini sıfırla";
    s[(int)Str::LayoutPatchSites] = "Yamaq ünvanları";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Önizləmə vəziyyəti";
    s[(int)Str::LayoutMoveElement] = "UI elementini köçür";
    s[(int)Str::LayoutRba] = "RBA rejimi";
    s[(int)Str::LayoutTemperatureControl] = "Temperatur nəzarəti";
    s[(int)Str::LayoutTemperature] = "Temperatur (°C)";
    s[(int)Str::LayoutRenderFailed] = "Orijinal kadr buferi mövcud deyil.";
    s[(int)Str::LayoutElementPower] = "Güc";
    s[(int)Str::LayoutElementBattery] = "Batareya";
    s[(int)Str::LayoutElementResistance] = "Müqavimət";
    s[(int)Str::LayoutElementDuration] = "Müddət";
    s[(int)Str::LayoutElementPuffs] = "Çəkimlər";
    s[(int)Str::LayoutElementMode] = "Rejim";
    s[(int)Str::LayoutElementStatus] = "Vəziyyət";
    s[(int)Str::LayoutElementScore] = "Xal";
    s[(int)Str::LayoutElementStatistics] = "Statistika";
    s[(int)Str::LayoutElementVoltage] = "Gərginlik";
    s[(int)Str::LayoutElementGraph] = "Qrafik";
    s[(int)Str::LayoutElementRank] = "Dərəcə";
    s[(int)Str::LayoutElementModeSelection] = "Rejim seçimi";
    s[(int)Str::LayoutElementLogo] = "Loqotip";
    s[(int)Str::LayoutChangedElements] = "Dəyişdirilmiş UI elementləri";
    s[(int)Str::EmulatorSupported] = "Bütün daxili DRAG mikroproqramlarının doğma UI önbaxışı.";
    s[(int)Str::EmulatorNativeNote] = "Cari dəyişikliklərlə orijinal çəkmə kodu. Elektrik hissəsi emulyasiya olunmur.";
    s[(int)Str::EmulatorWatts] = "Güc (W)";
    s[(int)Str::EmulatorPuffs] = "Çəkimlər";
    s[(int)Str::EmulatorBattery] = "Batareya (0–5)";
    s[(int)Str::EmulatorResistance] = "Müqavimət (mΩ)";
    s[(int)Str::EmulatorDuration] = "Müddət (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Kilidli";
    s[(int)Str::EmulatorCharging] = "Doldurulur";
    s[(int)Str::EmulatorScore] = "Ümumi xal";
    s[(int)Str::EmulatorScoreDisplay] = "Ən yaxşı nəticə";
    s[(int)Str::EmulatorRedraw] = "Yenidən çək";
    s[(int)Str::EmulatorLandscape] = "Üfüqi";
    s[(int)Str::EmulatorScreen] = "Ekran";
    s[(int)Str::EmulatorMainScreen] = "Əsas ekran";
    s[(int)Str::FontSystemFonts] = "Sistem şriftləri";
    s[(int)Str::FontUserFonts] = "İstifadəçi şriftləri";
    s[(int)Str::EmulatorMenuItem] = "Menyu elementi";
    s[(int)Str::EmulatorMaterial] = "Material";
    s[(int)Str::EmulatorMenuConfirmed] = "Təsdiqlənib";
    s[(int)Str::EmulatorResources] = "Mikroproqramın oxuduğu resurslar";
    s[(int)Str::EmulatorNoResources] = "Kataloq resurslarının oxunması qeydə alınmayıb.";
    s[(int)Str::EmulatorUidNote] = "Emulyasiya olunan MCU UID, qoşulmuş cihazın ID-si deyil.";
    s[(int)Str::EmulatorLoaderVersion] = "Virtual yükləyici versiyası";
    s[(int)Str::EmulatorLoaderId] = "Virtual CHIP-ID baytları";
    s[(int)Str::EmulatorLoaderNote] = "Yükləyici məlumatları emulyasiya edilir, mikroproqramdan və ya cihazdan oxunmur.";
    s[(int)Str::TabFirmwarePatches] = "Yamalar";
    s[(int)Str::PatchBootLogo] = "Başlanğıc ekranı";
    s[(int)Str::PatchShutdownLogo] = "Söndürmə ekranı";
    s[(int)Str::PatchDeviceInfo] = "CHIP ID / versiya ekranı";
    s[(int)Str::PatchDeviceInfoNote] = "Yalnız statik şəkil. Servis ekranını əvəz edir, əsas güc və batareya ekranını dəyişmir.";
    s[(int)Str::PatchLogoNote] = "Bütün DRAG S Score / Factory versiyaları üçün müstəqil 80 × 160 şəkillər. Cihazda yoxlanılmayıb.";
    s[(int)Str::PatchUnsupported] = "Bu yama DRAG S Score və ya Factory mikroproqramı tələb edir.";
    s[(int)Str::PatchEnabled] = "Şəxsi loqonu aktiv et";
    s[(int)Str::PatchCanvas] = "Kompozisiya · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Ekranın nativ önizləməsi";
    s[(int)Str::PatchPreviewNote] = "Nativ önizləmə kompozisiya qaralamasını deyil, tətbiq edilmiş parametrləri göstərir.";
    s[(int)Str::PatchFit] = "Sığdır";
    s[(int)Str::PatchCover] = "Doldur";
    s[(int)Str::PatchStretch] = "Dart";
    s[(int)Str::PatchScale] = "Miqyas";
    s[(int)Str::PatchAnimation] = "Giriş animasiyası";
    s[(int)Str::PatchNone] = "Yoxdur";
    s[(int)Str::PatchFade] = "Tədricən görünmə";
    s[(int)Str::PatchDuration] = "Müddət (ms)";
    s[(int)Str::PatchAutoHardware] = "Avtomatik";
    s[(int)Str::PatchAnimationPhase] = "Animasiya mərhələsi";
    s[(int)Str::PatchPlay] = "Oynat";
    s[(int)Str::EmulatorModeScreens] = "Ekranlar";
    s[(int)Str::EmulatorModeDevice] = "Cihaz";
    s[(int)Str::DeviceUnsupported] = "Cihaz rejimi DRAG S Score / HW 3103 dəstəkləyir.";
    s[(int)Str::DeviceNote] = "Mikroproqram virtual periferiya ilə Reset-dən işləyir. Qızdırma simulyasiya edilir; elektrik təhlükəsizliyi yoxlanmır.";
    s[(int)Str::DeviceReset] = "Yenidən başlat";
    s[(int)Str::DevicePause] = "Fasilə";
    s[(int)Str::DeviceResume] = "Başlat";
    s[(int)Str::DeviceStep] = "Addım";
    s[(int)Str::DeviceChanged] = "Cari dəyişikliklər hələ yüklənməyib.";
    s[(int)Str::DeviceApply] = "Dəyişikliklərlə yenidən başlat";
    s[(int)Str::DeviceButtons] = "Düymələri və ya boşluq / ↑ / ↓ saxlayın.";
    s[(int)Str::DeviceAtomizerConnected] = "Spiral qoşulub";
    s[(int)Str::DeviceTime] = "Virtual vaxt";
    s[(int)Str::DeviceSleeping] = "Prosessor yatır";
    s[(int)Str::DeviceLoading] = "Cihaz yüklənir...";
    s[(int)Str::DeviceLog] = "Periferiyaya müraciətlər";
    s[(int)Str::BadgeNoFirmware] = "| [Mikroproqram açılmayıb - .zwdf/.xwdf faylını bura sürükləyin]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu element | %zu qeyd";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Mikroproqramı aç...";
    s[(int)Str::BtnExportAll] = "Bütün resursları qovluğa ixrac et...";
    s[(int)Str::BtnImportFolder] = "PNG qovluğunu idxal et...";
    s[(int)Str::BtnBuildFirmware] = " YENİ MİKROPROQRAM YARAT ";
    s[(int)Str::BtnRevertAllStaged] = "Bütün dəyişiklikləri sıfırla";
    s[(int)Str::GroupAll] = "Bütün qruplar";
    s[(int)Str::GroupAlerts] = "Xəbərdarlıqlar (Dialoqlar və nişanlar)";
    s[(int)Str::GroupFonts] = "Şriftlər (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Xallar və rütbələr";
    s[(int)Str::GroupStatusLabels] = "Status və interfeys yazıları";
    s[(int)Str::GroupControls] = "İdarəetmə və batareya";
    s[(int)Str::GroupScreens] = "Ekranlar və açılış loqoları";
    s[(int)Str::GroupAnimations] = "Animasiyalar";
    s[(int)Str::PanelResourcesHeader] = "Resurslar";
    s[(int)Str::LabelSearch] = "Axtarış";
    s[(int)Str::SearchPlaceholder] = "ad, id, ünvan üzrə...";
    s[(int)Str::NoFirmwareLoadedList] = "Mikroproqram yüklənməyib.\nFayl > Yarat / Aç";
    s[(int)Str::NoResourcesMatchFilter] = "Axtarış filtrinə uyğun resurs tapılmadı.";
    s[(int)Str::PanelResourcePreviewHeader] = "Önizləmə";
    s[(int)Str::SelectResourcePrompt] = "Baxmaq və redaktə etmək üçün soldakı siyahıdan resurs seçin.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Qrup";
    s[(int)Str::LabelColor] = "Rəng";
    s[(int)Str::LabelColorCustom] = "Xüsusi";
    s[(int)Str::LabelFlashAddress] = "Flash ünvanı";
    s[(int)Str::LabelDimensions] = "Ölçülər";
    s[(int)Str::LabelBytes] = "Baytlar";
    s[(int)Str::LabelRecords] = "Qeydlər";
    s[(int)Str::StatusModifiedStaged] = "Status: DƏYİŞDİRİLDİ (yaddaşda hazırlanıb)";
    s[(int)Str::StatusOriginal] = "Status: ORİJİNAL (mikroproqramdan)";
    s[(int)Str::StatusStockOriginal] = "Status: ZAVOD ORİJİNALI (A050/E050 etalonuna uyğundur)";
    s[(int)Str::StatusCustomDiffers] = "Status: XÜSUSİ / MODİFİKASİYA (zavoddan fərqlənir)";
    s[(int)Str::BtnRestoreStock] = "Zavod vəziyyətinə qaytar";
    s[(int)Str::LabelCompareWithStock] = "Zavod etalon mikroproqramı ilə müqayisə:";
    s[(int)Str::LabelActivePreview] = "Hazırkı aktiv:";
    s[(int)Str::LabelStockReferencePreview] = "Zavod etalonu:";
    s[(int)Str::FilterOnlyCustom] = "Yalnız dəyişdirilmiş resursları göstər";
    s[(int)Str::MsgRestoredToStock] = "'%s' zavod etalon vəziyyətinə qaytarıldı.";
    s[(int)Str::BtnReplaceFromPng] = "PNG ilə əvəz et...";
    s[(int)Str::BtnRestoreOriginal] = "Orijinalı bərpa et";
    s[(int)Str::BtnExportThisPng] = "Bu PNG-ni ixrac et...";
    s[(int)Str::BtnExportHorizontal] = "Üfüqi ixrac et (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Rəng palitrası və render (RGB565)";
    s[(int)Str::LabelTheme] = "Mövzu";
    s[(int)Str::BtnApply] = "Tətbiq et";
    s[(int)Str::BtnResetAll] = "Hamısını sıfırla";
    s[(int)Str::LabelRole] = "Rol";
    s[(int)Str::BtnResetRole] = "Rolu sıfırla";
    s[(int)Str::PromptLoadFwColors] = "Rəngləri tənzimləmək üçün mikroproqramı yükləyin.";
    s[(int)Str::LabelPreviewTint] = "Önizləmə çaları";
    s[(int)Str::InfoDigitsMonochrome] = "Rəqəmlər monoxrom maskalardır. Onların rəngi mikroproqram kodu ilə təyin olunur.";
    s[(int)Str::InfoPreviewNotSaved] = "Önizləmə çaları mikroproqramda saxlanmır. RGB565 PNG ixracında bu çalar nəzərə alınır.";
    s[(int)Str::LabelPreviewOnly] = "Yalnız önizləmə üçün:";
    s[(int)Str::LabelPresets] = "Hazır şablonlar...";
    s[(int)Str::BtnResetColor] = "Rəngi sıfırla";
    s[(int)Str::LabelZoom] = "Önizləmə miqyası";
    s[(int)Str::LabelPixelGrid] = "Piksel toru";
    s[(int)Str::LabelHorizontalPreview] = "Rahat oxunan üfüqi önizləmə (136x22):";
    s[(int)Str::FontSec1Title] = "1. Şrift seçimi";
    s[(int)Str::FontSec1WindowsFont] = "Windows şrifti";
    s[(int)Str::FontSec1BrowseTtf] = "TTF/OTF faylı seç...";
    s[(int)Str::FontSec1ActiveFont] = "Aktiv şrift: %s";
    s[(int)Str::FontSec2Title] = "2. Göstərmə şablonu";
    s[(int)Str::FontSec3Title] = "3. Render parametrləri";
    s[(int)Str::FontSec3Threshold] = "Parlaqlıq həddi";
    s[(int)Str::FontSec3ThresholdHint] = "Aşağı = qalın, Yuxarı = incə";
    s[(int)Str::FontSec3Stretch] = "Xana boyunca dart";
    s[(int)Str::FontSec3Baseline] = "Baza xətti üzrə düzləndirmə";
    s[(int)Str::FontSec3CenterV] = "Şaquli mərkəzləşdirmə";
    s[(int)Str::FontSec3PaddingX] = "Üfüqi boşluq X";
    s[(int)Str::FontSec3PaddingY] = "Şaquli boşluq Y";
    s[(int)Str::FontSec3Oversampling] = "Oversampling";
    s[(int)Str::FontSec4Title] = "4. Mikroproqrama inteqrasiya";
    s[(int)Str::FontSec4ApplyPreset] = " > QLİFLƏRİ AÇIQ MİKROPROQRAMA TƏTBİQ ET < ";
    s[(int)Str::FontSec4ApplyAll] = "Bütün şriftləri tətbiq et";
    s[(int)Str::FontSec5Title] = "5. PNG olaraq ixrac";
    s[(int)Str::FontSec5OutputFolder] = "Çıxış qovluğu";
    s[(int)Str::FontSec5BrowseFolder] = "Qovluq seç...";
    s[(int)Str::FontSec5OpenExplorer] = "Fayl bələdçisində aç";
    s[(int)Str::FontSec5ExportAllPreset] = "Bütün şablonları ixrac et";
    s[(int)Str::FontSec5ExportButton] = "1-bitlik PNG fayllarını diskə yaz";
    s[(int)Str::FontRightLivePreview] = "Canlı şrift önizləməsi";
    s[(int)Str::FontRightNoGlyphs] = "Qliflər yüklənməyib.";
    s[(int)Str::FontPresetInfo] = "Şablon: %s (%dx%d px, %d qlif, ölçü: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 paket spesifikasiyası";
    s[(int)Str::FwInfoNoFirmware] = "Mikroproqram yüklənməyib. 'Resurs Studiyası' bölməsindən .zwdf/.xwdf faylını açın.";
    s[(int)Str::FwInfoFilePath] = "Fayl yolu: %s";
    s[(int)Str::FwInfoSha256] = "Mənbə SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Paket ölçüsü: %zu bayt";
    s[(int)Str::FwInfoHwRevisions] = "Aparat versiyaları: %zu qeyd";
    s[(int)Str::FwInfoRecognizedResources] = "Aşkar edilən resurslar: %zu (şriftlər, nişanlar, rütbələr, yazılar, ekranlar)";
    s[(int)Str::FwInfoRecordsTable] = "Aparat qeydləri cədvəli";
    s[(int)Str::FwInfoColRecord] = "Qeyd #";
    s[(int)Str::FwInfoColDevCode] = "Cihaz kodu";
    s[(int)Str::FwInfoColPayload] = "Yükün ölçüsü";
    s[(int)Str::FwInfoColImage] = "Təsvirin ölçüsü";
    s[(int)Str::FwInfoColFlashBase] = "Flash baza ünvanı";
    s[(int)Str::FwInfoColMSP] = "Yığın göstəricisi (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Sıfırlama vektoru";
    s[(int)Str::BtnRevertAllModifications] = "Bütün dəyişiklikləri sıfırla";
    s[(int)Str::MsgReadyOpenFw] = "Hazırdır. .zwdf/.xwdf faylını açın və ya xüsusi şriftlər yaradın.";
    s[(int)Str::MsgSuccessOpened] = "Açıldı: %s";
    s[(int)Str::MsgOpenError] = "Açılma xətası: %s";
    s[(int)Str::MsgAllReverted] = "Bütün dəyişikliklər ilkin vəziyyətinə qaytarıldı.";
    s[(int)Str::MsgExportSuccess] = "%d resurs ixrac edildi: %s";
    s[(int)Str::MsgExportError] = "İxrac xətası: %s";
    s[(int)Str::MsgImportSuccess] = "Qovluqdan %d PNG idxal edildi.";
    s[(int)Str::MsgImportError] = "İdxal xətası: %s";
    s[(int)Str::MsgBuildSuccess] = "Yaradılma UĞURLU OLDU: %s";
    s[(int)Str::MsgBuildError] = "Yaradılma xətası: %s";
    s[(int)Str::MsgFontNotLoaded] = "Xəta: Şrift generatora yüklənməyib!";
    s[(int)Str::MsgFontInjectedPreset] = "%d qlif (%s ölçüsü) mikroproqram yaddaşına daxil edildi!";
    s[(int)Str::MsgFontInjectedAll] = "Tətbiq edilmiş qliflər: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d PNG faylı uğurla ixrac edildi: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d fayl (%s üçün) uğurla ixrac edildi: %s";
    s[(int)Str::MsgFontExportError] = "İxrac xətası: %s";
    s[(int)Str::ModalBuildReportTitle] = "Yaradılma hesabatı";
    s[(int)Str::ModalBuildSuccessBanner] = "MİKROPROQRAM YARADILDI VƏ YOXLANIŞ UĞURLU OLDU!";
    s[(int)Str::ModalSavedFile] = "Saxlanılan fayl: %s";
    s[(int)Str::ModalOutputSize] = "Çıxış ölçüsü: %zu bayt";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Dəyişdirilmiş rəsm resursları: %zu";
    s[(int)Str::ModalModColorGroups] = "Dəyişdirilmiş rəng qrupları: %zu";
    s[(int)Str::ModalPatchedRanges] = "Yamaqlanmış bayt diapazonları: %zu qeyddə %zu diapazon";
    s[(int)Str::ModalUntouchedBytes] = "Dəyişməz qalan təmiz baytlar: %zu (orijinal ilə eynidir)";
    s[(int)Str::ModalReportNote] = "Ətraflı hesabat mikroproqram yanında .report.json faylında saxlanıldı.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Fayl bələdçisində aç";
    s[(int)Str::BtnCopyClipboard] = "Təsviri kopyala";
    s[(int)Str::BtnPasteClipboard] = "Təsviri yapışdır";
    s[(int)Str::BtnUndo] = "Geri qaytar";
    s[(int)Str::BtnRedo] = "Təkrar et";
    s[(int)Str::TooltipUndo] = "Geri qaytar: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Təkrar et: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Geri qaytarılacaq əməl yoxdur";
    s[(int)Str::TooltipRedoEmpty] = "Təkrar ediləcək əməl yoxdur";
    s[(int)Str::LabelPixelTools] = "Piksel alətləri:";
    s[(int)Str::ToolPencil] = "Qələm";
    s[(int)Str::ToolEraser] = "Pozan";
    s[(int)Str::BtnInvertPixels] = "İnversiya";
    s[(int)Str::BtnClearPixels] = "Təmizlə";
    s[(int)Str::BtnFlipH] = "Üfüqi çevir";
    s[(int)Str::BtnFlipV] = "Şaquli çevir";
    s[(int)Str::MsgCopiedToClipboard] = "%s təsviri buferə kopyalandı.";
    s[(int)Str::MsgPastedFromClipboard] = "Mübadilə buferindəki təsvir %s üçün yapışdırıldı!";
    s[(int)Str::MsgClipboardNoImage] = "Mübadilə buferində şəkil tapılmadı.";
    s[(int)Str::MsgUndoPerformed] = "Geri qaytarıldı: %s";
    s[(int)Str::MsgRedoPerformed] = "Təkrar edildi: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu qeyd | %zu dəyişiklik";
    s[(int)Str::BadgeProjectSummary] = "| Layihə: %s (%s) | %zu qeyd | %zu dəyişiklik";
    s[(int)Str::LabelCategory] = "Kateqoriya";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Qrup: %s  |  Rəng: Xüsusi (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Qrup: %s  |  Rəng: %s";
    s[(int)Str::LabelMetaHardware] = "Flash ünvanı: 0x%08X  |  Ölçülər: %u x %u px  |  Baytlar: %zu  |  Qeydlər: %zu";
    s[(int)Str::LabelPixelCoord] = "Piksel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Sol klik: %s  |  Sağ klik: Poz";
    s[(int)Str::LabelDrawAction] = "Çək";
    s[(int)Str::LabelEraseAction] = "Poz";
    s[(int)Str::LabelFontPreset] = "Şablon";
    s[(int)Str::LabelFontPixelGrid] = "Piksel toru";
    s[(int)Str::MsgAppliedColorTheme] = "'%s' rəng mövzusu bütün uyğun resurslara tətbiq edildi.";
    s[(int)Str::MsgImportedMatchingPngs] = "Qovluqdan %zu uyğun PNG idxal edildi.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Drag & Drop ilə mikroproqram uğurla açıldı: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop xətası: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Drag & Drop ilə şrift yükləndi: %s";
    s[(int)Str::MsgFailedLoadFont] = "Şrift faylını yükləmək mümkün olmadı: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "%s üçün seçilmiş PNG orijinal ilə eynidir (dəyişmədi).";
    s[(int)Str::MsgStagedReplacement] = "%s üçün PNG əvəzləməsi hazırlandı!";
    s[(int)Str::MsgPngLoadError] = "PNG yükləmə xətası: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "%s dəyişdirilmiş resurs orijinal ilə eynidir (dəyişmədi).";
    s[(int)Str::MsgStagedReplacementCustom] = "%s üçün xüsusi PNG əvəzləməsi hazırlandı!";
    s[(int)Str::MsgStagedRgbReplacement] = "%s üçün xüsusi RGB565 PNG əvəzləməsi hazırlandı!";
    s[(int)Str::MsgRestoredOriginal] = "%s üçün orijinal mikroproqram resursu bərpa edildi.";
    s[(int)Str::MsgSavedAssetPng] = "%s faylı %s olaraq saxlanıldı";
    s[(int)Str::MsgSaveError] = "Saxlama xətası: %s";
    s[(int)Str::MsgSavedBootLogo] = "Üfüqi loqo %s kimi saxlanıldı";
    s[(int)Str::MsgRefuseOverwrite] = "Açılmış orijinal faylın üzərinə yazmaq qadağandır. Başqa yol seçin.";
    s[(int)Str::MsgColorPatchError] = "Rəng palitrası xətası: %s";
    s[(int)Str::MsgPackError] = "Paketləmə xətası: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu dəyişdirilmiş PNG idxal edildi (%zu orijinal ilə eynidir).";
    s[(int)Str::LabelBuiltin] = "daxili";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Daxili mikroproqramı yükləmək mümkün olmadı: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Daxili mikroproqram yükləndi: %s";
    s[(int)Str::ThemeFactoryDefault] = "Zavod standartı";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Orijinal Voopoo qırmızı və sarı zavod tərzi";
    s[(int)Str::ThemeCyberpunkNeon] = "Kiberpank neon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Elektrik sian, neon çəhrayı və bənövşəyi";
    s[(int)Str::ThemeMatrixGreen] = "Matris yaşıl";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT terminal fosfor və zümrüd yaşıl";
    s[(int)Str::ThemeArcticIceBlue] = "Arktika buzu";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Buzlaq sian, dərin səma və mavi parıltı";
    s[(int)Str::ThemeAmberFire] = "Kəhrəba alov";
    s[(int)Str::ThemeAmberFireDesc] = "Lava narıncı, qızıl alov və köz";
    s[(int)Str::ThemePurpleNebula] = "Bənövşəyi dumanlıq";
    s[(int)Str::ThemePurpleNebulaDesc] = "Tünd bənövşəyi, lavanda və neon ametist";
    s[(int)Str::ThemeStealthDarkGray] = "Stels tünd boz";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Mat qrafit, şifer və silah metalı";
    s[(int)Str::RoleAccentRed] = "Vurğu / qırmızı rejim";
    s[(int)Str::RoleAccentRedScope] = "Standart rejimlərdə əsas UI vurğusu — 8 yer";
    s[(int)Str::RoleAccentCyan] = "Vurğu / sian rejimi";
    s[(int)Str::RoleAccentCyanScope] = "SMART/RBA vəziyyətlərində əsas UI vurğusu — 7 yer";
    s[(int)Str::RoleHighlightYellow] = "İşıqlandırma / sarı";
    s[(int)Str::RoleHighlightYellowScope] = "Xal zolağı və xidmət panelləri — 4 yer";
    s[(int)Str::RoleRankBronze] = "Rütbə / bürünc";
    s[(int)Str::RoleRankBronzeScope] = "Bürünc rütbə palitrası";
    s[(int)Str::RoleRankGold] = "Rütbə / qızıl";
    s[(int)Str::RoleRankGoldScope] = "Qızıl rütbə palitrası";
    s[(int)Str::RoleRankIce] = "Rütbə / platin (buz)";
    s[(int)Str::RoleRankIceScope] = "Platin / Buz rütbə palitrası";
    s[(int)Str::RoleRankCyan] = "Rütbə / sian";
    s[(int)Str::RoleRankCyanScope] = "Sian rütbə palitrası";
    s[(int)Str::RoleRankOrange] = "Rütbə / narıncı";
    s[(int)Str::RoleRankOrangeScope] = "Narıncı rütbə palitrası";
    s[(int)Str::RoleRankRed] = "Rütbə / qırmızı";
    s[(int)Str::RoleRankRedScope] = "Qırmızı rütbə palitrası";
    s[(int)Str::RoleDigitsWattage] = "Güc rəqəmləri";
    s[(int)Str::RoleDigitsWattageScope] = "Əsas ekranda böyük güc göstəricisi (19x40)";
    s[(int)Str::RoleDigitsStats] = "Statistika rəqəmləri";
    s[(int)Str::ColorPresetWhite] = "Ağ (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Əsas / Qırmızı (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Sian (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Xidmət / Sarı (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bürünc (#B57947)";
    s[(int)Str::ColorPresetGold] = "Qızıl (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Narıncı (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Yaşıl (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Əsas rəqəmlər 0-9, 10 fayl)";
    s[(int)Str::FontPreset11x20] = "11x20 (İkinci dərəcəli rəqəmlər 0-9, 10 fayl)";
    s[(int)Str::FontPreset7x13] = "7x13 (Kiçik status 0-9, A-F, 16 fayl)";
    s[(int)Str::FontPreset6x10] = "6x10 (Mikro status 0-9, 10 fayl)";
    s[(int)Str::FontOs1x] = "1x (Birbaşa)";
    s[(int)Str::FontOs2x] = "2x (Sürətli)";
    s[(int)Str::FontOs4x] = "4x (Tövsiyə olunan)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

static void InitKyrgyz() {
    auto& s = g_table[(int)Language::Kyrgyz];
    s[(int)Str::SupportFull] = "Толук";
    s[(int)Str::SupportNone] = "Колдоого алынбайт";
    s[(int)Str::SupportPalette] = "Түс палитрасы";
    s[(int)Str::SupportMonochrome] = "Монохромдуу экран; түс палитрасы жок.";
    s[(int)Str::EmulatorHistoryDay] = "Тарых күнү";
    s[(int)Str::EmulatorClockHour] = "Саат";
    s[(int)Str::EmulatorClockMinute] = "Мүнөт";
    s[(int)Str::EmulatorClockField] = "Тандалган талаа";
    s[(int)Str::MenuFile] = "Файл";
    s[(int)Str::MenuEdit] = "Түзөтүү";
    s[(int)Str::MenuLanguage] = "Тил";
    s[(int)Str::MenuHelp] = "Жардам";
    s[(int)Str::UpdateTitle] = "Студиянын жаңыртуулары";
    s[(int)Str::UpdateVersion] = "Орнотулган версия: %s";
    s[(int)Str::UpdateCheck] = "Жаңыртууларды текшерүү";
    s[(int)Str::UpdateChecking] = "GitHub Releases текшерилүүдө...";
    s[(int)Str::UpdateCurrent] = "Акыркы версия орнотулган.";
    s[(int)Str::UpdateDownload] = "Жүктөп алуу";
    s[(int)Str::UpdateDownloading] = "Жүктөлүүдө жана текшерилүүдө...";
    s[(int)Str::UpdateInstall] = "Орнотуу жана кайра иштетүү";
    s[(int)Str::UpdateReady] = "Орнотууга даяр.";
    s[(int)Str::UpdateChangelog] = "Өзгөртүүлөр";
    s[(int)Str::UpdateFailed] = "Жаңыртуу катасы";
    s[(int)Str::UpdateAvailable] = "Жеткиликтүү версия: %s";
    s[(int)Str::MenuCreate] = "Түзүү...";
    s[(int)Str::MenuOpen] = "Ачуу...";
    s[(int)Str::MenuImport] = "Ресурстарды импорттоо...";
    s[(int)Str::MenuExport] = "Ресурстарды экспорттоо...";
    s[(int)Str::MenuBuild] = "Микропрограмманы чогултуу...";
    s[(int)Str::MenuExit] = "Чыгуу";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ресурстар";
    s[(int)Str::TabFontGenerator] = "Шрифт генератору";
    s[(int)Str::TabFirmwareInfo] = "Прошивка жөнүндө";
    s[(int)Str::TabFirmwareComparison] = "Салыштыруу";
    s[(int)Str::CompareModeTwoFiles] = "Эки файл";
    s[(int)Str::CompareModeEditor] = "Редактор жана файл";
    s[(int)Str::CompareLeft] = "Сол жак";
    s[(int)Str::CompareRight] = "Оң жак";
    s[(int)Str::CompareChooseFile] = "Файл тандоо...";
    s[(int)Str::CompareReadOnlyNote] = "Көрүү гана; редактордогу өзгөртүүлөр эске алынат.";
    s[(int)Str::CompareChooseSources] = "Салыштыруу үчүн эки булакты тандаңыз.";
    s[(int)Str::CompareEditorUnavailable] = "Редактордо прошивка ачылган эмес.";
    s[(int)Str::CompareChanged] = "Өзгөртүлгөн";
    s[(int)Str::CompareAdded] = "Кошулган";
    s[(int)Str::CompareRemoved] = "Өчүрүлгөн";
    s[(int)Str::CompareEqual] = "Бирдей";
    s[(int)Str::CompareIncompatible] = "Дал келбейт";
    s[(int)Str::CompareDecodeError] = "Декоддоо катасы";
    s[(int)Str::CompareStatusFilter] = "Абал";
    s[(int)Str::CompareDifferencePreview] = "Айырма";
    s[(int)Str::CompareHighlightPixels] = "Айырмаларды белгилөө";
    s[(int)Str::CompareDifferentPixels] = "Айырмаланган пикселдер: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Ресурс жок";
    s[(int)Str::CompareIncompatibleHint] = "Ресурстардын түрү же өлчөмү башка: пикселдик салыштыруу жеткиликсиз.";
    s[(int)Str::CompareImageUnavailable] = "Сүрөт жеткиликсиз";
    s[(int)Str::CompareTextureError] = "Алдын ала көрүү текстурасын түзүү мүмкүн болгон жок.";
    s[(int)Str::CompareAllStatuses] = "Бардык абалдар";
    s[(int)Str::TabFirmwareEmulator] = "Эмулятор";
    s[(int)Str::TabFirmwareLayout] = "UI жайгашуусу";
    s[(int)Str::LayoutUnsupported] = "Бул микропрограмма үчүн UI жайгашуусун түзөтүү жеткиликсиз.";
    s[(int)Str::LayoutNativeNote] = "Элементти сүйрөңүз же X/Y киргизиңиз. Алдын ала көрүү өзгөртүлгөн микропрограмманы аткарат; өзгөртүүлөр тандалган аппараттык ревизияга тиешелүү.";
    s[(int)Str::LayoutRevision] = "Аппараттык ревизия";
    s[(int)Str::LayoutCheckingRevisions] = "Бирдей интерфейстер текшерилүүдө…";
    s[(int)Str::LayoutAllRevisions] = "Бардык ревизиялар";
    s[(int)Str::LayoutRevisionGroup] = "Бирдей UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Ревизия: %s";
    s[(int)Str::LayoutResetScope] = "Тандалган ревизияларды калыбына келтирүү";
    s[(int)Str::LayoutSelectionUnavailable] = "Тандалган ревизиялардын баарында шайкеш элемент жок.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI элементтери";
    s[(int)Str::LayoutNoElements] = "Бул экранда жылдырылуучу блоктор жок.";
    s[(int)Str::LayoutSharedBlock] = "Бул блоктун координаттары бир нече экран үчүн жалпы.";
    s[(int)Str::LayoutPositionX] = "X (пиксел)";
    s[(int)Str::LayoutPositionY] = "Y (пиксел)";
    s[(int)Str::LayoutOffset] = "Жылыш: %+d, %+d пикс.";
    s[(int)Str::LayoutGridStep] = "Кадам: %d × %d пикс.";
    s[(int)Str::LayoutGridSpacing] = "Тор кадамы (пикс.)";
    s[(int)Str::LayoutGridConstraint] = "Тор түйүнүн көрсөтүү мүмкүн болбосо, түпкү координата кадамы жана чектери артыкчылыктуу.";
    s[(int)Str::LayoutSnapGrid] = "Торго байлоо";
    s[(int)Str::LayoutSnapElements] = "Элементтин четтерине жана борборлоруна байлоо";
    s[(int)Str::LayoutGuides] = "Жылдыруу багыттагычтары";
    s[(int)Str::LayoutAlign] = "Экранга тегиздөө";
    s[(int)Str::LayoutAlignLeft] = "Сол";
    s[(int)Str::LayoutAlignCenter] = "X борбору";
    s[(int)Str::LayoutAlignRight] = "Оң";
    s[(int)Str::LayoutAlignTop] = "Жогору";
    s[(int)Str::LayoutAlignMiddle] = "Y борбору";
    s[(int)Str::LayoutAlignBottom] = "Төмөн";
    s[(int)Str::LayoutNudgeHint] = "Жебелер: түпкү пиксел кадамы; Shift: чоң кадам. Фокус үчүн экранды басыңыз.";
    s[(int)Str::LayoutOverlays] = "Элементтин чектери";
    s[(int)Str::LayoutOverlapOverlay] = "Кесилиштерди белгилөө";
    s[(int)Str::LayoutOverlaps] = "Кесилишкен элементтер";
    s[(int)Str::LayoutNoOverlaps] = "Тик төрт бурчтуктар кесилишпейт";
    s[(int)Str::LayoutDrawOrderUnknown] = "Түпкү тартуу тартиби тастыкталган эмес";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) %s (#%d) элементинен кийин тартылат";
    s[(int)Str::LayoutDrawOrderNote] = "Бул туруктуу катмарлар эмес, негизги тартуу чакырыктарынын тартиби. Жаңыртууларда ал айырмаланышы мүмкүн; мурдагы мазмунду жазылган пикселдер гана жабат.";
    s[(int)Str::LayoutDrawSequence] = "Түпкү тартуу тартиби";
    s[(int)Str::LayoutDrawInterleaved] = "Чакыруулар кезектешет; жабылышы элементтин бөлүгүнө жараша болот.";
    s[(int)Str::EmulatorBefore] = "Мурда (ачылган прошивка)";
    s[(int)Str::EmulatorAfter] = "Кийин (учурдагы өзгөртүүлөр)";
    s[(int)Str::EmulatorSideBySide] = "Катар";
    s[(int)Str::EmulatorOriginalError] = "Ачылган прошивканы алдын ала көрүү катасы: %s";
    s[(int)Str::MenuSaveProject] = "Долбоорду сактоо";
    s[(int)Str::MenuSaveProjectAs] = "Долбоорду башкача сактоо...";
    s[(int)Str::MsgProjectSaved] = "Долбоор сакталды: %s";
    s[(int)Str::MenuBuild] = "Прошивканы экспорттоо";
    s[(int)Str::BtnOpenFirmware] = "Прошивканы же долбоорду ачуу...";
    s[(int)Str::StartupOpenFile] = "Прошивканы же долбоорду ачуу...";
    s[(int)Str::UnsavedPrompt] = "Улантуудан мурун долбоор сакталсынбы?";
    s[(int)Str::UnsavedPrompt] = "Улантуудан мурун долбоор сакталсынбы?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Элементти баштапкыга кайтаруу";
    s[(int)Str::LayoutResetRevision] = "Бул ревизиянын жайгашуусун кайтаруу";
    s[(int)Str::LayoutResetAll] = "Жайгашуунун бардык өзгөртүүлөрүн кайтаруу";
    s[(int)Str::LayoutPatchSites] = "Патч даректери";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Алдын ала көрүү абалы";
    s[(int)Str::LayoutMoveElement] = "UI элементин жылдыруу";
    s[(int)Str::LayoutRba] = "RBA режими";
    s[(int)Str::LayoutTemperatureControl] = "Температураны көзөмөлдөө";
    s[(int)Str::LayoutTemperature] = "Температура (°C)";
    s[(int)Str::LayoutRenderFailed] = "Түпкү кадр буфери жеткиликсиз.";
    s[(int)Str::LayoutElementPower] = "Кубат";
    s[(int)Str::LayoutElementBattery] = "Батарея";
    s[(int)Str::LayoutElementResistance] = "Каршылык";
    s[(int)Str::LayoutElementDuration] = "Узактык";
    s[(int)Str::LayoutElementPuffs] = "Соруу саны";
    s[(int)Str::LayoutElementMode] = "Режим";
    s[(int)Str::LayoutElementStatus] = "Абал";
    s[(int)Str::LayoutElementScore] = "Упай";
    s[(int)Str::LayoutElementStatistics] = "Статистика";
    s[(int)Str::LayoutElementVoltage] = "Чыңалуу";
    s[(int)Str::LayoutElementGraph] = "График";
    s[(int)Str::LayoutElementRank] = "Даража";
    s[(int)Str::LayoutElementModeSelection] = "Режимди тандоо";
    s[(int)Str::LayoutElementLogo] = "Логотип";
    s[(int)Str::LayoutChangedElements] = "Өзгөртүлгөн UI элементтери";
    s[(int)Str::EmulatorSupported] = "Бардык камтылган DRAG прошивкаларынын түпкү UI көрүнүшү.";
    s[(int)Str::EmulatorNativeNote] = "Өзгөртүүлөр менен түпкү сүрөт тартуу коду. Электр бөлүгү эмуляцияланбайт.";
    s[(int)Str::EmulatorWatts] = "Кубат (Вт)";
    s[(int)Str::EmulatorPuffs] = "Тартуулар";
    s[(int)Str::EmulatorBattery] = "Заряд (0–5)";
    s[(int)Str::EmulatorResistance] = "Каршылык (мОм)";
    s[(int)Str::EmulatorDuration] = "Узактык (0,01 с)";
    s[(int)Str::EmulatorLocked] = "Кулпуланган";
    s[(int)Str::EmulatorCharging] = "Кубаттоо";
    s[(int)Str::EmulatorScore] = "Жалпы упай";
    s[(int)Str::EmulatorScoreDisplay] = "Мыкты натыйжа";
    s[(int)Str::EmulatorRedraw] = "Кайра тартуу";
    s[(int)Str::EmulatorLandscape] = "Туурасынан";
    s[(int)Str::EmulatorScreen] = "Экран";
    s[(int)Str::EmulatorMainScreen] = "Негизги экран";
    s[(int)Str::FontSystemFonts] = "Системалык ариптер";
    s[(int)Str::FontUserFonts] = "Колдонуучунун ариптери";
    s[(int)Str::EmulatorMenuItem] = "Меню пункту";
    s[(int)Str::EmulatorMaterial] = "Материал";
    s[(int)Str::EmulatorMenuConfirmed] = "Ырасталды";
    s[(int)Str::EmulatorResources] = "Прошивка окуган ресурстар";
    s[(int)Str::EmulatorNoResources] = "Каталог ресурстарын окуу катталган жок.";
    s[(int)Str::EmulatorUidNote] = "Эмуляцияланган MCU UID, туташкан түзмөктүн ID эмес.";
    s[(int)Str::EmulatorLoaderVersion] = "Жүктөгүч версиясы (эмуляция)";
    s[(int)Str::EmulatorLoaderId] = "CHIP-ID байттары (эмуляция)";
    s[(int)Str::EmulatorLoaderNote] = "Жүктөгүч маалыматтары эмуляцияланат, прошивкадан же түзмөктөн окулбайт.";
    s[(int)Str::TabFirmwarePatches] = "Оңдоолор";
    s[(int)Str::PatchBootLogo] = "Ишке киргизүү экраны";
    s[(int)Str::PatchShutdownLogo] = "Өчүрүү экраны";
    s[(int)Str::PatchDeviceInfo] = "CHIP ID / нуска экраны";
    s[(int)Str::PatchDeviceInfoNote] = "Статикалык сүрөт гана. Кызмат экранын алмаштырат, негизги кубат жана батарея экранын өзгөртпөйт.";
    s[(int)Str::PatchLogoNote] = "Бардык DRAG S Score / Factory нускалары үчүн өзүнчө 80 × 160 сүрөттөр. Түзмөктө текшерилген эмес.";
    s[(int)Str::PatchUnsupported] = "Бул оңдоо DRAG S Score же Factory прошивкасын талап кылат.";
    s[(int)Str::PatchEnabled] = "Өз логотибин күйгүзүү";
    s[(int)Str::PatchCanvas] = "Композиция · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Экрандын нативдик көрүнүшү";
    s[(int)Str::PatchPreviewNote] = "Нативдик көрүнүш композициянын долбоорун эмес, колдонулган жөндөөлөрдү көрсөтөт.";
    s[(int)Str::PatchFit] = "Батыруу";
    s[(int)Str::PatchCover] = "Толтуруу";
    s[(int)Str::PatchStretch] = "Созуу";
    s[(int)Str::PatchScale] = "Масштаб";
    s[(int)Str::PatchAnimation] = "Пайда болуу анимациясы";
    s[(int)Str::PatchNone] = "Жок";
    s[(int)Str::PatchFade] = "Акырындап пайда болуу";
    s[(int)Str::PatchDuration] = "Узактык (мс)";
    s[(int)Str::PatchAutoHardware] = "Автоматтык";
    s[(int)Str::PatchAnimationPhase] = "Анимация баскычы";
    s[(int)Str::PatchPlay] = "Ойнотуу";
    s[(int)Str::EmulatorModeScreens] = "Экрандар";
    s[(int)Str::EmulatorModeDevice] = "Түзмөк";
    s[(int)Str::DeviceUnsupported] = "Түзмөк режими DRAG S Score / HW 3103 колдойт.";
    s[(int)Str::DeviceNote] = "Прошивка виртуалдык периферия менен Reset-тен аткарылат. Жылытуу моделденет; электр коопсуздугу текшерилбейт.";
    s[(int)Str::DeviceReset] = "Кайра баштоо";
    s[(int)Str::DevicePause] = "Тыным";
    s[(int)Str::DeviceResume] = "Баштоо";
    s[(int)Str::DeviceStep] = "Кадам";
    s[(int)Str::DeviceChanged] = "Учурдагы өзгөртүүлөр жүктөлө элек.";
    s[(int)Str::DeviceApply] = "Өзгөртүүлөр менен кайра баштоо";
    s[(int)Str::DeviceButtons] = "Баскычтарды же боштук / ↑ / ↓ кармаңыз.";
    s[(int)Str::DeviceAtomizerConnected] = "Спираль туташкан";
    s[(int)Str::DeviceTime] = "Виртуалдык убакыт";
    s[(int)Str::DeviceSleeping] = "Процессор уктоодо";
    s[(int)Str::DeviceLoading] = "Түзмөк жүктөлүүдө...";
    s[(int)Str::DeviceLog] = "Периферияга кайрылуулар";
    s[(int)Str::BadgeNoFirmware] = "| [Прошивка ачылган жок - .zwdf/.xwdf файлын бул жерге сүйрөңүз]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu элемент | %zu жазуу";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Прошивканы ачуу...";
    s[(int)Str::BtnExportAll] = "Бардык ресурстарды папкага экспорттоо...";
    s[(int)Str::BtnImportFolder] = "PNG папкасын импорттоо...";
    s[(int)Str::BtnBuildFirmware] = " ЖАҢЫ ПРОШИВКАНЫ ЧОГУЛТУУ ";
    s[(int)Str::BtnRevertAllStaged] = "Бардык өзгөртүүлөрдү жокко чыгаруу";
    s[(int)Str::GroupAll] = "Бардык топтор";
    s[(int)Str::GroupAlerts] = "Билдирмелер (Диалогдор жана сөлөкөттөр)";
    s[(int)Str::GroupFonts] = "Шрифттер (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Упай жана даражалар";
    s[(int)Str::GroupStatusLabels] = "Абал жана интерфейс жазуулары";
    s[(int)Str::GroupControls] = "Башкаруу жана батарея";
    s[(int)Str::GroupScreens] = "Экрандар жана жүктөө логотиптери";
    s[(int)Str::GroupAnimations] = "Анимациялар";
    s[(int)Str::PanelResourcesHeader] = "Ресурстар";
    s[(int)Str::LabelSearch] = "Издөө";
    s[(int)Str::SearchPlaceholder] = "аталышы, id, дареги боюнча...";
    s[(int)Str::NoFirmwareLoadedList] = "Прошивка жүктөлгөн жок.\nФайл > Түзүү / Ачуу";
    s[(int)Str::NoResourcesMatchFilter] = "Издөө чыпкасына дал келген ресурстар жок.";
    s[(int)Str::PanelResourcePreviewHeader] = "Алдын ала көрүү";
    s[(int)Str::SelectResourcePrompt] = "Көрүү жана оңдоо үчүн сол тизмеден ресурсту тандаңыз.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Топ";
    s[(int)Str::LabelColor] = "Түс";
    s[(int)Str::LabelColorCustom] = "Өзгөчө";
    s[(int)Str::LabelFlashAddress] = "Flash дареги";
    s[(int)Str::LabelDimensions] = "Өлчөмдөрү";
    s[(int)Str::LabelBytes] = "Байттар";
    s[(int)Str::LabelRecords] = "Жазуулар";
    s[(int)Str::StatusModifiedStaged] = "Абал: ӨЗГӨРТҮЛГӨН (эс тутумда даярдалган)";
    s[(int)Str::StatusOriginal] = "Абал: БАШТАПКЫ (прошивкадан)";
    s[(int)Str::StatusStockOriginal] = "Абал: ЗАВОДДУК БАШТАПКЫ (A050/E050 эталонуна дал келет)";
    s[(int)Str::StatusCustomDiffers] = "Абал: ӨЗГӨРТҮЛГӨН (заводдуктан айырмаланат)";
    s[(int)Str::BtnRestoreStock] = "Заводдук абалга келтирүү";
    s[(int)Str::LabelCompareWithStock] = "Заводдук прошивка менен салыштыруу:";
    s[(int)Str::LabelActivePreview] = "Учурдагы активдүү:";
    s[(int)Str::LabelStockReferencePreview] = "Заводдук эталон:";
    s[(int)Str::FilterOnlyCustom] = "Өзгөртүлгөн ресурстарды гана көрсөтүү";
    s[(int)Str::MsgRestoredToStock] = "'%s' заводдук эталонго кайтарылды.";
    s[(int)Str::BtnReplaceFromPng] = "PNG аркылуу алмаштыруу...";
    s[(int)Str::BtnRestoreOriginal] = "Баштапкы абалга кайтаруу";
    s[(int)Str::BtnExportThisPng] = "Бул PNGди экспорттоо...";
    s[(int)Str::BtnExportHorizontal] = "Туурасынан экспорттоо (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Түс палитрасы жана рендерер (RGB565)";
    s[(int)Str::LabelTheme] = "Тема";
    s[(int)Str::BtnApply] = "Колдонуу";
    s[(int)Str::BtnResetAll] = "Баарын баштапкы абалга келтирүү";
    s[(int)Str::LabelRole] = "Ролу";
    s[(int)Str::BtnResetRole] = "Ролду тазалоо";
    s[(int)Str::PromptLoadFwColors] = "Түстөрдү өзгөртүү үчүн прошивканы жүктөңүз.";
    s[(int)Str::LabelPreviewTint] = "Алдын ала көрүү түсү";
    s[(int)Str::InfoDigitsMonochrome] = "Сандар монохромдук маскалар болуп саналат. Алардын түсү прошивка коду менен аныкталат.";
    s[(int)Str::InfoPreviewNotSaved] = "Көрүү түсү прошивкага сакталбайт. RGB565 PNG экспортуна бул түс кошулат.";
    s[(int)Str::LabelPreviewOnly] = "Алдын ала көрүү үчүн гана:";
    s[(int)Str::LabelPresets] = "Алдын ала орнотуулар...";
    s[(int)Str::BtnResetColor] = "Түстү баштапкы абалга келтирүү";
    s[(int)Str::LabelZoom] = "Масштаб";
    s[(int)Str::LabelPixelGrid] = "Пиксел тору";
    s[(int)Str::LabelHorizontalPreview] = "Окууга ыңгайлуу горизонталдык көрүнүш (136x22):";
    s[(int)Str::FontSec1Title] = "1. Шрифтти тандоо";
    s[(int)Str::FontSec1WindowsFont] = "Windows шрифти";
    s[(int)Str::FontSec1BrowseTtf] = "TTF/OTF файлын тандоо...";
    s[(int)Str::FontSec1ActiveFont] = "Активдүү шрифт: %s";
    s[(int)Str::FontSec2Title] = "2. Көрсөтүү калыбы";
    s[(int)Str::FontSec3Title] = "3. Рендер параметрлери";
    s[(int)Str::FontSec3Threshold] = "Жарыктык босогосу";
    s[(int)Str::FontSec3ThresholdHint] = "Төмөн = жоон, Жогору = ичке";
    s[(int)Str::FontSec3Stretch] = "Уяча боюнча чоюу";
    s[(int)Str::FontSec3Baseline] = "Базалык сызык боюнча тегиздөө";
    s[(int)Str::FontSec3CenterV] = "Тигинен борборлоштуруу";
    s[(int)Str::FontSec3PaddingX] = "Горизонталдык аралык X";
    s[(int)Str::FontSec3PaddingY] = "Вертикалдык аралык Y";
    s[(int)Str::FontSec3Oversampling] = "Суперсэмплинг";
    s[(int)Str::FontSec4Title] = "4. Прошивкага киргизүү";
    s[(int)Str::FontSec4ApplyPreset] = " > ГЛИФТЕРДИ АЧЫЛГАН ПРОШИВКАГА КОЛДОНУУ < ";
    s[(int)Str::FontSec4ApplyAll] = "Бардык шрифттерди колдонуу";
    s[(int)Str::FontSec5Title] = "5. PNG форматында экспорттоо";
    s[(int)Str::FontSec5OutputFolder] = "Экспорт папкасы";
    s[(int)Str::FontSec5BrowseFolder] = "Папканы тандоо...";
    s[(int)Str::FontSec5OpenExplorer] = "Файл менеджеринде ачуу";
    s[(int)Str::FontSec5ExportAllPreset] = "Бардык калыптарды экспорттоо";
    s[(int)Str::FontSec5ExportButton] = "1-биттик PNG файлдарды дискке сактоо";
    s[(int)Str::FontRightLivePreview] = "Шрифтти жандуу көрүү";
    s[(int)Str::FontRightNoGlyphs] = "Глифтер жүктөлгөн жок.";
    s[(int)Str::FontPresetInfo] = "Калып: %s (%dx%d пкс, %d глиф, өлчөмү: %.1f пкс)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 топтомунун спецификациясы";
    s[(int)Str::FwInfoNoFirmware] = "Прошивка жүктөлгөн жок. 'Ресурстар студиясы' өтмөгүнөн .zwdf/.xwdf файлын ачыңыз.";
    s[(int)Str::FwInfoFilePath] = "Файл жолу: %s";
    s[(int)Str::FwInfoSha256] = "Баштапкы SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Топтом өлчөмү: %zu байт";
    s[(int)Str::FwInfoHwRevisions] = "Аппараттык версиялар: %zu жазуу";
    s[(int)Str::FwInfoRecognizedResources] = "Таанылган ресурстар: %zu (шрифттер, сөлөкөттөр, даражалар, жазуулар, экрандар)";
    s[(int)Str::FwInfoRecordsTable] = "Аппараттык жазуулар таблицасы";
    s[(int)Str::FwInfoColRecord] = "Жазуу #";
    s[(int)Str::FwInfoColDevCode] = "Түзмөк коду";
    s[(int)Str::FwInfoColPayload] = "Жүктөм өлчөмү";
    s[(int)Str::FwInfoColImage] = "Сүрөт өлчөмү";
    s[(int)Str::FwInfoColFlashBase] = "Flash базалык дареги";
    s[(int)Str::FwInfoColMSP] = "Стек көрсөткүчү (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Баштапкы абалга келтирүү вектору";
    s[(int)Str::BtnRevertAllModifications] = "Бардык өзгөртүүлөрдү жокко чыгаруу";
    s[(int)Str::MsgReadyOpenFw] = "Даяр. .zwdf/.xwdf файлын ачыңыз же өзгөчө шрифттерди жаратыңыз.";
    s[(int)Str::MsgSuccessOpened] = "Ачылды: %s";
    s[(int)Str::MsgOpenError] = "Ачуу катасы: %s";
    s[(int)Str::MsgAllReverted] = "Бардык киргизилген өзгөртүүлөр баштапкы абалга кайтарылды.";
    s[(int)Str::MsgExportSuccess] = "%d ресурс экспорттолду: %s";
    s[(int)Str::MsgExportError] = "Экспорттоо катасы: %s";
    s[(int)Str::MsgImportSuccess] = "Папкадан %d PNG импорттолду.";
    s[(int)Str::MsgImportError] = "Импорттоо катасы: %s";
    s[(int)Str::MsgBuildSuccess] = "Чогултуу ИЙГИЛИКТҮҮ: %s";
    s[(int)Str::MsgBuildError] = "Чогултуу катасы: %s";
    s[(int)Str::MsgFontNotLoaded] = "Ката: Шрифт генераторго жүктөлгөн жок!";
    s[(int)Str::MsgFontInjectedPreset] = "%d глиф (%s өлчөмү) прошивканын эс тутумуна киргизилди!";
    s[(int)Str::MsgFontInjectedAll] = "Колдонулган глифтер: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d PNG файлы ийгиликтүү экспорттолду: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d файл (%s үчүн) ийгиликтүү экспорттолду: %s";
    s[(int)Str::MsgFontExportError] = "Экспорттоо катасы: %s";
    s[(int)Str::ModalBuildReportTitle] = "Чогултуу отчету";
    s[(int)Str::ModalBuildSuccessBanner] = "ПРОШИВКАНЫ ЧОГУЛТУУ ЖАНА ТЕКШЕРҮҮ ИЙГИЛИКТҮҮ!";
    s[(int)Str::ModalSavedFile] = "Сакталган файл: %s";
    s[(int)Str::ModalOutputSize] = "Чыгыш өлчөмү: %zu байт";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Өзгөртүлгөн растрдык ресурстар: %zu";
    s[(int)Str::ModalModColorGroups] = "Өзгөртүлгөн түс топтору: %zu";
    s[(int)Str::ModalPatchedRanges] = "Оңдолгон байт диапазондору: %zu жазууда %zu диапазон";
    s[(int)Str::ModalUntouchedBytes] = "Өзгөртүлбөгөн таза байттар: %zu (баштапкысына толук дал келет)";
    s[(int)Str::ModalReportNote] = "Толук отчет прошивканын жанында .report.json файлына сакталды.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Файл менеджеринде ачуу";
    s[(int)Str::BtnCopyClipboard] = "Сүрөттү көчүрүү";
    s[(int)Str::BtnPasteClipboard] = "Сүрөттү коюу";
    s[(int)Str::BtnUndo] = "Жокко чыгаруу";
    s[(int)Str::BtnRedo] = "Кайталоо";
    s[(int)Str::TooltipUndo] = "Жокко чыгаруу: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Кайталоо: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Жокко чыгарууга эч нерсе жок";
    s[(int)Str::TooltipRedoEmpty] = "Кайталоого эч нерсе жок";
    s[(int)Str::LabelPixelTools] = "Пиксел куралдары:";
    s[(int)Str::ToolPencil] = "Карандаш";
    s[(int)Str::ToolEraser] = "Өчүргүч";
    s[(int)Str::BtnInvertPixels] = "Инверсия";
    s[(int)Str::BtnClearPixels] = "Тазалоо";
    s[(int)Str::BtnFlipH] = "Туурасынан буруу";
    s[(int)Str::BtnFlipV] = "Тигинен буруу";
    s[(int)Str::MsgCopiedToClipboard] = "%s сүрөтү алмашуу буферине көчүрүлдү.";
    s[(int)Str::MsgPastedFromClipboard] = "Алмашуу буфериндеги сүрөт %s үчүн коюлду!";
    s[(int)Str::MsgClipboardNoImage] = "Алмашуу буферинде сүрөт табылган жок.";
    s[(int)Str::MsgUndoPerformed] = "Жокко чыгарылды: %s";
    s[(int)Str::MsgRedoPerformed] = "Кайталанды: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu жазуу | %zu өзгөртүү";
    s[(int)Str::BadgeProjectSummary] = "| Долбоор: %s (%s) | %zu жазуу | %zu өзгөртүү";
    s[(int)Str::LabelCategory] = "Категория";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Топ: %s  |  Түс: Өзгөчө (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Топ: %s  |  Түс: %s";
    s[(int)Str::LabelMetaHardware] = "Flash дареги: 0x%08X  |  Өлчөмдөрү: %u x %u пкс  |  Байттар: %zu  |  Жазуулар: %zu";
    s[(int)Str::LabelPixelCoord] = "Пиксел: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Сол баскыч: %s  |  Оң баскыч: Өчүрүү";
    s[(int)Str::LabelDrawAction] = "Чийүү";
    s[(int)Str::LabelEraseAction] = "Өчүрүү";
    s[(int)Str::LabelFontPreset] = "Калып";
    s[(int)Str::LabelFontPixelGrid] = "Пиксел тору";
    s[(int)Str::MsgAppliedColorTheme] = "'%s' түс темасы бардык дал келген ресурстарга колдонулду.";
    s[(int)Str::MsgImportedMatchingPngs] = "Папкадан %zu дал келген PNG импорттолду.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Прошивка Drag & Drop аркылуу ийгиликтүү ачылды: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop катасы: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Шрифт Drag & Drop аркылуу жүктөлдү: %s";
    s[(int)Str::MsgFailedLoadFont] = "Шрифт файлын жүктөө мүмкүн болбоду: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "%s үчүн тандалган PNG баштапкыга дал келет (өзгөргөн жок).";
    s[(int)Str::MsgStagedReplacement] = "%s үчүн PNG алмаштыруусу даярдалды!";
    s[(int)Str::MsgPngLoadError] = "PNG жүктөө катасы: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "%s алмаштырылган ресурсу баштапкыга дал келди (өзгөргөн жок).";
    s[(int)Str::MsgStagedReplacementCustom] = "%s үчүн өзгөчө PNG алмаштыруусу даярдалды!";
    s[(int)Str::MsgStagedRgbReplacement] = "%s үчүн өзгөчө RGB565 PNG алмаштыруусу даярдалды!";
    s[(int)Str::MsgRestoredOriginal] = "%s үчүн баштапкы прошивка ресурсу калыбына келтирилди.";
    s[(int)Str::MsgSavedAssetPng] = "%s файлы %s катары сакталды";
    s[(int)Str::MsgSaveError] = "Сактоо катасы: %s";
    s[(int)Str::MsgSavedBootLogo] = "Туурасынан логотип %s катары сакталды";
    s[(int)Str::MsgRefuseOverwrite] = "Жүктөлгөн баштапкы файлдын үстүнөн жазууга тыюу салынган. Башка жолду тандаңыз.";
    s[(int)Str::MsgColorPatchError] = "Түс палитрасы катасы: %s";
    s[(int)Str::MsgPackError] = "Топтомдоо катасы: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu өзгөртүлгөн PNG импорттолду (%zu баштапкыга дал келди).";
    s[(int)Str::LabelBuiltin] = "камтылган";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Камтылган прошивканы жүктөө мүмкүн болбоду: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Камтылган прошивка жүктөлдү: %s";
    s[(int)Str::ThemeFactoryDefault] = "Заводдук баштапкы";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Voopoo кызыл жана сары заводдук стили";
    s[(int)Str::ThemeCyberpunkNeon] = "Киберпанк неон";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Электрдик циан, неон кызгылт жана кочкул кызыл";
    s[(int)Str::ThemeMatrixGreen] = "Матрица жашыл";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT терминал фосфору жана зымырыт жашыл";
    s[(int)Str::ThemeArcticIceBlue] = "Арктика музу";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Мөңгү циан, терең асман жана көгүлтүр жаркыроо";
    s[(int)Str::ThemeAmberFire] = "Янтарь жалыны";
    s[(int)Str::ThemeAmberFireDesc] = "Лава кызгылт сары, алтын жалын жана чок";
    s[(int)Str::ThemePurpleNebula] = "Кызгылт көк тумандык";
    s[(int)Str::ThemePurpleNebulaDesc] = "Терең сыя, лаванда жана неон аметист";
    s[(int)Str::ThemeStealthDarkGray] = "Стелс кочкул боз";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Күңүрт графит, сланец жана курал металлы";
    s[(int)Str::RoleAccentRed] = "Басым / кызыл режим";
    s[(int)Str::RoleAccentRedScope] = "Стандарттык режимдердеги негизги UI басымы — 8 орун";
    s[(int)Str::RoleAccentCyan] = "Басым / циан режими";
    s[(int)Str::RoleAccentCyanScope] = "SMART/RBA абалдарындагы негизги UI басымы — 7 орун";
    s[(int)Str::RoleHighlightYellow] = "Бөлүп көрсөтүү / сары";
    s[(int)Str::RoleHighlightYellowScope] = "Упай тилкеси жана кызматтык панелдер — 4 орун";
    s[(int)Str::RoleRankBronze] = "Даража / коло";
    s[(int)Str::RoleRankBronzeScope] = "Коло даража палитрасы";
    s[(int)Str::RoleRankGold] = "Даража / алтын";
    s[(int)Str::RoleRankGoldScope] = "Алтын даража палитрасы";
    s[(int)Str::RoleRankIce] = "Даража / платина (муз)";
    s[(int)Str::RoleRankIceScope] = "Платина / Муз даража палитрасы";
    s[(int)Str::RoleRankCyan] = "Даража / циан";
    s[(int)Str::RoleRankCyanScope] = "Циан даража палитрасы";
    s[(int)Str::RoleRankOrange] = "Даража / кызгылт сары";
    s[(int)Str::RoleRankOrangeScope] = "Кызгылт сары даража палитрасы";
    s[(int)Str::RoleRankRed] = "Даража / кызыл";
    s[(int)Str::RoleRankRedScope] = "Кызыл даража палитрасы";
    s[(int)Str::RoleDigitsWattage] = "Кубаттуулук сандары";
    s[(int)Str::RoleDigitsWattageScope] = "Башкы экрандагы чоң кубаттуулук көрсөткүчү (19x40)";
    s[(int)Str::RoleDigitsStats] = "Статистика сандары";
    s[(int)Str::ColorPresetWhite] = "Ак (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Негизги / Кызыл (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Циан (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Кызматтык / Сары (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Коло (#B57947)";
    s[(int)Str::ColorPresetGold] = "Алтын (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Кызгылт сары (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Жашыл (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Негизги сандар 0-9, 10 файл)";
    s[(int)Str::FontPreset11x20] = "11x20 (Экинчи сандар 0-9, 10 файл)";
    s[(int)Str::FontPreset7x13] = "7x13 (Кичине абал 0-9, A-F, 16 файл)";
    s[(int)Str::FontPreset6x10] = "6x10 (Микро абал 0-9, 10 файл)";
    s[(int)Str::FontOs1x] = "1x (Түз)";
    s[(int)Str::FontOs2x] = "2x (Тез)";
    s[(int)Str::FontOs4x] = "4x (Сунушталган)";
    s[(int)Str::FontOs8x] = "8x (Ультра)";
}

static void InitTajik() {
    auto& s = g_table[(int)Language::Tajik];
    s[(int)Str::SupportFull] = "Пурра";
    s[(int)Str::SupportNone] = "Дастгирӣ намешавад";
    s[(int)Str::SupportPalette] = "Палитраи рангҳо";
    s[(int)Str::SupportMonochrome] = "Экрани якранг; палитраи рангҳо нест.";
    s[(int)Str::EmulatorHistoryDay] = "Рӯзи таърих";
    s[(int)Str::EmulatorClockHour] = "Соат";
    s[(int)Str::EmulatorClockMinute] = "Дақиқа";
    s[(int)Str::EmulatorClockField] = "Майдони интихобшуда";
    s[(int)Str::MenuFile] = "Файл";
    s[(int)Str::MenuEdit] = "Таҳрир";
    s[(int)Str::MenuLanguage] = "Забон";
    s[(int)Str::MenuHelp] = "Кумак";
    s[(int)Str::UpdateTitle] = "Навсозии студия";
    s[(int)Str::UpdateVersion] = "Версияи насбшуда: %s";
    s[(int)Str::UpdateCheck] = "Санҷидани навсозиҳо";
    s[(int)Str::UpdateChecking] = "Санҷиши GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Версияи охирин насб шудааст.";
    s[(int)Str::UpdateDownload] = "Боргирӣ";
    s[(int)Str::UpdateDownloading] = "Боргирӣ ва санҷиш...";
    s[(int)Str::UpdateInstall] = "Насб ва бозоғозӣ";
    s[(int)Str::UpdateReady] = "Барои насб омода аст.";
    s[(int)Str::UpdateChangelog] = "Тағйирот";
    s[(int)Str::UpdateFailed] = "Хатои навсозӣ";
    s[(int)Str::UpdateAvailable] = "Версияи дастрас: %s";
    s[(int)Str::MenuCreate] = "Сохтан...";
    s[(int)Str::MenuOpen] = "Кушодан...";
    s[(int)Str::MenuImport] = "Воридоти захираҳо...";
    s[(int)Str::MenuExport] = "Содироти захираҳо...";
    s[(int)Str::MenuBuild] = "Сохтани микробарнома...";
    s[(int)Str::MenuExit] = "Баромад";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Захираҳо";
    s[(int)Str::TabFontGenerator] = "Генератори шрифтҳо";
    s[(int)Str::TabFirmwareInfo] = "Дар бораи нармафзор";
    s[(int)Str::TabFirmwareComparison] = "Муқоиса";
    s[(int)Str::CompareModeTwoFiles] = "Ду файл";
    s[(int)Str::CompareModeEditor] = "Муҳаррир ва файл";
    s[(int)Str::CompareLeft] = "Чап";
    s[(int)Str::CompareRight] = "Рост";
    s[(int)Str::CompareChooseFile] = "Интихоби файл...";
    s[(int)Str::CompareReadOnlyNote] = "Танҳо намоиш; тағйироти муҳаррир ба ҳисоб гирифта мешаванд.";
    s[(int)Str::CompareChooseSources] = "Барои муқоиса ҳар ду манбаъро интихоб кунед.";
    s[(int)Str::CompareEditorUnavailable] = "Дар муҳаррир нармафзор кушода нашудааст.";
    s[(int)Str::CompareChanged] = "Тағйирёфта";
    s[(int)Str::CompareAdded] = "Иловашуда";
    s[(int)Str::CompareRemoved] = "Ҳазфшуда";
    s[(int)Str::CompareEqual] = "Яксон";
    s[(int)Str::CompareIncompatible] = "Номувофиқ";
    s[(int)Str::CompareDecodeError] = "Хатои рамзкушоӣ";
    s[(int)Str::CompareStatusFilter] = "Ҳолат";
    s[(int)Str::CompareDifferencePreview] = "Фарқ";
    s[(int)Str::CompareHighlightPixels] = "Ҷудо кардани фарқиятҳо";
    s[(int)Str::CompareDifferentPixels] = "Пикселҳои фарқкунанда: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Захира нест";
    s[(int)Str::CompareIncompatibleHint] = "Навъ ё андозаи захираҳо фарқ мекунад: муқоисаи пикселҳо дастнорас аст.";
    s[(int)Str::CompareImageUnavailable] = "Тасвир дастнорас аст";
    s[(int)Str::CompareTextureError] = "Сохтани текстураи пешнамоиш имконнопазир шуд.";
    s[(int)Str::CompareAllStatuses] = "Ҳамаи ҳолатҳо";
    s[(int)Str::TabFirmwareEmulator] = "Эмулятор";
    s[(int)Str::TabFirmwareLayout] = "Ҷойгиршавии UI";
    s[(int)Str::LayoutUnsupported] = "Таҳрири ҷойгиршавии UI барои ин нармафзор дастрас нест.";
    s[(int)Str::LayoutNativeNote] = "Унсурро кашед ё X/Y ворид кунед. Пешнамоиш нармафзори тағйирёфтаро иҷро мекунад; тағйирот ба нусхаи сахтафзории интихобшуда дахл дорад.";
    s[(int)Str::LayoutRevision] = "Нусхаи сахтафзор";
    s[(int)Str::LayoutCheckingRevisions] = "Санҷиши интерфейсҳои якхела…";
    s[(int)Str::LayoutAllRevisions] = "Ҳамаи нусхаҳо";
    s[(int)Str::LayoutRevisionGroup] = "UI-и якхела: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Нусха: %s";
    s[(int)Str::LayoutResetScope] = "Барқарор кардани нусхаҳои интихобшуда";
    s[(int)Str::LayoutSelectionUnavailable] = "Барои ҳамаи нусхаҳои интихобшуда унсури мувофиқ нест.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Унсурҳои UI";
    s[(int)Str::LayoutNoElements] = "Дар ин экран блокҳои ҷунбондашаванда нестанд.";
    s[(int)Str::LayoutSharedBlock] = "Координатҳои ин блок барои якчанд экран умумӣ мебошанд.";
    s[(int)Str::LayoutPositionX] = "X (пиксел)";
    s[(int)Str::LayoutPositionY] = "Y (пиксел)";
    s[(int)Str::LayoutOffset] = "Ҷойивазкунӣ: %+d, %+d пиксел";
    s[(int)Str::LayoutGridStep] = "Қадам: %d × %d пиксел";
    s[(int)Str::LayoutGridSpacing] = "Қадами шабака (пиксел)";
    s[(int)Str::LayoutGridConstraint] = "Агар нуқтаи шабакаро ифода кардан имконнопазир бошад, қадам ва ҳудуди аслии координатҳо афзалият доранд.";
    s[(int)Str::LayoutSnapGrid] = "Пайваст ба шабака";
    s[(int)Str::LayoutSnapElements] = "Пайваст ба канорҳо ва марказҳои унсурҳо";
    s[(int)Str::LayoutGuides] = "Роҳнамои ҳаракат";
    s[(int)Str::LayoutAlign] = "Ҳамвор кардан бо экран";
    s[(int)Str::LayoutAlignLeft] = "Чап";
    s[(int)Str::LayoutAlignCenter] = "Маркази X";
    s[(int)Str::LayoutAlignRight] = "Рост";
    s[(int)Str::LayoutAlignTop] = "Боло";
    s[(int)Str::LayoutAlignMiddle] = "Маркази Y";
    s[(int)Str::LayoutAlignBottom] = "Поён";
    s[(int)Str::LayoutNudgeHint] = "Тирчаҳо: қадами аслии пиксел; Shift: қадами калонтар. Барои фокус экранро пахш кунед.";
    s[(int)Str::LayoutOverlays] = "Ҳудуди унсурҳо";
    s[(int)Str::LayoutOverlapOverlay] = "Нишон додани буришҳо";
    s[(int)Str::LayoutOverlaps] = "Унсурҳои буришкунанда";
    s[(int)Str::LayoutNoOverlaps] = "Буриши росткунҷаҳо нест";
    s[(int)Str::LayoutDrawOrderUnknown] = "Тартиби аслии тасвир тасдиқ нашудааст";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) дертар аз %s (#%d) кашида мешавад";
    s[(int)Str::LayoutDrawOrderNote] = "Ин тартиби даъватҳои тасвири асосист, на қабатҳои доимӣ. Ҳангоми навсозӣ он метавонад фарқ кунад; муҳтавои қаблиро танҳо пикселҳои воқеан навишташуда мепӯшонанд.";
    s[(int)Str::LayoutDrawSequence] = "Тартиби аслии тасвир";
    s[(int)Str::LayoutDrawInterleaved] = "Даъватҳо навбат мекунанд; рӯйпӯшшавӣ аз қисми унсур вобаста аст.";
    s[(int)Str::EmulatorBefore] = "Пеш (нармафзори кушода)";
    s[(int)Str::EmulatorAfter] = "Пас (тағйироти ҷорӣ)";
    s[(int)Str::EmulatorSideBySide] = "Паҳлу ба паҳлу";
    s[(int)Str::EmulatorOriginalError] = "Хатои пешнамоиши нармафзори кушода: %s";
    s[(int)Str::MenuSaveProject] = "Захираи лоиҳа";
    s[(int)Str::MenuSaveProjectAs] = "Захираи лоиҳа ҳамчун...";
    s[(int)Str::MsgProjectSaved] = "Лоиҳа захира шуд: %s";
    s[(int)Str::MenuBuild] = "Содироти нармафзор";
    s[(int)Str::BtnOpenFirmware] = "Кушодани нармафзор ё лоиҳа...";
    s[(int)Str::StartupOpenFile] = "Кушодани нармафзор ё лоиҳа...";
    s[(int)Str::UnsavedPrompt] = "Пеш аз идома лоиҳа захира шавад?";
    s[(int)Str::UnsavedPrompt] = "Пеш аз идома лоиҳа захира шавад?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Барқарор кардани унсур";
    s[(int)Str::LayoutResetRevision] = "Барқарор кардани ҷойгиршавии ин нусха";
    s[(int)Str::LayoutResetAll] = "Бекор кардани ҳамаи тағйироти ҷойгиршавӣ";
    s[(int)Str::LayoutPatchSites] = "Суроғаҳои ислоҳ";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Ҳолати пешнамоиш";
    s[(int)Str::LayoutMoveElement] = "Ҷойивазкунии унсури UI";
    s[(int)Str::LayoutRba] = "Реҷаи RBA";
    s[(int)Str::LayoutTemperatureControl] = "Назорати ҳарорат";
    s[(int)Str::LayoutTemperature] = "Ҳарорат (°C)";
    s[(int)Str::LayoutRenderFailed] = "Буфери аслии кадр дастрас нест.";
    s[(int)Str::LayoutElementPower] = "Қувва";
    s[(int)Str::LayoutElementBattery] = "Батарея";
    s[(int)Str::LayoutElementResistance] = "Муқовимат";
    s[(int)Str::LayoutElementDuration] = "Давомнокӣ";
    s[(int)Str::LayoutElementPuffs] = "Кашишҳо";
    s[(int)Str::LayoutElementMode] = "Реҷа";
    s[(int)Str::LayoutElementStatus] = "Ҳолат";
    s[(int)Str::LayoutElementScore] = "Хол";
    s[(int)Str::LayoutElementStatistics] = "Омор";
    s[(int)Str::LayoutElementVoltage] = "Шиддат";
    s[(int)Str::LayoutElementGraph] = "График";
    s[(int)Str::LayoutElementRank] = "Рутба";
    s[(int)Str::LayoutElementModeSelection] = "Интихоби реҷа";
    s[(int)Str::LayoutElementLogo] = "Логотип";
    s[(int)Str::LayoutChangedElements] = "Унсурҳои тағйирёфтаи UI";
    s[(int)Str::EmulatorSupported] = "Пешнамоиши аслии UI-и ҳамаи нармафзорҳои дохилии DRAG.";
    s[(int)Str::EmulatorNativeNote] = "Рамзи аслии тасвир бо тағйирот. Қисми барқӣ эмулятсия намешавад.";
    s[(int)Str::EmulatorWatts] = "Қувва (Вт)";
    s[(int)Str::EmulatorPuffs] = "Кашишҳо";
    s[(int)Str::EmulatorBattery] = "Заряд (0–5)";
    s[(int)Str::EmulatorResistance] = "Муқовимат (мОм)";
    s[(int)Str::EmulatorDuration] = "Давомнокӣ (0,01 с)";
    s[(int)Str::EmulatorLocked] = "Қулфшуда";
    s[(int)Str::EmulatorCharging] = "Пуркунӣ";
    s[(int)Str::EmulatorScore] = "Холи умумӣ";
    s[(int)Str::EmulatorScoreDisplay] = "Беҳтарин натиҷа";
    s[(int)Str::EmulatorRedraw] = "Аз нав кашидан";
    s[(int)Str::EmulatorLandscape] = "Уфуқӣ";
    s[(int)Str::EmulatorScreen] = "Экран";
    s[(int)Str::EmulatorMainScreen] = "Экрани асосӣ";
    s[(int)Str::FontSystemFonts] = "Ҳуруфи низом";
    s[(int)Str::FontUserFonts] = "Ҳуруфи корбар";
    s[(int)Str::EmulatorMenuItem] = "Банди меню";
    s[(int)Str::EmulatorMaterial] = "Мавод";
    s[(int)Str::EmulatorMenuConfirmed] = "Тасдиқ шуд";
    s[(int)Str::EmulatorResources] = "Захираҳои хондаи нармафзор";
    s[(int)Str::EmulatorNoResources] = "Хондани захираҳои феҳрист сабт нашудааст.";
    s[(int)Str::EmulatorUidNote] = "UID-и MCU-и эмулятсияшуда, на ID-и дастгоҳи пайвастшуда.";
    s[(int)Str::EmulatorLoaderVersion] = "Версияи боркунанда (эмулятсия)";
    s[(int)Str::EmulatorLoaderId] = "Байтҳои CHIP-ID (эмулятсия)";
    s[(int)Str::EmulatorLoaderNote] = "Маълумоти боркунанда эмулятсия мешавад, на аз нармафзор ё дастгоҳ хонда мешавад.";
    s[(int)Str::TabFirmwarePatches] = "Ислоҳҳо";
    s[(int)Str::PatchBootLogo] = "Экрани оғоз";
    s[(int)Str::PatchShutdownLogo] = "Экрани хомӯшкунӣ";
    s[(int)Str::PatchDeviceInfo] = "Экрани CHIP ID / версия";
    s[(int)Str::PatchDeviceInfoNote] = "Танҳо тасвири статикӣ. Экрани хизматиро иваз мекунад, на экрани асосии қувва ва батареяро.";
    s[(int)Str::PatchLogoNote] = "Тасвирҳои мустақили 80 × 160 барои ҳамаи навъҳои DRAG S Score / Factory. Дар дастгоҳ санҷида нашудааст.";
    s[(int)Str::PatchUnsupported] = "Ин ислоҳ нармафзори DRAG S Score ё Factory-ро талаб мекунад.";
    s[(int)Str::PatchEnabled] = "Фаъол кардани нишони худ";
    s[(int)Str::PatchCanvas] = "Таркиб · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Пешнамоиши аслии экран";
    s[(int)Str::PatchPreviewNote] = "Пешнамоиши аслӣ танзимоти татбиқшударо нишон медиҳад, на тарҳи таркибро.";
    s[(int)Str::PatchFit] = "Ҷой додан";
    s[(int)Str::PatchCover] = "Пур кардан";
    s[(int)Str::PatchStretch] = "Кашидан";
    s[(int)Str::PatchScale] = "Миқёс";
    s[(int)Str::PatchAnimation] = "Аниматсияи пайдоиш";
    s[(int)Str::PatchNone] = "Нест";
    s[(int)Str::PatchFade] = "Пайдоиши тадриҷӣ";
    s[(int)Str::PatchDuration] = "Давомнокӣ (мс)";
    s[(int)Str::PatchAutoHardware] = "Худкор";
    s[(int)Str::PatchAnimationPhase] = "Марҳилаи аниматсия";
    s[(int)Str::PatchPlay] = "Пахш кардан";
    s[(int)Str::EmulatorModeScreens] = "Экранҳо";
    s[(int)Str::EmulatorModeDevice] = "Дастгоҳ";
    s[(int)Str::DeviceUnsupported] = "Реҷаи дастгоҳ DRAG S Score / HW 3103-ро дастгирӣ мекунад.";
    s[(int)Str::DeviceNote] = "Нармафзор аз Reset бо таҷҳизоти виртуалӣ иҷро мешавад. Гармкунӣ моделсозӣ мешавад; бехатарии барқ санҷида намешавад.";
    s[(int)Str::DeviceReset] = "Аз нав оғоз";
    s[(int)Str::DevicePause] = "Таваққуф";
    s[(int)Str::DeviceResume] = "Оғоз";
    s[(int)Str::DeviceStep] = "Қадам";
    s[(int)Str::DeviceChanged] = "Тағйироти ҷорӣ ҳанӯз бор нашудаанд.";
    s[(int)Str::DeviceApply] = "Бо тағйирот аз нав оғоз";
    s[(int)Str::DeviceButtons] = "Тугмаҳо ё фосила / ↑ / ↓-ро нигоҳ доред.";
    s[(int)Str::DeviceAtomizerConnected] = "Спирал пайваст аст";
    s[(int)Str::DeviceTime] = "Вақти виртуалӣ";
    s[(int)Str::DeviceSleeping] = "Протсессор дар хоб";
    s[(int)Str::DeviceLoading] = "Дастгоҳ бор мешавад...";
    s[(int)Str::DeviceLog] = "Муроҷиат ба таҷҳизот";
    s[(int)Str::BadgeNoFirmware] = "| [Нармафзор кушода нашудааст - файли .zwdf/.xwdf-ро ба ин ҷо кашед]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu унсур | %zu сабт";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Кушодани нармафзор...";
    s[(int)Str::BtnExportAll] = "Содироти ҳамаи захираҳо ба ҷузвдон...";
    s[(int)Str::BtnImportFolder] = "Воридоти ҷузвдони PNG...";
    s[(int)Str::BtnBuildFirmware] = " СОХТАНИ НАЗМИ НАВИ НАВ ";
    s[(int)Str::BtnRevertAllStaged] = "Бекор кардани ҳамаи тағйирот";
    s[(int)Str::GroupAll] = "Ҳамаи гурӯҳҳо";
    s[(int)Str::GroupAlerts] = "Огоҳиномаҳо (Муколамаҳо ва нишонаҳо)";
    s[(int)Str::GroupFonts] = "Шрифтҳо (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Холҳо ва дараҷаҳо";
    s[(int)Str::GroupStatusLabels] = "Ҳолат ва навиштаҷоти интерфейс";
    s[(int)Str::GroupControls] = "Идоракунӣ ва батарея";
    s[(int)Str::GroupScreens] = "Экранҳо ва логотипҳои оғоз";
    s[(int)Str::GroupAnimations] = "Аниматсияҳо";
    s[(int)Str::PanelResourcesHeader] = "Захираҳо";
    s[(int)Str::LabelSearch] = "Ҷустуҷӯ";
    s[(int)Str::SearchPlaceholder] = "аз рӯи ном, id, суроға...";
    s[(int)Str::NoFirmwareLoadedList] = "Нармафзор бор нашудааст.\nФайл > Сохтан / Кушодан";
    s[(int)Str::NoResourcesMatchFilter] = "Захираҳои мувофиқ ба филтр ёфт нашуданд.";
    s[(int)Str::PanelResourcePreviewHeader] = "Пешнамоиш";
    s[(int)Str::SelectResourcePrompt] = "Барои дидан ва таҳрир кардан захираро аз рӯйхат интихоб кунед.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Гурӯҳ";
    s[(int)Str::LabelColor] = "Ранг";
    s[(int)Str::LabelColorCustom] = "Фармоишӣ";
    s[(int)Str::LabelFlashAddress] = "Суроғаи Flash";
    s[(int)Str::LabelDimensions] = "Андозаҳо";
    s[(int)Str::LabelBytes] = "Байтҳо";
    s[(int)Str::LabelRecords] = "Сабтҳо";
    s[(int)Str::StatusModifiedStaged] = "Ҳолат: ТАҒЙИРЁФТА (дар хотира омода шуд)";
    s[(int)Str::StatusOriginal] = "Ҳолат: АСЛӢ (аз нармафзор)";
    s[(int)Str::StatusStockOriginal] = "Ҳолат: ЗАВОДИИ АСЛӢ (ба эталони A050/E050 мувофиқ аст)";
    s[(int)Str::StatusCustomDiffers] = "Ҳолат: ТАҒЙИРЁФТА (аз заводӣ фарқ мекунад)";
    s[(int)Str::BtnRestoreStock] = "Барқарор кардани ҳолати заводӣ";
    s[(int)Str::LabelCompareWithStock] = "Муқоиса бо нармафзори заводии эталонӣ:";
    s[(int)Str::LabelActivePreview] = "Фаъоли кунунӣ:";
    s[(int)Str::LabelStockReferencePreview] = "Эталони заводӣ:";
    s[(int)Str::FilterOnlyCustom] = "Танҳо захираҳои тағйирёфта нишон дода шаванд";
    s[(int)Str::MsgRestoredToStock] = "'%s' ба ҳолати эталони заводӣ барқарор шуд.";
    s[(int)Str::BtnReplaceFromPng] = "Иваз кардан аз PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Барқароркунии аслӣ";
    s[(int)Str::BtnExportThisPng] = "Содироти ин PNG...";
    s[(int)Str::BtnExportHorizontal] = "Содироти уфуқӣ (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Палитраи рангҳо ва рендер (RGB565)";
    s[(int)Str::LabelTheme] = "Мавзӯъ";
    s[(int)Str::BtnApply] = "Татбиқ кардан";
    s[(int)Str::BtnResetAll] = "Бозсозии ҳама";
    s[(int)Str::LabelRole] = "Нақш";
    s[(int)Str::BtnResetRole] = "Бозсозии нақш";
    s[(int)Str::PromptLoadFwColors] = "Барои тағйири рангҳо нармафзорро бор кунед.";
    s[(int)Str::LabelPreviewTint] = "Ранги пешнамоиш";
    s[(int)Str::InfoDigitsMonochrome] = "Рақамҳо ниқобҳои монохромӣ мебошанд. Ранги онҳо тавассути рамзи нармафзор таъин мешавад.";
    s[(int)Str::InfoPreviewNotSaved] = "Ранги пешнамоиш дар нармафзор сабт намешавад. Содироти RGB565 PNG ин рангро дар бар мегирад.";
    s[(int)Str::LabelPreviewOnly] = "Танҳо барои пешнамоиш:";
    s[(int)Str::LabelPresets] = "Танзимоти пешакӣ...";
    s[(int)Str::BtnResetColor] = "Бозсозии ранг";
    s[(int)Str::LabelZoom] = "Миқёс";
    s[(int)Str::LabelPixelGrid] = "Тӯри пикселҳо";
    s[(int)Str::LabelHorizontalPreview] = "Пешнамоиши уфуқии хоно (136x22):";
    s[(int)Str::FontSec1Title] = "1. Интихоби шрифт";
    s[(int)Str::FontSec1WindowsFont] = "Шрифти Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Интихоби файли TTF/OTF...";
    s[(int)Str::FontSec1ActiveFont] = "Шрифти фаъол: %s";
    s[(int)Str::FontSec2Title] = "2. Қолаби намоиш";
    s[(int)Str::FontSec3Title] = "3. Параметрҳои рендер";
    s[(int)Str::FontSec3Threshold] = "Ҳадди равшанӣ";
    s[(int)Str::FontSec3ThresholdHint] = "Камтар = ғафс, Зиёдтар = борик";
    s[(int)Str::FontSec3Stretch] = "Кашидан дар ҳуҷайра";
    s[(int)Str::FontSec3Baseline] = "Ҳамоҳангсозӣ аз рӯи хати асосӣ";
    s[(int)Str::FontSec3CenterV] = "Марказонидани амудӣ";
    s[(int)Str::FontSec3PaddingX] = "Фосилаи уфуқӣ X";
    s[(int)Str::FontSec3PaddingY] = "Фосилаи амудӣ Y";
    s[(int)Str::FontSec3Oversampling] = "Суперсэмплинг";
    s[(int)Str::FontSec4Title] = "4. Воридкунӣ ба нармафзор";
    s[(int)Str::FontSec4ApplyPreset] = " > ТАТБИҚИ ГЛИФҲО БА НАЗМИ НАВ < ";
    s[(int)Str::FontSec4ApplyAll] = "Татбиқи ҳамаи шрифтҳо";
    s[(int)Str::FontSec5Title] = "5. Содирот ба PNG";
    s[(int)Str::FontSec5OutputFolder] = "Ҷузвдони содирот";
    s[(int)Str::FontSec5BrowseFolder] = "Интихоби ҷузвдон...";
    s[(int)Str::FontSec5OpenExplorer] = "Кушодан дар мудири файлҳо";
    s[(int)Str::FontSec5ExportAllPreset] = "Содироти ҳамаи қолабҳо";
    s[(int)Str::FontSec5ExportButton] = "Сабти файлҳои 1-битии PNG ба диск";
    s[(int)Str::FontRightLivePreview] = "Пешнамоиши зиндаи шрифт";
    s[(int)Str::FontRightNoGlyphs] = "Глифҳо бор нашудаанд.";
    s[(int)Str::FontPresetInfo] = "Қолаб: %s (%dx%d пкс, %d глиф, андоза: %.1f пкс)";
    s[(int)Str::FwInfoSpecTitle] = "Мушаххасоти бастаи VOOPOO DRAG S / DRAG X / DRAG 2";
    s[(int)Str::FwInfoNoFirmware] = "Нармафзор бор нашудааст. Аз бахши 'Студияи захираҳо' файли .zwdf/.xwdf-ро кушоед.";
    s[(int)Str::FwInfoFilePath] = "Роҳи файл: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256-и аслӣ: %s";
    s[(int)Str::FwInfoPackageSize] = "Андозаи баста: %zu байт";
    s[(int)Str::FwInfoHwRevisions] = "Версияҳои сахтафзор: %zu сабт";
    s[(int)Str::FwInfoRecognizedResources] = "Захираҳои шинохташуда: %zu (шрифтҳо, нишонаҳо, дараҷаҳо, навиштаҷот, экранҳо)";
    s[(int)Str::FwInfoRecordsTable] = "Ҷадвали сабтҳои сахтафзор";
    s[(int)Str::FwInfoColRecord] = "Сабти #";
    s[(int)Str::FwInfoColDevCode] = "Рамзи дастгоҳ";
    s[(int)Str::FwInfoColPayload] = "Андозаи бор";
    s[(int)Str::FwInfoColImage] = "Андозаи тасвир";
    s[(int)Str::FwInfoColFlashBase] = "Суроғаи асосии Flash";
    s[(int)Str::FwInfoColMSP] = "Нишондиҳандаи стек (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Вектори бозоғозӣ";
    s[(int)Str::BtnRevertAllModifications] = "Бекор кардани ҳамаи тағйирот";
    s[(int)Str::MsgReadyOpenFw] = "Омода. Файли .zwdf/.xwdf-ро кушоед ё шрифтҳои худро эҷод кунед.";
    s[(int)Str::MsgSuccessOpened] = "Кушода шуд: %s";
    s[(int)Str::MsgOpenError] = "Хатои кушодан: %s";
    s[(int)Str::MsgAllReverted] = "Ҳамаи тағйирот ба ҳолати аслии нармафзор баргардонида шуданд.";
    s[(int)Str::MsgExportSuccess] = "%d захира содирот шуд: %s";
    s[(int)Str::MsgExportError] = "Хатои содирот: %s";
    s[(int)Str::MsgImportSuccess] = "Аз ҷузвдон %d PNG ворид карда шуд.";
    s[(int)Str::MsgImportError] = "Хатои воридот: %s";
    s[(int)Str::MsgBuildSuccess] = "Сохтан БОМУВАФФАҚИЯТ: %s";
    s[(int)Str::MsgBuildError] = "Хатои сохтан: %s";
    s[(int)Str::MsgFontNotLoaded] = "Хато: Шрифт ба генератор бор нашудааст!";
    s[(int)Str::MsgFontInjectedPreset] = "%d глиф (%s андоза) ба хотираи нармафзор ворид карда шуд!";
    s[(int)Str::MsgFontInjectedAll] = "Глифҳои татбиқшуда: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d файли PNG бомуваффақият содирот шуд: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d файл барои %s бомуваффақият содирот шуд: %s";
    s[(int)Str::MsgFontExportError] = "Хатои содирот: %s";
    s[(int)Str::ModalBuildReportTitle] = "Ҳисоботи сохтан";
    s[(int)Str::ModalBuildSuccessBanner] = "СОХТАН ВА САНҶИШИ НАЗМИ НАВ БОМУВАФФАҚ ШУД!";
    s[(int)Str::ModalSavedFile] = "Файли сабтшуда: %s";
    s[(int)Str::ModalOutputSize] = "Андозаи баромад: %zu байт";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Захираҳои тасвирии тағйирёфта: %zu";
    s[(int)Str::ModalModColorGroups] = "Гурӯҳҳои рангии тағйирёфта: %zu";
    s[(int)Str::ModalPatchedRanges] = "Диапазонҳои таҳриршудаи байт: %zu дар %zu сабт";
    s[(int)Str::ModalUntouchedBytes] = "Байтҳои бетағйир монда: %zu (комилан ба асл мувофиқанд)";
    s[(int)Str::ModalReportNote] = "Ҳисоботи муфассал дар файли .report.json дар паҳлӯи нармафзор сабт шуд.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Кушодан дар мудири файлҳо";
    s[(int)Str::BtnCopyClipboard] = "Нусхабардории тасвир";
    s[(int)Str::BtnPasteClipboard] = "Гузоштани тасвир";
    s[(int)Str::BtnUndo] = "Бекор кардан";
    s[(int)Str::BtnRedo] = "Такрор кардан";
    s[(int)Str::TooltipUndo] = "Бекор кардан: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Такрор кардан: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Чизи бекоршаванда нест";
    s[(int)Str::TooltipRedoEmpty] = "Чизи такроршаванда нест";
    s[(int)Str::LabelPixelTools] = "Асбобҳои пикселӣ:";
    s[(int)Str::ToolPencil] = "Қалам";
    s[(int)Str::ToolEraser] = "Поккунак";
    s[(int)Str::BtnInvertPixels] = "Инверсия";
    s[(int)Str::BtnClearPixels] = "Тоза кардан";
    s[(int)Str::BtnFlipH] = "Чархиши уфуқӣ";
    s[(int)Str::BtnFlipV] = "Чархиши амудӣ";
    s[(int)Str::MsgCopiedToClipboard] = "Тасвири %s ба буфер нусхабардорӣ шуд.";
    s[(int)Str::MsgPastedFromClipboard] = "Тасвир аз буфер барои %s гузошта шуд!";
    s[(int)Str::MsgClipboardNoImage] = "Дар буфер ягон тасвир ёфт нашуд.";
    s[(int)Str::MsgUndoPerformed] = "Бекор карда шуд: %s";
    s[(int)Str::MsgRedoPerformed] = "Такрор карда шуд: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu сабт | %zu тағйирот";
    s[(int)Str::BadgeProjectSummary] = "| Лоиҳа: %s (%s) | %zu сабт | %zu тағйирот";
    s[(int)Str::LabelCategory] = "Гурӯҳбандӣ";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Гурӯҳ: %s  |  Ранг: Фармоишӣ (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Гурӯҳ: %s  |  Ранг: %s";
    s[(int)Str::LabelMetaHardware] = "Суроғаи Flash: 0x%08X  |  Андозаҳо: %u x %u пкс  |  Байтҳо: %zu  |  Сабтҳо: %zu";
    s[(int)Str::LabelPixelCoord] = "Пиксел: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Тугмаи чап: %s  |  Тугмаи рост: Пок кардан";
    s[(int)Str::LabelDrawAction] = "Кашидан";
    s[(int)Str::LabelEraseAction] = "Пок кардан";
    s[(int)Str::LabelFontPreset] = "Қолаб";
    s[(int)Str::LabelFontPixelGrid] = "Тӯри пикселҳо";
    s[(int)Str::MsgAppliedColorTheme] = "Мавзӯи рангии '%s' ба ҳамаи захираҳои мувофиқ татбиқ шуд.";
    s[(int)Str::MsgImportedMatchingPngs] = "Аз ҷузвдон %zu PNG-и мувофиқ ворид шуд.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Нармафзор тавассути Drag & Drop кушода шуд: %s";
    s[(int)Str::MsgDragDropError] = "Хатои Drag & Drop: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Шрифт тавассути Drag & Drop бор карда шуд: %s";
    s[(int)Str::MsgFailedLoadFont] = "Бор кардани файли шрифт муяссар нашуд: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "PNG-и интихобшуда барои %s ба аслӣ мувофиқ аст (бе тағйир).";
    s[(int)Str::MsgStagedReplacement] = "Ивазкунии PNG барои %s омода карда шуд!";
    s[(int)Str::MsgPngLoadError] = "Хатои боргирии PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Захираи ивазшудаи %s ба асл мувофиқ омад (тағйир наёфт).";
    s[(int)Str::MsgStagedReplacementCustom] = "Ивазкунии фармоишии PNG барои %s омода шуд!";
    s[(int)Str::MsgStagedRgbReplacement] = "Ивазкунии фармоишии RGB565 PNG барои %s омода шуд!";
    s[(int)Str::MsgRestoredOriginal] = "Захираи аслии нармафзор барои %s барқарор карда шуд.";
    s[(int)Str::MsgSavedAssetPng] = "%s ҳамчун %s сабт шуд";
    s[(int)Str::MsgSaveError] = "Хатои сабт: %s";
    s[(int)Str::MsgSavedBootLogo] = "Логотипи уфуқӣ ҳамчун %s сабт шуд";
    s[(int)Str::MsgRefuseOverwrite] = "Сабт рӯи файли аслии кушодашуда манъ аст. Лутфан роҳи дигарро интихоб кунед.";
    s[(int)Str::MsgColorPatchError] = "Хатои палитраи рангҳо: %s";
    s[(int)Str::MsgPackError] = "Хатои бастабандӣ: %s";
    s[(int)Str::MsgImportedChangedPngs] = "%zu PNG-и тағйирёфта ворид шуд (%zu ба асл мувофиқ омад).";
    s[(int)Str::LabelBuiltin] = "дарунсохт";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Нармафзори дарунсохтро бор кардан номумкин шуд: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Нармафзори дарунсохт бор шуд: %s";
    s[(int)Str::ThemeFactoryDefault] = "Заводии пешфарз";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Услуби аслии заводии Voopoo (сурх ва зард)";
    s[(int)Str::ThemeCyberpunkNeon] = "Киберпанк неон";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Сиани барқӣ, гулобии неон ва бунафш";
    s[(int)Str::ThemeMatrixGreen] = "Матритса сабз";
    s[(int)Str::ThemeMatrixGreenDesc] = "Фосфори терминали CRT ва зумуррад сабз";
    s[(int)Str::ThemeArcticIceBlue] = "Яхи арктикӣ";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Сиани пиряхӣ, осмони чуқур ва дурахши кабуд";
    s[(int)Str::ThemeAmberFire] = "Оташи қаҳрабо";
    s[(int)Str::ThemeAmberFireDesc] = "Лаваи норанҷӣ, шӯълаи тиллоӣ ва ахгар";
    s[(int)Str::ThemePurpleNebula] = "Туманоти бунафш";
    s[(int)Str::ThemePurpleNebulaDesc] = "Бунафши чуқур, лаванда ва аметисти неон";
    s[(int)Str::ThemeStealthDarkGray] = "Стелс хокистарии торик";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Графити хира, сланец ва металли силоҳ";
    s[(int)Str::RoleAccentRed] = "Аксент / ҳолати сурх";
    s[(int)Str::RoleAccentRedScope] = "Аксенти асосии UI дар ҳолатҳои стандартӣ — 8 ҷой";
    s[(int)Str::RoleAccentCyan] = "Аксент / ҳолати сиан";
    s[(int)Str::RoleAccentCyanScope] = "Аксенти асосии UI дар ҳолатҳои SMART/RBA — 7 ҷой";
    s[(int)Str::RoleHighlightYellow] = "Равшанкунӣ / зард";
    s[(int)Str::RoleHighlightYellowScope] = "Хати холҳо ва панелҳои хидматӣ — 4 ҷой";
    s[(int)Str::RoleRankBronze] = "Рутба / биринҷӣ";
    s[(int)Str::RoleRankBronzeScope] = "Палитраи рутбаи Биринҷӣ";
    s[(int)Str::RoleRankGold] = "Рутба / тилло";
    s[(int)Str::RoleRankGoldScope] = "Палитраи рутбаи Тилло";
    s[(int)Str::RoleRankIce] = "Рутба / платина (ях)";
    s[(int)Str::RoleRankIceScope] = "Палитраи рутбаи Платина / Ях";
    s[(int)Str::RoleRankCyan] = "Рутба / сиан";
    s[(int)Str::RoleRankCyanScope] = "Палитраи рутбаи Сиан";
    s[(int)Str::RoleRankOrange] = "Рутба / норанҷӣ";
    s[(int)Str::RoleRankOrangeScope] = "Палитраи рутбаи Норанҷӣ";
    s[(int)Str::RoleRankRed] = "Рутба / сурх";
    s[(int)Str::RoleRankRedScope] = "Палитраи рутбаи Сурх";
    s[(int)Str::RoleDigitsWattage] = "Рақамҳои тавоноӣ";
    s[(int)Str::RoleDigitsWattageScope] = "Нишондиҳандаи калони тавоноӣ дар экрани асосӣ (19x40)";
    s[(int)Str::RoleDigitsStats] = "Рақамҳои омор";
    s[(int)Str::ColorPresetWhite] = "Сафед (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Асосӣ / Сурх (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Сиан (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Хидматӣ / Зард (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Биринҷӣ (#B57947)";
    s[(int)Str::ColorPresetGold] = "Тилло (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Норанҷӣ (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Сабз (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Рақамҳои асосӣ 0-9, 10 файл)";
    s[(int)Str::FontPreset11x20] = "11x20 (Рақамҳои дуюмдараҷа 0-9, 10 файл)";
    s[(int)Str::FontPreset7x13] = "7x13 (Ҳолати хурд 0-9, A-F, 16 файл)";
    s[(int)Str::FontPreset6x10] = "6x10 (Микро ҳолат 0-9, 10 файл)";
    s[(int)Str::FontOs1x] = "1x (Мустақим)";
    s[(int)Str::FontOs2x] = "2x (Зуд)";
    s[(int)Str::FontOs4x] = "4x (Тавсияшуда)";
    s[(int)Str::FontOs8x] = "8x (Ультра)";
}

static void InitArmenian() {
    auto& s = g_table[(int)Language::Armenian];
    s[(int)Str::SupportFull] = "Լրիվ";
    s[(int)Str::SupportNone] = "Չի աջակցվում";
    s[(int)Str::SupportPalette] = "Գունապնակ";
    s[(int)Str::SupportMonochrome] = "Մոնոխրոմ էկրան. գունապնակ չկա։";
    s[(int)Str::EmulatorHistoryDay] = "Պատմության օրը";
    s[(int)Str::EmulatorClockHour] = "Ժամեր";
    s[(int)Str::EmulatorClockMinute] = "Րոպեներ";
    s[(int)Str::EmulatorClockField] = "Ընտրված դաշտ";
    s[(int)Str::MenuFile] = "Ֆայլ";
    s[(int)Str::MenuEdit] = "Խմբագրում";
    s[(int)Str::MenuLanguage] = "Լեզու";
    s[(int)Str::MenuHelp] = "Օգնություն";
    s[(int)Str::UpdateTitle] = "Ստուդիայի թարմացումներ";
    s[(int)Str::UpdateVersion] = "Տեղադրված տարբերակ՝ %s";
    s[(int)Str::UpdateCheck] = "Ստուգել թարմացումները";
    s[(int)Str::UpdateChecking] = "GitHub Releases-ի ստուգում...";
    s[(int)Str::UpdateCurrent] = "Տեղադրված է վերջին տարբերակը։";
    s[(int)Str::UpdateDownload] = "Ներբեռնել";
    s[(int)Str::UpdateDownloading] = "Ներբեռնում և ստուգում...";
    s[(int)Str::UpdateInstall] = "Տեղադրել և վերագործարկել";
    s[(int)Str::UpdateReady] = "Պատրաստ է տեղադրմանը։";
    s[(int)Str::UpdateChangelog] = "Փոփոխություններ";
    s[(int)Str::UpdateFailed] = "Թարմացման սխալ";
    s[(int)Str::UpdateAvailable] = "Հասանելի տարբերակ՝ %s";
    s[(int)Str::MenuCreate] = "Ստեղծել...";
    s[(int)Str::MenuOpen] = "Բացել...";
    s[(int)Str::MenuImport] = "Ներմուծել ռեսուրսներ...";
    s[(int)Str::MenuExport] = "Արտահանել ռեսուրսներ...";
    s[(int)Str::MenuBuild] = "Կազմել ներկառուցված ծրագիրը...";
    s[(int)Str::MenuExit] = "Ելք";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Ռեսուրսներ";
    s[(int)Str::TabFontGenerator] = "Տառատեսակների գեներատոր";
    s[(int)Str::TabFirmwareInfo] = "Ծրագրակազմի մասին";
    s[(int)Str::TabFirmwareComparison] = "Համեմատություն";
    s[(int)Str::CompareModeTwoFiles] = "Երկու ֆայլ";
    s[(int)Str::CompareModeEditor] = "Խմբագրիչ և ֆայլ";
    s[(int)Str::CompareLeft] = "Ձախ";
    s[(int)Str::CompareRight] = "Աջ";
    s[(int)Str::CompareChooseFile] = "Ընտրել ֆայլ...";
    s[(int)Str::CompareReadOnlyNote] = "Միայն դիտում․ խմբագրիչի փոփոխությունները ներառված են։";
    s[(int)Str::CompareChooseSources] = "Ընտրեք համեմատվող երկու աղբյուրները։";
    s[(int)Str::CompareEditorUnavailable] = "Խմբագրիչում ծրագրակազմ բացված չէ։";
    s[(int)Str::CompareChanged] = "Փոփոխված";
    s[(int)Str::CompareAdded] = "Ավելացված";
    s[(int)Str::CompareRemoved] = "Հեռացված";
    s[(int)Str::CompareEqual] = "Նույնը";
    s[(int)Str::CompareIncompatible] = "Անհամատեղելի";
    s[(int)Str::CompareDecodeError] = "Ապակոդավորման սխալ";
    s[(int)Str::CompareStatusFilter] = "Վիճակ";
    s[(int)Str::CompareDifferencePreview] = "Տարբերություն";
    s[(int)Str::CompareHighlightPixels] = "Ընդգծել տարբերությունները";
    s[(int)Str::CompareDifferentPixels] = "Տարբեր պիքսելներ՝ %zu / %zu";
    s[(int)Str::CompareMissingResource] = "Ռեսուրսը բացակայում է";
    s[(int)Str::CompareIncompatibleHint] = "Ռեսուրսների տեսակները կամ չափերը տարբեր են․ պիքսելային համեմատությունն անհասանելի է։";
    s[(int)Str::CompareImageUnavailable] = "Պատկերն անհասանելի է";
    s[(int)Str::CompareTextureError] = "Չհաջողվեց ստեղծել նախադիտման տեքստուրան։";
    s[(int)Str::CompareAllStatuses] = "Բոլոր վիճակները";
    s[(int)Str::TabFirmwareEmulator] = "Էմուլյատոր";
    s[(int)Str::TabFirmwareLayout] = "UI-ի դասավորություն";
    s[(int)Str::LayoutUnsupported] = "Այս ծրագրակազմի UI-ի դասավորության խմբագրումը հասանելի չէ։";
    s[(int)Str::LayoutNativeNote] = "Քաշեք տարրը կամ մուտքագրեք X/Y։ Նախադիտումը կատարում է փոփոխված ծրագրակազմը․ փոփոխությունները վերաբերում են ընտրված սարքային տարբերակին։";
    s[(int)Str::LayoutRevision] = "Սարքային տարբերակ";
    s[(int)Str::LayoutCheckingRevisions] = "Նույնական միջերեսների ստուգում…";
    s[(int)Str::LayoutAllRevisions] = "Բոլոր տարբերակները";
    s[(int)Str::LayoutRevisionGroup] = "Նույնական UI՝ %s";
    s[(int)Str::LayoutRevisionIndividual] = "Տարբերակ՝ %s";
    s[(int)Str::LayoutResetScope] = "Վերակայել ընտրված տարբերակները";
    s[(int)Str::LayoutSelectionUnavailable] = "Ընտրված բոլոր տարբերակներում համատեղելի տարր չկա։";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI-ի տարրեր";
    s[(int)Str::LayoutNoElements] = "Այս էկրանին տեղափոխվող բլոկներ չկան։";
    s[(int)Str::LayoutSharedBlock] = "Այս բլոկի կոորդինատները ընդհանուր են մի քանի էկրանների համար։";
    s[(int)Str::LayoutPositionX] = "X (պիքսել)";
    s[(int)Str::LayoutPositionY] = "Y (պիքսել)";
    s[(int)Str::LayoutOffset] = "Տեղաշարժ՝ %+d, %+d պիքսել";
    s[(int)Str::LayoutGridStep] = "Քայլ՝ %d × %d պիքսել";
    s[(int)Str::LayoutGridSpacing] = "Ցանցի քայլը (պիքսել)";
    s[(int)Str::LayoutGridConstraint] = "Եթե ցանցի կետը ներկայացնել հնարավոր չէ, առաջնային են կոորդինատների բնօրինակ քայլն ու սահմանները։";
    s[(int)Str::LayoutSnapGrid] = "Կցել ցանցին";
    s[(int)Str::LayoutSnapElements] = "Կցել տարրերի եզրերին և կենտրոններին";
    s[(int)Str::LayoutGuides] = "Տեղաշարժի ուղեցույցներ";
    s[(int)Str::LayoutAlign] = "Հավասարեցնել էկրանին";
    s[(int)Str::LayoutAlignLeft] = "Ձախ";
    s[(int)Str::LayoutAlignCenter] = "X կենտրոն";
    s[(int)Str::LayoutAlignRight] = "Աջ";
    s[(int)Str::LayoutAlignTop] = "Վերև";
    s[(int)Str::LayoutAlignMiddle] = "Y կենտրոն";
    s[(int)Str::LayoutAlignBottom] = "Ներքև";
    s[(int)Str::LayoutNudgeHint] = "Սլաքներ՝ բնօրինակ պիքսելային քայլ, Shift՝ ավելի մեծ քայլ։ Ֆոկուսի համար սեղմեք էկրանին։";
    s[(int)Str::LayoutOverlays] = "Տարրերի եզրագծեր";
    s[(int)Str::LayoutOverlapOverlay] = "Նշել հատումները";
    s[(int)Str::LayoutOverlaps] = "Հատվող տարրեր";
    s[(int)Str::LayoutNoOverlaps] = "Ուղղանկյունները չեն հատվում";
    s[(int)Str::LayoutDrawOrderUnknown] = "Բնօրինակ գծման հերթականությունը հաստատված չէ";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) գծվում է %s (#%d) տարրից ուշ";
    s[(int)Str::LayoutDrawOrderNote] = "Հիմնական վերագծման կանչերի հերթականությունն է, ոչ թե մշտական շերտերը։ Թարմացումների ժամանակ այն կարող է տարբերվել․ նախորդ բովանդակությունը ծածկում են միայն իրականում գրված պիքսելները։";
    s[(int)Str::LayoutDrawSequence] = "Գծման բնօրինակ հաջորդականություն";
    s[(int)Str::LayoutDrawInterleaved] = "Կանչերը հերթագայվում են․ ծածկումը կախված է տարրի մասից։";
    s[(int)Str::EmulatorBefore] = "Մինչև (բացված ծրագրակազմ)";
    s[(int)Str::EmulatorAfter] = "Հետո (ընթացիկ փոփոխություններ)";
    s[(int)Str::EmulatorSideBySide] = "Կողք կողքի";
    s[(int)Str::EmulatorOriginalError] = "Բացված ծրագրակազմի նախադիտման սխալ՝ %s";
    s[(int)Str::MenuSaveProject] = "Պահպանել նախագիծը";
    s[(int)Str::MenuSaveProjectAs] = "Պահպանել նախագիծը որպես...";
    s[(int)Str::MsgProjectSaved] = "Նախագիծը պահպանված է՝ %s";
    s[(int)Str::MenuBuild] = "Արտահանել ծրագրակազմը";
    s[(int)Str::BtnOpenFirmware] = "Բացել ծրագրակազմ կամ նախագիծ...";
    s[(int)Str::StartupOpenFile] = "Բացել ծրագրակազմ կամ նախագիծ...";
    s[(int)Str::UnsavedPrompt] = "Շարունակելուց առաջ պահպանե՞լ նախագիծը։";
    s[(int)Str::UnsavedPrompt] = "Շարունակելուց առաջ պահպանե՞լ նախագիծը։";
    s[(int)Str::LayoutBounds] = "X՝ %d…%d / Y՝ %d…%d";
    s[(int)Str::LayoutResetElement] = "Վերակայել տարրը";
    s[(int)Str::LayoutResetRevision] = "Վերակայել այս տարբերակի դասավորությունը";
    s[(int)Str::LayoutResetAll] = "Վերակայել դասավորության բոլոր փոփոխությունները";
    s[(int)Str::LayoutPatchSites] = "Փոփոխությունների հասցեներ";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Նախադիտման վիճակ";
    s[(int)Str::LayoutMoveElement] = "Տեղափոխել UI-ի տարրը";
    s[(int)Str::LayoutRba] = "RBA ռեժիմ";
    s[(int)Str::LayoutTemperatureControl] = "Ջերմաստիճանի կառավարում";
    s[(int)Str::LayoutTemperature] = "Ջերմաստիճան (°C)";
    s[(int)Str::LayoutRenderFailed] = "Բնօրինակ կադրի բուֆերը հասանելի չէ։";
    s[(int)Str::LayoutElementPower] = "Հզորություն";
    s[(int)Str::LayoutElementBattery] = "Մարտկոց";
    s[(int)Str::LayoutElementResistance] = "Դիմադրություն";
    s[(int)Str::LayoutElementDuration] = "Տևողություն";
    s[(int)Str::LayoutElementPuffs] = "Ներշնչումներ";
    s[(int)Str::LayoutElementMode] = "Ռեժիմ";
    s[(int)Str::LayoutElementStatus] = "Վիճակ";
    s[(int)Str::LayoutElementScore] = "Միավորներ";
    s[(int)Str::LayoutElementStatistics] = "Վիճակագրություն";
    s[(int)Str::LayoutElementVoltage] = "Լարում";
    s[(int)Str::LayoutElementGraph] = "Գրաֆիկ";
    s[(int)Str::LayoutElementRank] = "Աստիճան";
    s[(int)Str::LayoutElementModeSelection] = "Ռեժիմի ընտրություն";
    s[(int)Str::LayoutElementLogo] = "Լոգոտիպ";
    s[(int)Str::LayoutChangedElements] = "Փոփոխված UI տարրեր";
    s[(int)Str::EmulatorSupported] = "Բոլոր ներկառուցված DRAG ծրագրակազմերի բնօրինակ UI-ի նախադիտում։";
    s[(int)Str::EmulatorNativeNote] = "Բնօրինակ գծագրման կոդը՝ ընթացիկ փոփոխություններով։ Էլեկտրական մասը չի էմուլացվում։";
    s[(int)Str::EmulatorWatts] = "Հզորություն (Վտ)";
    s[(int)Str::EmulatorPuffs] = "Քաշումներ";
    s[(int)Str::EmulatorBattery] = "Լիցք (0–5)";
    s[(int)Str::EmulatorResistance] = "Դիմադրություն (մՕմ)";
    s[(int)Str::EmulatorDuration] = "Տևողություն (0,01 վ)";
    s[(int)Str::EmulatorLocked] = "Կողպված";
    s[(int)Str::EmulatorCharging] = "Լիցքավորում";
    s[(int)Str::EmulatorScore] = "Ընդհանուր միավոր";
    s[(int)Str::EmulatorScoreDisplay] = "Լավագույն արդյունք";
    s[(int)Str::EmulatorRedraw] = "Վերագծել";
    s[(int)Str::EmulatorLandscape] = "Հորիզոնական";
    s[(int)Str::EmulatorScreen] = "Էկրան";
    s[(int)Str::EmulatorMainScreen] = "Գլխավոր էկրան";
    s[(int)Str::FontSystemFonts] = "Համակարգային տառատեսակներ";
    s[(int)Str::FontUserFonts] = "Օգտատիրոջ տառատեսակներ";
    s[(int)Str::EmulatorMenuItem] = "Ընտրացանկի կետ";
    s[(int)Str::EmulatorMaterial] = "Նյութ";
    s[(int)Str::EmulatorMenuConfirmed] = "Հաստատված";
    s[(int)Str::EmulatorResources] = "Ծրագրակազմի կարդացած ռեսուրսները";
    s[(int)Str::EmulatorNoResources] = "Կատալոգի ռեսուրսների ընթերցումներ չեն գրանցվել։";
    s[(int)Str::EmulatorUidNote] = "Էմուլացված MCU-ի UID, ոչ միացված սարքի ID-ն։";
    s[(int)Str::EmulatorLoaderVersion] = "Վիրտուալ բեռնիչի տարբերակ";
    s[(int)Str::EmulatorLoaderId] = "Վիրտուալ CHIP-ID բայթեր";
    s[(int)Str::EmulatorLoaderNote] = "Բեռնիչի տվյալները էմուլացվում են, ոչ թե կարդացվում ծրագրակազմից կամ սարքից։";
    s[(int)Str::TabFirmwarePatches] = "Կարկատումներ";
    s[(int)Str::PatchBootLogo] = "Գործարկման էկրան";
    s[(int)Str::PatchShutdownLogo] = "Անջատման էկրան";
    s[(int)Str::PatchDeviceInfo] = "CHIP ID / տարբերակի էկրան";
    s[(int)Str::PatchDeviceInfoNote] = "Միայն անշարժ պատկեր։ Փոխարինում է սպասարկման էկրանը, ոչ թե հզորության և մարտկոցի գլխավոր էկրանը։";
    s[(int)Str::PatchLogoNote] = "Անկախ 80 × 160 պատկերներ բոլոր DRAG S Score / Factory տարբերակների համար։ Սարքի վրա չի ստուգվել։";
    s[(int)Str::PatchUnsupported] = "Այս կարկատումը պահանջում է DRAG S Score կամ Factory ծրագրակազմ։";
    s[(int)Str::PatchEnabled] = "Միացնել սեփական պատկերը";
    s[(int)Str::PatchCanvas] = "Կոմպոզիցիա · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Էկրանի բնիկ նախադիտում";
    s[(int)Str::PatchPreviewNote] = "Բնիկ նախադիտումը ցույց է տալիս կիրառված կարգավորումները, ոչ թե կոմպոզիցիայի սևագիրը։";
    s[(int)Str::PatchFit] = "Տեղավորել";
    s[(int)Str::PatchCover] = "Լրացնել";
    s[(int)Str::PatchStretch] = "Ձգել";
    s[(int)Str::PatchScale] = "Մասշտաբ";
    s[(int)Str::PatchAnimation] = "Հայտնվելու անիմացիա";
    s[(int)Str::PatchNone] = "Չկա";
    s[(int)Str::PatchFade] = "Աստիճանական հայտնվել";
    s[(int)Str::PatchDuration] = "Տևողություն (մվ)";
    s[(int)Str::PatchAutoHardware] = "Ինքնաշխատ";
    s[(int)Str::PatchAnimationPhase] = "Անիմացիայի փուլ";
    s[(int)Str::PatchPlay] = "Նվագարկել";
    s[(int)Str::EmulatorModeScreens] = "Էկրաններ";
    s[(int)Str::EmulatorModeDevice] = "Սարք";
    s[(int)Str::DeviceUnsupported] = "Սարքի ռեժիմն աջակցում է DRAG S Score / HW 3103։";
    s[(int)Str::DeviceNote] = "Ծրագրակազմն աշխատում է Reset-ից՝ վիրտուալ ծայրամասային սարքերով։ Տաքացումը մոդելավորվում է, էլեկտրական անվտանգությունը չի ստուգվում։";
    s[(int)Str::DeviceReset] = "Վերագործարկել";
    s[(int)Str::DevicePause] = "Դադար";
    s[(int)Str::DeviceResume] = "Սկսել";
    s[(int)Str::DeviceStep] = "Քայլ";
    s[(int)Str::DeviceChanged] = "Ընթացիկ փոփոխությունները դեռ բեռնված չեն։";
    s[(int)Str::DeviceApply] = "Վերագործարկել փոփոխություններով";
    s[(int)Str::DeviceButtons] = "Պահեք կոճակները կամ բացատ / ↑ / ↓։";
    s[(int)Str::DeviceAtomizerConnected] = "Պարույրը միացված է";
    s[(int)Str::DeviceTime] = "Վիրտուալ ժամանակ";
    s[(int)Str::DeviceSleeping] = "Պրոցեսորը քնած է";
    s[(int)Str::DeviceLoading] = "Սարքը բեռնվում է...";
    s[(int)Str::DeviceLog] = "Ծայրամասային դիմումներ";
    s[(int)Str::BadgeNoFirmware] = "| [Ծրագրակազմը բացված չէ - Քաշեք .zwdf/.xwdf ֆայլն այստեղ]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu տարր | %zu գրառում";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Բացել ծրագրակազմը...";
    s[(int)Str::BtnExportAll] = "Արտահանել բոլոր ռեսուրսները պանակ...";
    s[(int)Str::BtnImportFolder] = "Ներմուծել PNG պանակ...";
    s[(int)Str::BtnBuildFirmware] = " ՀԱՎԱՔԵԼ ՆՈՐ ԾՐԱԳՐԱԿԱԶՄ ";
    s[(int)Str::BtnRevertAllStaged] = "Չեղարկել բոլոր փոփոխությունները";
    s[(int)Str::GroupAll] = "Բոլոր խմբերը";
    s[(int)Str::GroupAlerts] = "Ծանուցումներ (Երկխոսություններ և պատկերակներ)";
    s[(int)Str::GroupFonts] = "Տառատեսակներ (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Միավորներ և աստիճաններ";
    s[(int)Str::GroupStatusLabels] = "Կարգավիճակ և ինտերֆեյսի տեքստեր";
    s[(int)Str::GroupControls] = "Կառավարում և մարտկոց";
    s[(int)Str::GroupScreens] = "Էկրաններ և բեռնման լոգոներ";
    s[(int)Str::GroupAnimations] = "Անիմացիաներ";
    s[(int)Str::PanelResourcesHeader] = "Ռեսուրսներ";
    s[(int)Str::LabelSearch] = "Որոնում";
    s[(int)Str::SearchPlaceholder] = "ըստ անվան, id-ի, հասցեի...";
    s[(int)Str::NoFirmwareLoadedList] = "Ծրագրակազմ բեռնված չէ:\nՖայլ > Ստեղծել / Բացել";
    s[(int)Str::NoResourcesMatchFilter] = "Որոնման ֆիլտրին համապատասխան ռեսուրսներ չկան:";
    s[(int)Str::PanelResourcePreviewHeader] = "Նախադիտում";
    s[(int)Str::SelectResourcePrompt] = "Դիտելու և խմբագրելու համար ձախ ցանկից ընտրեք ռեսուրս:";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Խումբ";
    s[(int)Str::LabelColor] = "Գույն";
    s[(int)Str::LabelColorCustom] = "Հատուկ";
    s[(int)Str::LabelFlashAddress] = "Flash հասցե";
    s[(int)Str::LabelDimensions] = "Չափսեր";
    s[(int)Str::LabelBytes] = "Բայթեր";
    s[(int)Str::LabelRecords] = "Գրառումներ";
    s[(int)Str::StatusModifiedStaged] = "Կարգավիճակ: ՓՈՓՈԽՎԱԾ (պատրաստված է հիշողության մեջ)";
    s[(int)Str::StatusOriginal] = "Կարգավիճակ: ԲՆՕՐԻՆԱԿ (ծրագրակազմից)";
    s[(int)Str::StatusStockOriginal] = "Կարգավիճակ: ԳՈՐԾԱՐԱՆԱՅԻՆ (համապատասխանում է A050/E050-ին)";
    s[(int)Str::StatusCustomDiffers] = "Կարգավիճակ: ՓՈՓՈԽՎԱԾ (տարբերվում է գործարանայինից)";
    s[(int)Str::BtnRestoreStock] = "Վերականգնել գործարանային տարբերակը";
    s[(int)Str::LabelCompareWithStock] = "Համեմատություն գործարանային ծրագրակազմի հետ:";
    s[(int)Str::LabelActivePreview] = "Ընթացիկ ակտիվ:";
    s[(int)Str::LabelStockReferencePreview] = "Գործարանային էտալոն:";
    s[(int)Str::FilterOnlyCustom] = "Ցուցադրել միայն փոփոխված ռեսուրսները";
    s[(int)Str::MsgRestoredToStock] = "'%s'-ը վերականգնվեց գործարանային էտալոնի:";
    s[(int)Str::BtnReplaceFromPng] = "Փոխարինել PNG-ից...";
    s[(int)Str::BtnRestoreOriginal] = "Վերականգնել բնօրինակը";
    s[(int)Str::BtnExportThisPng] = "Արտահանել այս PNG-ն...";
    s[(int)Str::BtnExportHorizontal] = "Արտահանել հորիզոնական (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Գունային գունապնակ և ռենդերեր (RGB565)";
    s[(int)Str::LabelTheme] = "Թեմա";
    s[(int)Str::BtnApply] = "Կիրառել";
    s[(int)Str::BtnResetAll] = "Վերականգնել բոլորը";
    s[(int)Str::LabelRole] = "Դեր";
    s[(int)Str::BtnResetRole] = "Վերականգնել դերը";
    s[(int)Str::PromptLoadFwColors] = "Գույները փոխելու համար բեռնեք ծրագրակազմը:";
    s[(int)Str::LabelPreviewTint] = "Նախադիտման երանգ";
    s[(int)Str::InfoDigitsMonochrome] = "Թվերը մոնոխրոմ դիմակներ են: Դրանց գույնը սահմանվում է ծրագրային կոդով:";
    s[(int)Str::InfoPreviewNotSaved] = "Նախադիտման երանգը չի պահպանվում ծրագրակազմում: RGB565 PNG արտահանումը ներառում է այն:";
    s[(int)Str::LabelPreviewOnly] = "Միայն նախադիտման համար:";
    s[(int)Str::LabelPresets] = "Նախադրվածքներ...";
    s[(int)Str::BtnResetColor] = "Վերականգնել գույնը";
    s[(int)Str::LabelZoom] = "Մասշտաբ";
    s[(int)Str::LabelPixelGrid] = "Պիքսելային ցանց";
    s[(int)Str::LabelHorizontalPreview] = "Ընթեռնելի հորիզոնական նախադիտում (136x22):";
    s[(int)Str::FontSec1Title] = "1. Տառատեսակի ընտրություն";
    s[(int)Str::FontSec1WindowsFont] = "Windows տառատեսակ";
    s[(int)Str::FontSec1BrowseTtf] = "Ընտրել TTF/OTF ֆայլ...";
    s[(int)Str::FontSec1ActiveFont] = "Ակտիվ տառատեսակ: %s";
    s[(int)Str::FontSec2Title] = "2. Ցուցադրման նախադրվածք";
    s[(int)Str::FontSec3Title] = "3. Ռենդերման պարամետրեր";
    s[(int)Str::FontSec3Threshold] = "Պայծառության շեմ";
    s[(int)Str::FontSec3ThresholdHint] = "Ցածր = թավ, Բարձր = բարակ";
    s[(int)Str::FontSec3Stretch] = "Ձգել վանդակի չափով";
    s[(int)Str::FontSec3Baseline] = "Հավասարեցում ըստ բազային գծի";
    s[(int)Str::FontSec3CenterV] = "Ուղղահայաց կենտրոնացում";
    s[(int)Str::FontSec3PaddingX] = "Հորիզոնական հեռավորություն X";
    s[(int)Str::FontSec3PaddingY] = "Ուղղահայաց հեռավորություն Y";
    s[(int)Str::FontSec3Oversampling] = "Oversampling";
    s[(int)Str::FontSec4Title] = "4. Ինտեգրում ծրագրակազմի մեջ";
    s[(int)Str::FontSec4ApplyPreset] = " > ԿԻՐԱՌԵԼ ԳԼԻՖՆԵՐԸ ԲԱՑՎԱԾ ԾՐԱԳՐԱԿԱԶՄԻՆ < ";
    s[(int)Str::FontSec4ApplyAll] = "Կիրառել բոլոր տառատեսակները";
    s[(int)Str::FontSec5Title] = "5. Արտահանում PNG ֆորմատով";
    s[(int)Str::FontSec5OutputFolder] = "Ելքային պանակ";
    s[(int)Str::FontSec5BrowseFolder] = "Ընտրել պանակ...";
    s[(int)Str::FontSec5OpenExplorer] = "Բացել ֆայլային մենեջերում";
    s[(int)Str::FontSec5ExportAllPreset] = "Արտահանել բոլոր նախադրվածքները";
    s[(int)Str::FontSec5ExportButton] = "Պահպանել 1-բիթանոց PNG ֆայլերը";
    s[(int)Str::FontRightLivePreview] = "Տառատեսակի նախադիտում";
    s[(int)Str::FontRightNoGlyphs] = "Գլիֆներ բեռնված չեն:";
    s[(int)Str::FontPresetInfo] = "Կաղապար: %s (%dx%d px, %d գլիֆ, չափ: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 փաթեթի բնութագիր";
    s[(int)Str::FwInfoNoFirmware] = "Ծրագրակազմ բեռնված չէ: Բացեք .zwdf/.xwdf ֆայլը 'Ռեսուրսների ստուդիա' բաժնում:";
    s[(int)Str::FwInfoFilePath] = "Ֆայլի ուղին: %s";
    s[(int)Str::FwInfoSha256] = "Սկզբնական SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "Փաթեթի չափսը: %zu բայթ";
    s[(int)Str::FwInfoHwRevisions] = "Սարքակազմի տարբերակներ: %zu գրառում";
    s[(int)Str::FwInfoRecognizedResources] = "Ճանաչված ռեսուրսներ: %zu (տառատեսակներ, պատկերակներ, էկրաններ)";
    s[(int)Str::FwInfoRecordsTable] = "Սարքակազմի գրառումների աղյուսակ";
    s[(int)Str::FwInfoColRecord] = "Գրառում #";
    s[(int)Str::FwInfoColDevCode] = "Սարքի կոդ";
    s[(int)Str::FwInfoColPayload] = "Բեռի չափ";
    s[(int)Str::FwInfoColImage] = "Պատկերի չափ";
    s[(int)Str::FwInfoColFlashBase] = "Flash բազային հասցե";
    s[(int)Str::FwInfoColMSP] = "Ստեկի ցուցիչ (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Վերագործարկման վեկտոր";
    s[(int)Str::BtnRevertAllModifications] = "Չեղարկել բոլոր փոփոխությունները";
    s[(int)Str::MsgReadyOpenFw] = "Պատրաստ է: Բացեք .zwdf/.xwdf ֆայլ կամ ստեղծեք հատուկ տառատեսակներ:";
    s[(int)Str::MsgSuccessOpened] = "Բացվեց՝ %s";
    s[(int)Str::MsgOpenError] = "Բացման սխալ: %s";
    s[(int)Str::MsgAllReverted] = "Բոլոր փոփոխությունները վերադարձվեցին սկզբնական վիճակին:";
    s[(int)Str::MsgExportSuccess] = "Արտահանվեց %d ռեսուրս դեպի: %s";
    s[(int)Str::MsgExportError] = "Արտահանման սխալ: %s";
    s[(int)Str::MsgImportSuccess] = "Պանակից ներմուծվեց %d PNG:";
    s[(int)Str::MsgImportError] = "Ներմուծման սխալ: %s";
    s[(int)Str::MsgBuildSuccess] = "Հավաքումը ՀԱՋՈՂՎԵՑ: %s";
    s[(int)Str::MsgBuildError] = "Հավաքման սխալ: %s";
    s[(int)Str::MsgFontNotLoaded] = "Սխալ: Տառատեսակ բեռնված չէ գեներատորի մեջ:";
    s[(int)Str::MsgFontInjectedPreset] = "%d գլիֆ (%s չափ) ներդրվեց ծրագրակազմի հիշողության մեջ:";
    s[(int)Str::MsgFontInjectedAll] = "Կիրառված նշաններ՝ %d։";
    s[(int)Str::MsgFontExportSuccess] = "%d PNG ֆայլ բարեհաջող արտահանվեց դեպի: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d ֆայլ %s-ի համար բարեհաջող արտահանվեց դեպի: %s";
    s[(int)Str::MsgFontExportError] = "Արտահանման սխալ: %s";
    s[(int)Str::ModalBuildReportTitle] = "Հավաքման հաշվետվություն";
    s[(int)Str::ModalBuildSuccessBanner] = "ԾՐԱԳՐԱԿԱԶՄԻ ՀԱՎԱՔՈՒՄԸ ԵՎ ՍՏՈՒԳՈՒՄԸ ՀԱՋՈՂՎԵՑ:";
    s[(int)Str::ModalSavedFile] = "Պահպանված ֆայլ: %s";
    s[(int)Str::ModalOutputSize] = "Ելքային չափս: %zu բայթ";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Փոփոխված պատկերներ: %zu";
    s[(int)Str::ModalModColorGroups] = "Փոփոխված գունային խմբեր: %zu";
    s[(int)Str::ModalPatchedRanges] = "Փոփոխված բայթային տիրույթներ: %zu (%zu գրառումներում)";
    s[(int)Str::ModalUntouchedBytes] = "Անփոփոխ մաքուր բայթեր: %zu (նույնական են բնօրինակին)";
    s[(int)Str::ModalReportNote] = "Մանրամասն հաշվետվությունը պահպանվեց .report.json ֆայլում:";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Բացել ֆայլային մենեջերում";
    s[(int)Str::BtnCopyClipboard] = "Պատճենել պատկերը";
    s[(int)Str::BtnPasteClipboard] = "Տեղադրել պատկերը";
    s[(int)Str::BtnUndo] = "Հետարկել";
    s[(int)Str::BtnRedo] = "Կրկնել";
    s[(int)Str::TooltipUndo] = "Հետարկել: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Կրկնել: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Հետարկելու ոչինչ չկա";
    s[(int)Str::TooltipRedoEmpty] = "Կրկնելու ոչինչ չկա";
    s[(int)Str::LabelPixelTools] = "Պիքսելային գործիքներ:";
    s[(int)Str::ToolPencil] = "Մատիտ";
    s[(int)Str::ToolEraser] = "Ռետին";
    s[(int)Str::BtnInvertPixels] = "Շրջել";
    s[(int)Str::BtnClearPixels] = "Մաքրել";
    s[(int)Str::BtnFlipH] = "Շրջել հորիզոնական";
    s[(int)Str::BtnFlipV] = "Շրջել ուղղահայաց";
    s[(int)Str::MsgCopiedToClipboard] = "%s պատկերը պատճենվեց փոխանակման բուֆեր:";
    s[(int)Str::MsgPastedFromClipboard] = "Պատկերը փոխանակման բուֆերից տեղադրվեց %s-ի համար:";
    s[(int)Str::MsgClipboardNoImage] = "Փոխանակման բուֆերում պատկեր չկա:";
    s[(int)Str::MsgUndoPerformed] = "Հետարկվեց: %s";
    s[(int)Str::MsgRedoPerformed] = "Կրկնվեց: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu գրառում | %zu փոփոխություն";
    s[(int)Str::BadgeProjectSummary] = "| Նախագիծ: %s (%s) | %zu գրառում | %zu փոփոխություն";
    s[(int)Str::LabelCategory] = "Կատեգորիա";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Խումբ: %s  |  Գույն: Հատուկ (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Խումբ: %s  |  Գույն: %s";
    s[(int)Str::LabelMetaHardware] = "Flash հասցե: 0x%08X  |  Չափսեր: %u x %u px  |  Բայթեր: %zu  |  Գրառումներ: %zu";
    s[(int)Str::LabelPixelCoord] = "Պիքսել: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Ձախ սեղմում: %s  |  Աջ սեղմում: Ջնջել";
    s[(int)Str::LabelDrawAction] = "Նկարել";
    s[(int)Str::LabelEraseAction] = "Ջնջել";
    s[(int)Str::LabelFontPreset] = "Կաղապար";
    s[(int)Str::LabelFontPixelGrid] = "Պիքսելային ցանց";
    s[(int)Str::MsgAppliedColorTheme] = "'%s' գունային թեման կիրառվեց համապատասխան ռեսուրսների համար:";
    s[(int)Str::MsgImportedMatchingPngs] = "Պանակից ներմուծվեց %zu համապատասխան PNG:";
    s[(int)Str::MsgOpenedFwDragDrop] = "Ծրագրակազմը բացվեց Drag & Drop-ով: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop սխալ: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Տառատեսակը բեռնվեց Drag & Drop-ով: %s";
    s[(int)Str::MsgFailedLoadFont] = "Չհաջողվեց բեռնել տառատեսակի ֆայլը: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "%s-ի համար ընտրված PNG-ն նույնական է բնօրինակին:";
    s[(int)Str::MsgStagedReplacement] = "%s-ի համար PNG փոխարինումը պատրաստ է:";
    s[(int)Str::MsgPngLoadError] = "PNG բեռնման սխալ: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Փոխարինված %s-ը համընկավ բնօրինակին:";
    s[(int)Str::MsgStagedReplacementCustom] = "%s-ի համար հատուկ PNG փոխարինումը պատրաստ է:";
    s[(int)Str::MsgStagedRgbReplacement] = "%s-ի համար հատուկ RGB565 PNG փոխարինումը պատրաստ է:";
    s[(int)Str::MsgRestoredOriginal] = "%s-ի բնօրինակ ռեսուրսը վերականգնվեց:";
    s[(int)Str::MsgSavedAssetPng] = "Պահպանվեց %s որպես %s";
    s[(int)Str::MsgSaveError] = "Պահպանման սխալ: %s";
    s[(int)Str::MsgSavedBootLogo] = "Հորիզոնական լոգոն պահպանվեց որպես %s";
    s[(int)Str::MsgRefuseOverwrite] = "Սկզբնական ֆայլի վրա գրելն արգելված է: Ընտրեք այլ ուղի:";
    s[(int)Str::MsgColorPatchError] = "Գունային սխալ: %s";
    s[(int)Str::MsgPackError] = "Փաթեթավորման սխալ: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Ներմուծվեց %zu փոփոխված PNG (%zu բնօրինակին համընկավ):";
    s[(int)Str::LabelBuiltin] = "ներկառուցված";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Չհաջողվեց բեռնել ներկառուցված ծրագրակազմը: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Ներկառուցված ծրագրակազմը բեռնվեց: %s";
    s[(int)Str::ThemeFactoryDefault] = "Գործարանային լռելյայն";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Voopoo բնօրինակ կարմիր և դեղին ոճ";
    s[(int)Str::ThemeCyberpunkNeon] = "Կիբերպանկ նեոն";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Էլեկտրական ցիան, նեոն վարդագույն և մանուշակագույն";
    s[(int)Str::ThemeMatrixGreen] = "Մատրիցա կանաչ";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT տերմինալի ֆոսֆոր և զմրուխտ կանաչ";
    s[(int)Str::ThemeArcticIceBlue] = "Արկտիկական սառույց";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Սառցադաշտային ցիան, խորը երկինք և լազուր փայլ";
    s[(int)Str::ThemeAmberFire] = "Սաթե կրակ";
    s[(int)Str::ThemeAmberFireDesc] = "Լավայի նարնջագույն, ոսկե բոց և կայծ";
    s[(int)Str::ThemePurpleNebula] = "Մանուշակագույն միգամածություն";
    s[(int)Str::ThemePurpleNebulaDesc] = "Խորը մանուշակագույն, լավանդա և նեոն ամեթիստ";
    s[(int)Str::ThemeStealthDarkGray] = "Ստելս մուգ մոխրագույն";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Մատ գրաֆիտ, թերթաքար և զենքի մետաղ";
    s[(int)Str::RoleAccentRed] = "Շեշտ / կարմիր ռեժիմ";
    s[(int)Str::RoleAccentRedScope] = "Հիմնական UI շեշտ ստանդարտ ռեժիմներում — 8 տեղ";
    s[(int)Str::RoleAccentCyan] = "Շեշտ / ցիան ռեժիմ";
    s[(int)Str::RoleAccentCyanScope] = "Հիմնական UI շեշտ SMART/RBA վիճակներում — 7 տեղ";
    s[(int)Str::RoleHighlightYellow] = "Ընդգծում / դեղին";
    s[(int)Str::RoleHighlightYellowScope] = "Միավորների գիծ և սպասարկման վահանակներ — 4 տեղ";
    s[(int)Str::RoleRankBronze] = "Աստիճան / բրոնզ";
    s[(int)Str::RoleRankBronzeScope] = "Բրոնզե աստիճանի գունապնակ";
    s[(int)Str::RoleRankGold] = "Աստիճան / ոսկի";
    s[(int)Str::RoleRankGoldScope] = "Ոսկե աստիճանի գունապնակ";
    s[(int)Str::RoleRankIce] = "Աստիճան / պլատին (սառույց)";
    s[(int)Str::RoleRankIceScope] = "Պլատին / Սառույց աստիճանի գունապնակ";
    s[(int)Str::RoleRankCyan] = "Աստիճան / ցիան";
    s[(int)Str::RoleRankCyanScope] = "Ցիան աստիճանի գունապնակ";
    s[(int)Str::RoleRankOrange] = "Աստիճան / նարնջագույն";
    s[(int)Str::RoleRankOrangeScope] = "Նարնջագույն աստիճանի գունապնակ";
    s[(int)Str::RoleRankRed] = "Աստիճան / կարմիր";
    s[(int)Str::RoleRankRedScope] = "Կարմիր աստիճանի գունապնակ";
    s[(int)Str::RoleDigitsWattage] = "Հզորության թվեր";
    s[(int)Str::RoleDigitsWattageScope] = "Հզորության մեծ ցուցանիշ գլխավոր էկրանին (19x40)";
    s[(int)Str::RoleDigitsStats] = "Վիճակագրության թվեր";
    s[(int)Str::ColorPresetWhite] = "Սպիտակ (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Հիմնական / Կարմիր (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Ցիան (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Սպասարկման / Դեղին (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Բրոնզ (#B57947)";
    s[(int)Str::ColorPresetGold] = "Ոսկի (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Նարնջագույն (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Կանաչ (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Հիմնական թվեր 0-9, 10 ֆայլ)";
    s[(int)Str::FontPreset11x20] = "11x20 (Երկրորդական թվեր 0-9, 10 ֆայլ)";
    s[(int)Str::FontPreset7x13] = "7x13 (Փոքր կարգավիճակ 0-9, A-F, 16 ֆայլ)";
    s[(int)Str::FontPreset6x10] = "6x10 (Միկրո կարգավիճակ 0-9, 10 ֆայլ)";
    s[(int)Str::FontOs1x] = "1x (Ուղիղ)";
    s[(int)Str::FontOs2x] = "2x (Արագ)";
    s[(int)Str::FontOs4x] = "4x (Առաջարկվող)";
    s[(int)Str::FontOs8x] = "8x (Ուլտրա)";
}

static void InitGeorgian() {
    auto& s = g_table[(int)Language::Georgian];
    s[(int)Str::SupportFull] = "სრული";
    s[(int)Str::SupportNone] = "არ არის მხარდაჭერილი";
    s[(int)Str::SupportPalette] = "ფერების პალიტრა";
    s[(int)Str::SupportMonochrome] = "მონოქრომული ეკრანი; ფერების პალიტრა არ არის.";
    s[(int)Str::EmulatorHistoryDay] = "ისტორიის დღე";
    s[(int)Str::EmulatorClockHour] = "საათი";
    s[(int)Str::EmulatorClockMinute] = "წუთი";
    s[(int)Str::EmulatorClockField] = "არჩეული ველი";
    s[(int)Str::MenuFile] = "ფაილი";
    s[(int)Str::MenuEdit] = "რედაქტირება";
    s[(int)Str::MenuLanguage] = "ენა";
    s[(int)Str::MenuHelp] = "დახმარება";
    s[(int)Str::UpdateTitle] = "სტუდიის განახლებები";
    s[(int)Str::UpdateVersion] = "დაყენებული ვერსია: %s";
    s[(int)Str::UpdateCheck] = "განახლებების შემოწმება";
    s[(int)Str::UpdateChecking] = "GitHub Releases-ის შემოწმება...";
    s[(int)Str::UpdateCurrent] = "დაყენებულია ბოლო ვერსია.";
    s[(int)Str::UpdateDownload] = "ჩამოტვირთვა";
    s[(int)Str::UpdateDownloading] = "ჩამოტვირთვა და შემოწმება...";
    s[(int)Str::UpdateInstall] = "დაყენება და გადატვირთვა";
    s[(int)Str::UpdateReady] = "მზადაა დასაყენებლად.";
    s[(int)Str::UpdateChangelog] = "ცვლილებები";
    s[(int)Str::UpdateFailed] = "განახლების შეცდომა";
    s[(int)Str::UpdateAvailable] = "ხელმისაწვდომი ვერსია: %s";
    s[(int)Str::MenuCreate] = "შექმნა...";
    s[(int)Str::MenuOpen] = "გახსნა...";
    s[(int)Str::MenuImport] = "რესურსების იმპორტი...";
    s[(int)Str::MenuExport] = "რესურსების ექსპორტი...";
    s[(int)Str::MenuBuild] = "მიკროპროგრამის აწყობა...";
    s[(int)Str::MenuExit] = "გასვლა";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "რესურსები";
    s[(int)Str::TabFontGenerator] = "შრიფტის გენერატორი";
    s[(int)Str::TabFirmwareInfo] = "პროგრამის შესახებ";
    s[(int)Str::TabFirmwareComparison] = "შედარება";
    s[(int)Str::CompareModeTwoFiles] = "ორი ფაილი";
    s[(int)Str::CompareModeEditor] = "რედაქტორი და ფაილი";
    s[(int)Str::CompareLeft] = "მარცხენა";
    s[(int)Str::CompareRight] = "მარჯვენა";
    s[(int)Str::CompareChooseFile] = "ფაილის არჩევა...";
    s[(int)Str::CompareReadOnlyNote] = "მხოლოდ ნახვა; რედაქტორის ცვლილებები გათვალისწინებულია.";
    s[(int)Str::CompareChooseSources] = "აირჩიეთ ორივე წყარო შესადარებლად.";
    s[(int)Str::CompareEditorUnavailable] = "რედაქტორში პროგრამა არ არის გახსნილი.";
    s[(int)Str::CompareChanged] = "შეცვლილი";
    s[(int)Str::CompareAdded] = "დამატებული";
    s[(int)Str::CompareRemoved] = "წაშლილი";
    s[(int)Str::CompareEqual] = "ერთნაირი";
    s[(int)Str::CompareIncompatible] = "შეუთავსებელი";
    s[(int)Str::CompareDecodeError] = "დეკოდირების შეცდომა";
    s[(int)Str::CompareStatusFilter] = "მდგომარეობა";
    s[(int)Str::CompareDifferencePreview] = "სხვაობა";
    s[(int)Str::CompareHighlightPixels] = "განსხვავებების გამოყოფა";
    s[(int)Str::CompareDifferentPixels] = "განსხვავებული პიქსელები: %zu / %zu";
    s[(int)Str::CompareMissingResource] = "რესურსი არ არის";
    s[(int)Str::CompareIncompatibleHint] = "რესურსების ტიპები ან ზომები განსხვავდება: პიქსელური შედარება მიუწვდომელია.";
    s[(int)Str::CompareImageUnavailable] = "სურათი მიუწვდომელია";
    s[(int)Str::CompareTextureError] = "წინასწარი ნახვის ტექსტურა ვერ შეიქმნა.";
    s[(int)Str::CompareAllStatuses] = "ყველა მდგომარეობა";
    s[(int)Str::TabFirmwareEmulator] = "ემულატორი";
    s[(int)Str::TabFirmwareLayout] = "UI განლაგება";
    s[(int)Str::LayoutUnsupported] = "ამ პროგრამისთვის UI განლაგების რედაქტირება მიუწვდომელია.";
    s[(int)Str::LayoutNativeNote] = "გადაათრიეთ ელემენტი ან შეიყვანეთ X/Y. წინასწარი ხედი შეცვლილ პროგრამას ასრულებს; ცვლილებები არჩეულ აპარატურულ რევიზიას ეხება.";
    s[(int)Str::LayoutRevision] = "აპარატურული რევიზია";
    s[(int)Str::LayoutCheckingRevisions] = "იდენტური ინტერფეისების შემოწმება…";
    s[(int)Str::LayoutAllRevisions] = "ყველა რევიზია";
    s[(int)Str::LayoutRevisionGroup] = "იდენტური UI: %s";
    s[(int)Str::LayoutRevisionIndividual] = "რევიზია: %s";
    s[(int)Str::LayoutResetScope] = "არჩეული რევიზიების აღდგენა";
    s[(int)Str::LayoutSelectionUnavailable] = "ყველა არჩეულ რევიზიაში თავსებადი ელემენტი არ არის.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "UI ელემენტები";
    s[(int)Str::LayoutNoElements] = "ამ ეკრანზე გადასაადგილებელი ბლოკები არ არის.";
    s[(int)Str::LayoutSharedBlock] = "ამ ბლოკის კოორდინატები რამდენიმე ეკრანისთვის საერთოა.";
    s[(int)Str::LayoutPositionX] = "X (პიქსელი)";
    s[(int)Str::LayoutPositionY] = "Y (პიქსელი)";
    s[(int)Str::LayoutOffset] = "წანაცვლება: %+d, %+d პიქს.";
    s[(int)Str::LayoutGridStep] = "ბიჯი: %d × %d პიქს.";
    s[(int)Str::LayoutGridSpacing] = "ბადის ბიჯი (პიქს.)";
    s[(int)Str::LayoutGridConstraint] = "თუ ბადის წერტილის წარმოდგენა შეუძლებელია, უპირატესობა აქვს კოორდინატების საწყის ბიჯსა და საზღვრებს.";
    s[(int)Str::LayoutSnapGrid] = "ბადეზე მიბმა";
    s[(int)Str::LayoutSnapElements] = "ელემენტების კიდეებსა და ცენტრებზე მიბმა";
    s[(int)Str::LayoutGuides] = "გადაადგილების დამხმარე ხაზები";
    s[(int)Str::LayoutAlign] = "ეკრანზე გასწორება";
    s[(int)Str::LayoutAlignLeft] = "მარცხნივ";
    s[(int)Str::LayoutAlignCenter] = "X ცენტრი";
    s[(int)Str::LayoutAlignRight] = "მარჯვნივ";
    s[(int)Str::LayoutAlignTop] = "ზემოთ";
    s[(int)Str::LayoutAlignMiddle] = "Y ცენტრი";
    s[(int)Str::LayoutAlignBottom] = "ქვემოთ";
    s[(int)Str::LayoutNudgeHint] = "ისრები: პიქსელის საწყისი ბიჯი; Shift: დიდი ბიჯი. ფოკუსისთვის დააწკაპუნეთ ეკრანზე.";
    s[(int)Str::LayoutOverlays] = "ელემენტების კონტურები";
    s[(int)Str::LayoutOverlapOverlay] = "გადაფარვების მონიშვნა";
    s[(int)Str::LayoutOverlaps] = "გადამკვეთი ელემენტები";
    s[(int)Str::LayoutNoOverlaps] = "მართკუთხედები არ იკვეთება";
    s[(int)Str::LayoutDrawOrderUnknown] = "ხატვის საწყისი მიმდევრობა დაუდასტურებელია";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) იხატება %s (#%d) ელემენტზე მოგვიანებით";
    s[(int)Str::LayoutDrawOrderNote] = "ეს ძირითადი გადახატვის გამოძახებების რიგია და არა მუდმივი ფენები. განახლებისას ის შეიძლება განსხვავდებოდეს; წინა შიგთავსს მხოლოდ რეალურად ჩაწერილი პიქსელები ფარავს.";
    s[(int)Str::LayoutDrawSequence] = "ხატვის საწყისი მიმდევრობა";
    s[(int)Str::LayoutDrawInterleaved] = "გამოძახებები მონაცვლეობს; გადაფარვა ელემენტის ნაწილზეა დამოკიდებული.";
    s[(int)Str::EmulatorBefore] = "მანამდე (გახსნილი პროგრამა)";
    s[(int)Str::EmulatorAfter] = "შემდეგ (მიმდინარე ცვლილებები)";
    s[(int)Str::EmulatorSideBySide] = "გვერდიგვერდ";
    s[(int)Str::EmulatorOriginalError] = "გახსნილი პროგრამის გადახედვის შეცდომა: %s";
    s[(int)Str::MenuSaveProject] = "პროექტის შენახვა";
    s[(int)Str::MenuSaveProjectAs] = "პროექტის შენახვა როგორც...";
    s[(int)Str::MsgProjectSaved] = "პროექტი შენახულია: %s";
    s[(int)Str::MenuBuild] = "პროგრამის ექსპორტი";
    s[(int)Str::BtnOpenFirmware] = "პროგრამის ან პროექტის გახსნა...";
    s[(int)Str::StartupOpenFile] = "პროგრამის ან პროექტის გახსნა...";
    s[(int)Str::UnsavedPrompt] = "გაგრძელებამდე შეინახოთ პროექტი?";
    s[(int)Str::UnsavedPrompt] = "გაგრძელებამდე შეინახოთ პროექტი?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "ელემენტის აღდგენა";
    s[(int)Str::LayoutResetRevision] = "ამ რევიზიის განლაგების აღდგენა";
    s[(int)Str::LayoutResetAll] = "განლაგების ყველა ცვლილების აღდგენა";
    s[(int)Str::LayoutPatchSites] = "პატჩების მისამართები";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "წინასწარი ხედის მდგომარეობა";
    s[(int)Str::LayoutMoveElement] = "UI ელემენტის გადაადგილება";
    s[(int)Str::LayoutRba] = "RBA რეჟიმი";
    s[(int)Str::LayoutTemperatureControl] = "ტემპერატურის კონტროლი";
    s[(int)Str::LayoutTemperature] = "ტემპერატურა (°C)";
    s[(int)Str::LayoutRenderFailed] = "ორიგინალი კადრის ბუფერი მიუწვდომელია.";
    s[(int)Str::LayoutElementPower] = "სიმძლავრე";
    s[(int)Str::LayoutElementBattery] = "ბატარეა";
    s[(int)Str::LayoutElementResistance] = "წინაღობა";
    s[(int)Str::LayoutElementDuration] = "ხანგრძლივობა";
    s[(int)Str::LayoutElementPuffs] = "ნაფაზები";
    s[(int)Str::LayoutElementMode] = "რეჟიმი";
    s[(int)Str::LayoutElementStatus] = "მდგომარეობა";
    s[(int)Str::LayoutElementScore] = "ქულა";
    s[(int)Str::LayoutElementStatistics] = "სტატისტიკა";
    s[(int)Str::LayoutElementVoltage] = "ძაბვა";
    s[(int)Str::LayoutElementGraph] = "გრაფიკი";
    s[(int)Str::LayoutElementRank] = "რანგი";
    s[(int)Str::LayoutElementModeSelection] = "რეჟიმის არჩევა";
    s[(int)Str::LayoutElementLogo] = "ლოგოტიპი";
    s[(int)Str::LayoutChangedElements] = "შეცვლილი UI ელემენტები";
    s[(int)Str::EmulatorSupported] = "ყველა ჩაშენებული DRAG პროგრამის ორიგინალი UI-ის გადახედვა.";
    s[(int)Str::EmulatorNativeNote] = "ორიგინალი ხატვის კოდი მიმდინარე ცვლილებებით. ელექტრული ნაწილი არ ემულირდება.";
    s[(int)Str::EmulatorWatts] = "სიმძლავრე (ვტ)";
    s[(int)Str::EmulatorPuffs] = "ჩასუნთქვები";
    s[(int)Str::EmulatorBattery] = "მუხტი (0–5)";
    s[(int)Str::EmulatorResistance] = "წინაღობა (მΩ)";
    s[(int)Str::EmulatorDuration] = "ხანგრძლივობა (0,01 წმ)";
    s[(int)Str::EmulatorLocked] = "ჩაკეტილი";
    s[(int)Str::EmulatorCharging] = "იტენება";
    s[(int)Str::EmulatorScore] = "სულ ქულა";
    s[(int)Str::EmulatorScoreDisplay] = "საუკეთესო შედეგი";
    s[(int)Str::EmulatorRedraw] = "თავიდან დახატვა";
    s[(int)Str::EmulatorLandscape] = "ჰორიზონტალური";
    s[(int)Str::EmulatorScreen] = "ეკრანი";
    s[(int)Str::EmulatorMainScreen] = "მთავარი ეკრანი";
    s[(int)Str::FontSystemFonts] = "სისტემური შრიფტები";
    s[(int)Str::FontUserFonts] = "მომხმარებლის შრიფტები";
    s[(int)Str::EmulatorMenuItem] = "მენიუს პუნქტი";
    s[(int)Str::EmulatorMaterial] = "მასალა";
    s[(int)Str::EmulatorMenuConfirmed] = "დადასტურებულია";
    s[(int)Str::EmulatorResources] = "პროგრამის მიერ წაკითხული რესურსები";
    s[(int)Str::EmulatorNoResources] = "კატალოგის რესურსების წაკითხვა არ დაფიქსირდა.";
    s[(int)Str::EmulatorUidNote] = "ემულირებული MCU UID, არა მიერთებული მოწყობილობის ID.";
    s[(int)Str::EmulatorLoaderVersion] = "ვირტუალური ჩამტვირთავის ვერსია";
    s[(int)Str::EmulatorLoaderId] = "ვირტუალური CHIP-ID ბაიტები";
    s[(int)Str::EmulatorLoaderNote] = "ჩამტვირთავის მონაცემები ემულირებულია და არა პროგრამიდან ან მოწყობილობიდან წაკითხული.";
    s[(int)Str::TabFirmwarePatches] = "პატჩები";
    s[(int)Str::PatchBootLogo] = "ჩართვის ეკრანი";
    s[(int)Str::PatchShutdownLogo] = "გამორთვის ეკრანი";
    s[(int)Str::PatchDeviceInfo] = "CHIP ID / ვერსიის ეკრანი";
    s[(int)Str::PatchDeviceInfoNote] = "მხოლოდ სტატიკური სურათი. ცვლის სერვისის ეკრანს და არა სიმძლავრისა და ბატარეის მთავარ ეკრანს.";
    s[(int)Str::PatchLogoNote] = "დამოუკიდებელი 80 × 160 სურათები DRAG S Score / Factory-ის ყველა რევიზიისთვის. მოწყობილობაზე არ შემოწმებულა.";
    s[(int)Str::PatchUnsupported] = "ამ პატჩს სჭირდება DRAG S Score ან Factory პროგრამა.";
    s[(int)Str::PatchEnabled] = "საკუთარი ლოგოს ჩართვა";
    s[(int)Str::PatchCanvas] = "კომპოზიცია · 80 × 160";
    s[(int)Str::PatchNativePreview] = "ეკრანის ნატიური ხედი";
    s[(int)Str::PatchPreviewNote] = "ნატიური ხედი აჩვენებს გამოყენებულ პარამეტრებს და არა კომპოზიციის მონახაზს.";
    s[(int)Str::PatchFit] = "ჩატევა";
    s[(int)Str::PatchCover] = "შევსება";
    s[(int)Str::PatchStretch] = "გაჭიმვა";
    s[(int)Str::PatchScale] = "მასშტაბი";
    s[(int)Str::PatchAnimation] = "გამოჩენის ანიმაცია";
    s[(int)Str::PatchNone] = "არცერთი";
    s[(int)Str::PatchFade] = "თანდათან გამოჩენა";
    s[(int)Str::PatchDuration] = "ხანგრძლივობა (მწმ)";
    s[(int)Str::PatchAutoHardware] = "ავტომატური";
    s[(int)Str::PatchAnimationPhase] = "ანიმაციის ფაზა";
    s[(int)Str::PatchPlay] = "დაკვრა";
    s[(int)Str::EmulatorModeScreens] = "ეკრანები";
    s[(int)Str::EmulatorModeDevice] = "მოწყობილობა";
    s[(int)Str::DeviceUnsupported] = "მოწყობილობის რეჟიმი მხარს უჭერს DRAG S Score / HW 3103-ს.";
    s[(int)Str::DeviceNote] = "პროგრამა სრულდება Reset-იდან ვირტუალური პერიფერიით. გათბობა მოდელირდება; ელექტრული უსაფრთხოება არ მოწმდება.";
    s[(int)Str::DeviceReset] = "გადატვირთვა";
    s[(int)Str::DevicePause] = "პაუზა";
    s[(int)Str::DeviceResume] = "გაშვება";
    s[(int)Str::DeviceStep] = "ნაბიჯი";
    s[(int)Str::DeviceChanged] = "მიმდინარე ცვლილებები ჯერ არ ჩატვირთულა.";
    s[(int)Str::DeviceApply] = "ცვლილებებით გადატვირთვა";
    s[(int)Str::DeviceButtons] = "გეჭიროთ ღილაკები ან გამოტოვება / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "სპირალი მიერთებულია";
    s[(int)Str::DeviceTime] = "ვირტუალური დრო";
    s[(int)Str::DeviceSleeping] = "პროცესორს სძინავს";
    s[(int)Str::DeviceLoading] = "მოწყობილობა იტვირთება...";
    s[(int)Str::DeviceLog] = "პერიფერიაზე წვდომა";
    s[(int)Str::BadgeNoFirmware] = "| [პროგრამა არ არის გახსნილი - ჩააგდეთ .zwdf/.xwdf ფაილი აქ]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu ელემენტი | %zu ჩანაწერი";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "პროგრამის გახსნა...";
    s[(int)Str::BtnExportAll] = "ყველა რესურსის ექსპორტი საქაღალდეში...";
    s[(int)Str::BtnImportFolder] = "PNG საქაღალდის იმპორტი...";
    s[(int)Str::BtnBuildFirmware] = " ახალი პროგრამის აწყობა ";
    s[(int)Str::BtnRevertAllStaged] = "ყველა ცვლილების გაუქმება";
    s[(int)Str::GroupAll] = "ყველა ჯგუფი";
    s[(int)Str::GroupAlerts] = "შეტყობინებები (დიალოგები და ხატულები)";
    s[(int)Str::GroupFonts] = "შრიფტები (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "ქულები და რანგები";
    s[(int)Str::GroupStatusLabels] = "სტატუსი და UI ტექსტები";
    s[(int)Str::GroupControls] = "მართვა და ელემენტი";
    s[(int)Str::GroupScreens] = "ეკრანები და ჩატვირთვის ლოგოები";
    s[(int)Str::GroupAnimations] = "ანიმაციები";
    s[(int)Str::PanelResourcesHeader] = "რესურსები";
    s[(int)Str::LabelSearch] = "ძიება";
    s[(int)Str::SearchPlaceholder] = "სახელით, id-ით, მისამართით...";
    s[(int)Str::NoFirmwareLoadedList] = "პროგრამა არ არის ჩატვირთული.\nფაილი > შექმნა / გახსნა";
    s[(int)Str::NoResourcesMatchFilter] = "ფილტრის შესაბამისი რესურსები ვერ მოიძებნა.";
    s[(int)Str::PanelResourcePreviewHeader] = "წინასწარი ნახვა";
    s[(int)Str::SelectResourcePrompt] = "აირჩიეთ რესურსი მარცხენა სიიდან სანახავად და რედაქტირებისთვის.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "ჯგუფი";
    s[(int)Str::LabelColor] = "ფერი";
    s[(int)Str::LabelColorCustom] = "მორგებული";
    s[(int)Str::LabelFlashAddress] = "Flash მისამართი";
    s[(int)Str::LabelDimensions] = "ზომები";
    s[(int)Str::LabelBytes] = "ბაიტები";
    s[(int)Str::LabelRecords] = "ჩანაწერები";
    s[(int)Str::StatusModifiedStaged] = "სტატუსი: შეცვლილია (მომზადებულია მეხსიერებაში)";
    s[(int)Str::StatusOriginal] = "სტატუსი: ორიგინალი (პროგრამიდან)";
    s[(int)Str::StatusStockOriginal] = "სტატუსი: ქარხნული ორიგინალი (A050/E050 ეტალონი)";
    s[(int)Str::StatusCustomDiffers] = "სტატუსი: მოდიფიცირებული (განსხვავდება ქარხნულისგან)";
    s[(int)Str::BtnRestoreStock] = "ქარხნული ვერსიის აღდგენა";
    s[(int)Str::LabelCompareWithStock] = "შედარება ქარხნულ ეტალონთან:";
    s[(int)Str::LabelActivePreview] = "მიმდინარე აქტიური:";
    s[(int)Str::LabelStockReferencePreview] = "ქარხნული ეტალონი:";
    s[(int)Str::FilterOnlyCustom] = "მხოლოდ შეცვლილი რესურსების ჩვენება";
    s[(int)Str::MsgRestoredToStock] = "'%s' აღდგა ქარხნულ ეტალონამდე.";
    s[(int)Str::BtnReplaceFromPng] = "PNG-ით ჩანაცვლება...";
    s[(int)Str::BtnRestoreOriginal] = "ორიგინალის აღდგენა";
    s[(int)Str::BtnExportThisPng] = "ამ PNG-ის ექსპორტი...";
    s[(int)Str::BtnExportHorizontal] = "ჰორიზონტალური ექსპორტი (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "ფერების პალიტრა და რენდერი (RGB565)";
    s[(int)Str::LabelTheme] = "თემა";
    s[(int)Str::BtnApply] = "გამოყენება";
    s[(int)Str::BtnResetAll] = "ყველას დაბრუნება";
    s[(int)Str::LabelRole] = "როლი";
    s[(int)Str::BtnResetRole] = "როლის განულება";
    s[(int)Str::PromptLoadFwColors] = "ჩატვირთეთ პროგრამა ფერების სანახავად და შესაცვლელად.";
    s[(int)Str::LabelPreviewTint] = "წინასწარი ნახვის ელფერი";
    s[(int)Str::InfoDigitsMonochrome] = "ციფრები მონოქრომული ნიღბებია. მათი ფერი განისაზღვრება კოდით.";
    s[(int)Str::InfoPreviewNotSaved] = "ეს ელფერი არ ინახება პროგრამაში. RGB565 PNG ექსპორტი შეიცავს ამ ელფერს.";
    s[(int)Str::LabelPreviewOnly] = "მხოლოდ წინასწარი ნახვისთვის:";
    s[(int)Str::LabelPresets] = "პრესეტები...";
    s[(int)Str::BtnResetColor] = "ფერის განულება";
    s[(int)Str::LabelZoom] = "მასშტაბი";
    s[(int)Str::LabelPixelGrid] = "პიქსელების ბადე";
    s[(int)Str::LabelHorizontalPreview] = "კითხვადი ჰორიზონტალური ნახვა (136x22):";
    s[(int)Str::FontSec1Title] = "1. შრიფტის არჩევა";
    s[(int)Str::FontSec1WindowsFont] = "Windows შრიფტი";
    s[(int)Str::FontSec1BrowseTtf] = "TTF/OTF ფაილის არჩევა...";
    s[(int)Str::FontSec1ActiveFont] = "აქტიური შრიფტი: %s";
    s[(int)Str::FontSec2Title] = "2. ჩვენების შაბლონი";
    s[(int)Str::FontSec3Title] = "3. რენდერის პარამეტრები";
    s[(int)Str::FontSec3Threshold] = "სიკაშკაშის ზღვარი";
    s[(int)Str::FontSec3ThresholdHint] = "დაბალი = მსხვილი, მაღალი = თხელი";
    s[(int)Str::FontSec3Stretch] = "უჯრის ზომაზე გაწელვა";
    s[(int)Str::FontSec3Baseline] = "საბაზისო ხაზზე გასწორება";
    s[(int)Str::FontSec3CenterV] = "ვერტიკალური ცენტრირება";
    s[(int)Str::FontSec3PaddingX] = "ჰორიზონტალური დაშორება X";
    s[(int)Str::FontSec3PaddingY] = "ვერტიკალური დაშორება Y";
    s[(int)Str::FontSec3Oversampling] = "Oversampling";
    s[(int)Str::FontSec4Title] = "4. პროგრამაში ინტეგრაცია";
    s[(int)Str::FontSec4ApplyPreset] = " > გლიფების გამოყენება გახსნილ პროგრამაზე < ";
    s[(int)Str::FontSec4ApplyAll] = "ყველა შრიფტის გამოყენება";
    s[(int)Str::FontSec5Title] = "5. PNG ექსპორტი";
    s[(int)Str::FontSec5OutputFolder] = "გამოსატანი საქაღალდე";
    s[(int)Str::FontSec5BrowseFolder] = "საქაღალდის არჩევა...";
    s[(int)Str::FontSec5OpenExplorer] = "ფაილ მენეჯერში გახსნა";
    s[(int)Str::FontSec5ExportAllPreset] = "ყველა შაბლონის ექსპორტი";
    s[(int)Str::FontSec5ExportButton] = "1-ბიტიანი PNG ფაილების შენახვა";
    s[(int)Str::FontRightLivePreview] = "შრიფტის ცოცხალი ნახვა";
    s[(int)Str::FontRightNoGlyphs] = "გლიფები არ არის ჩატვირთული.";
    s[(int)Str::FontPresetInfo] = "შაბლონი: %s (%dx%d px, %d გლიფი, ზომა: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "VOOPOO DRAG S / DRAG X / DRAG 2 პაკეტის სპეციფიკაცია";
    s[(int)Str::FwInfoNoFirmware] = "პროგრამა არ არის ჩატვირთული. გახსენით .zwdf/.xwdf ფაილი 'რესურსების სტუდიაში'.";
    s[(int)Str::FwInfoFilePath] = "ფაილის გზა: %s";
    s[(int)Str::FwInfoSha256] = "საწყისი SHA-256: %s";
    s[(int)Str::FwInfoPackageSize] = "პაკეტის ზომა: %zu ბაიტი";
    s[(int)Str::FwInfoHwRevisions] = "აპარატურული ვერსიები: %zu ჩანაწერი";
    s[(int)Str::FwInfoRecognizedResources] = "ამოცნობილი რესურსები: %zu (შრიფტები, ხატულები, ეკრანები)";
    s[(int)Str::FwInfoRecordsTable] = "აპარატურული ჩანაწერების ცხრილი";
    s[(int)Str::FwInfoColRecord] = "ჩანაწერი #";
    s[(int)Str::FwInfoColDevCode] = "მოწყობილობის კოდი";
    s[(int)Str::FwInfoColPayload] = "დატვირთვის ზომა";
    s[(int)Str::FwInfoColImage] = "გამოსახულების ზომა";
    s[(int)Str::FwInfoColFlashBase] = "Flash ბაზური მისამართი";
    s[(int)Str::FwInfoColMSP] = "სტეკის მაჩვენებელი (MSP)";
    s[(int)Str::FwInfoColResetVector] = "გადატვირთვის ვექტორი";
    s[(int)Str::BtnRevertAllModifications] = "ყველა ცვლილების გაუქმება";
    s[(int)Str::MsgReadyOpenFw] = "მზადაა. გახსენით .zwdf/.xwdf ფაილი ან შექმენით შრიფტები.";
    s[(int)Str::MsgSuccessOpened] = "გაიხსნა: %s";
    s[(int)Str::MsgOpenError] = "გახსნის შეცდომა: %s";
    s[(int)Str::MsgAllReverted] = "ყველა ცვლილება დაბრუნდა საწყის მდგომარეობაში.";
    s[(int)Str::MsgExportSuccess] = "ექსპორტირებულია %d რესურსი: %s";
    s[(int)Str::MsgExportError] = "ექსპორტის შეცდომა: %s";
    s[(int)Str::MsgImportSuccess] = "იმპორტირებულია %d PNG საქაღალდიდან.";
    s[(int)Str::MsgImportError] = "იმპორტის შეცდომა: %s";
    s[(int)Str::MsgBuildSuccess] = "აწყობა წარმატებულია: %s";
    s[(int)Str::MsgBuildError] = "აწყობის შეცდომა: %s";
    s[(int)Str::MsgFontNotLoaded] = "შეცდომა: შრიფტი არ არის ჩატვირთული გენერატორში!";
    s[(int)Str::MsgFontInjectedPreset] = "%d გლიფი (%s ზომა) ჩაიწერა პროგრამის მეხსიერებაში!";
    s[(int)Str::MsgFontInjectedAll] = "გამოყენებული გლიფები: %d.";
    s[(int)Str::MsgFontExportSuccess] = "%d PNG ფაილი წარმატებით ექსპორტირდა: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "%d ფაილი %s-ისთვის წარმატებით ექსპორტირდა: %s";
    s[(int)Str::MsgFontExportError] = "ექსპორტის შეცდომა: %s";
    s[(int)Str::ModalBuildReportTitle] = "აწყობის ანგარიში";
    s[(int)Str::ModalBuildSuccessBanner] = "პროგრამის აწყობა და შემოწმება წარმატებით დასრულდა!";
    s[(int)Str::ModalSavedFile] = "შენახული ფაილი: %s";
    s[(int)Str::ModalOutputSize] = "გამოსული ზომა: %zu ბაიტი";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "შეცვლილი სურათები: %zu";
    s[(int)Str::ModalModColorGroups] = "შეცვლილი ფერთა ჯგუფები: %zu";
    s[(int)Str::ModalPatchedRanges] = "შესწორებული ბაიტების დიაპაზონი: %zu (%zu ჩანაწერში)";
    s[(int)Str::ModalUntouchedBytes] = "უხლებელი ბაიტები: %zu (იდენტურია ორიგინალის)";
    s[(int)Str::ModalReportNote] = "დეტალური ანგარიში შენახულია .report.json ფაილში პროგრამის გვერდით.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "ფაილ მენეჯერში გახსნა";
    s[(int)Str::BtnCopyClipboard] = "სურათის კოპირება";
    s[(int)Str::BtnPasteClipboard] = "სურათის ჩასმა";
    s[(int)Str::BtnUndo] = "გაუქმება";
    s[(int)Str::BtnRedo] = "გამეორება";
    s[(int)Str::TooltipUndo] = "გაუქმება: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "გამეორება: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "გასაუქმებელი არაფერია";
    s[(int)Str::TooltipRedoEmpty] = "გასამეორებელი არაფერია";
    s[(int)Str::LabelPixelTools] = "პიქსელის ხელსაწყოები:";
    s[(int)Str::ToolPencil] = "ფანქარი";
    s[(int)Str::ToolEraser] = "საშლელი";
    s[(int)Str::BtnInvertPixels] = "ინვერსია";
    s[(int)Str::BtnClearPixels] = "გასუფთავება";
    s[(int)Str::BtnFlipH] = "ჰორიზონტალურად შებრუნება";
    s[(int)Str::BtnFlipV] = "ვერტიკალურად შებრუნება";
    s[(int)Str::MsgCopiedToClipboard] = "%s სურათი კოპირებულია ბუფერში.";
    s[(int)Str::MsgPastedFromClipboard] = "სურათი ბუფერიდან ჩასმულია %s-ისთვის!";
    s[(int)Str::MsgClipboardNoImage] = "ბუფერში სურათი ვერ მოიძებნა.";
    s[(int)Str::MsgUndoPerformed] = "გაუქმდა: %s";
    s[(int)Str::MsgRedoPerformed] = "განმეორდა: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu ჩანაწერი | %zu ცვლილება";
    s[(int)Str::BadgeProjectSummary] = "| პროექტი: %s (%s) | %zu ჩანაწერი | %zu ცვლილება";
    s[(int)Str::LabelCategory] = "კატეგორია";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  ჯგუფი: %s  |  ფერი: მორგებული (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  ჯგუფი: %s  |  ფერი: %s";
    s[(int)Str::LabelMetaHardware] = "Flash მისამართი: 0x%08X  |  ზომები: %u x %u px  |  ბაიტები: %zu  |  ჩანაწერები: %zu";
    s[(int)Str::LabelPixelCoord] = "პიქსელი: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| მარცხენა ღილაკი: %s  |  მარჯვენა ღილაკი: წაშლა";
    s[(int)Str::LabelDrawAction] = "დახატვა";
    s[(int)Str::LabelEraseAction] = "წაშლა";
    s[(int)Str::LabelFontPreset] = "შაბლონი";
    s[(int)Str::LabelFontPixelGrid] = "შრიფტის ბადე";
    s[(int)Str::MsgAppliedColorTheme] = "'%s' ფერის თემა გამოყენებულია შესაბამის რესურსებზე.";
    s[(int)Str::MsgImportedMatchingPngs] = "საქაღალდიდან იმპორტირდა %zu შესაბამისი PNG.";
    s[(int)Str::MsgOpenedFwDragDrop] = "პროგრამა გაიხსნა Drag & Drop-ით: %s";
    s[(int)Str::MsgDragDropError] = "Drag & Drop შეცდომა: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "შრიფტი ჩაიტვირთა Drag & Drop-ით: %s";
    s[(int)Str::MsgFailedLoadFont] = "შრიფტის ფაილის ჩატვირთვა ვერ მოხერხდა: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "%s-სთვის არჩეული PNG ემთხვევა ორიგინალს (უცვლელია).";
    s[(int)Str::MsgStagedReplacement] = "%s-სთვის PNG ჩანაცვლება მომზადებულია!";
    s[(int)Str::MsgPngLoadError] = "PNG ჩატვირთვის შეცდომა: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "%s ჩანაცვლებული რესურსი დაემთხვა ორიგინალს.";
    s[(int)Str::MsgStagedReplacementCustom] = "%s-სთვის სპეციალური PNG ჩანაცვლება მომზადებულია!";
    s[(int)Str::MsgStagedRgbReplacement] = "%s-სთვის სპეციალური RGB565 PNG ჩანაცვლება მომზადებულია!";
    s[(int)Str::MsgRestoredOriginal] = "%s-სთვის ორიგინალი რესურსი აღდგენილია.";
    s[(int)Str::MsgSavedAssetPng] = "%s შენახულია როგორც %s";
    s[(int)Str::MsgSaveError] = "შენახვის შეცდომა: %s";
    s[(int)Str::MsgSavedBootLogo] = "ჰორიზონტალური ლოგო შენახულია როგორც %s";
    s[(int)Str::MsgRefuseOverwrite] = "გახსნილი ფაილის გადაწერა აკრძალულია. აირჩიეთ სხვა გზა.";
    s[(int)Str::MsgColorPatchError] = "ფერთა შეცდომა: %s";
    s[(int)Str::MsgPackError] = "პაკეტირების შეცდომა: %s";
    s[(int)Str::MsgImportedChangedPngs] = "იმპორტირებულია %zu შეცვლილი PNG (%zu ემთხვევა ორიგინალს).";
    s[(int)Str::LabelBuiltin] = "ჩაშენებული";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "ჩაშენებული პროგრამის ჩატვირთვა ვერ მოხერხდა: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "ჩაშენებული პროგრამა ჩაიტვირთა: %s";
    s[(int)Str::ThemeFactoryDefault] = "ქარხნული ნაგულისხმევი";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Voopoo-ს ორიგინალი წითელი და ყვითელი სტილი";
    s[(int)Str::ThemeCyberpunkNeon] = "კიბერპანკ ნეონი";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "ელექტრო ციანი, ნეონის ვარდისფერი და იისფერი";
    s[(int)Str::ThemeMatrixGreen] = "მატრიცა მწვანე";
    s[(int)Str::ThemeMatrixGreenDesc] = "CRT ტერმინალის ფოსფორი და ზურმუხტისფერი მწვანე";
    s[(int)Str::ThemeArcticIceBlue] = "არქტიკული ყინული";
    s[(int)Str::ThemeArcticIceBlueDesc] = "მყინვარული ციანი, ღრმა ცა და ლაჟვარდოვანი ნათება";
    s[(int)Str::ThemeAmberFire] = "ქარვისფერი ცეცხლი";
    s[(int)Str::ThemeAmberFireDesc] = "ლავის ნარინჯისფერი, ოქროს ალი და ნაკვერჩხალი";
    s[(int)Str::ThemePurpleNebula] = "იისფერი ნისლეული";
    s[(int)Str::ThemePurpleNebulaDesc] = "ღრმა იისფერი, ლავანდა და ნეონის ამეთვისტო";
    s[(int)Str::ThemeStealthDarkGray] = "სტელსი მუქი ნაცრისფერი";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "მქრქალი გრაფიტი, ფიქალი და იარაღის მეტალი";
    s[(int)Str::RoleAccentRed] = "აქცენტი / წითელი რეჟიმი";
    s[(int)Str::RoleAccentRedScope] = "მთავარი UI აქცენტი სტანდარტულ რეჟიმებში — 8 ადგილი";
    s[(int)Str::RoleAccentCyan] = "აქცენტი / ციანის რეჟიმი";
    s[(int)Str::RoleAccentCyanScope] = "მთავარი UI აქცენტი SMART/RBA რეჟიმებში — 7 ადგილი";
    s[(int)Str::RoleHighlightYellow] = "გამოკვეთა / ყვითელი";
    s[(int)Str::RoleHighlightYellowScope] = "ქულების ზოლი და სერვისის პანელები — 4 ადგილი";
    s[(int)Str::RoleRankBronze] = "რანგი / ბრინჯაო";
    s[(int)Str::RoleRankBronzeScope] = "ბრინჯაოს რანგის პალიტრა";
    s[(int)Str::RoleRankGold] = "რანგი / ოქრო";
    s[(int)Str::RoleRankGoldScope] = "ოქროს რანგის პალიტრა";
    s[(int)Str::RoleRankIce] = "რანგი / პლატინა (ყინული)";
    s[(int)Str::RoleRankIceScope] = "პლატინა / ყინულის რანგის პალიტრა";
    s[(int)Str::RoleRankCyan] = "რანგი / ციანი";
    s[(int)Str::RoleRankCyanScope] = "ციანის რანგის პალიტრა";
    s[(int)Str::RoleRankOrange] = "რანგი / ნარინჯისფერი";
    s[(int)Str::RoleRankOrangeScope] = "ნარინჯისფერი რანგის პალიტრა";
    s[(int)Str::RoleRankRed] = "რანგი / წითელი";
    s[(int)Str::RoleRankRedScope] = "წითელი რანგის პალიტრა";
    s[(int)Str::RoleDigitsWattage] = "სიმძლავრის ციფრები";
    s[(int)Str::RoleDigitsWattageScope] = "დიდი სიმძლავრის მაჩვენებელი მთავარ ეკრანზე (19x40)";
    s[(int)Str::RoleDigitsStats] = "სტატისტიკის ციფრები";
    s[(int)Str::ColorPresetWhite] = "თეთრი (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "ძირითადი / წითელი (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / ციანი (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "სერვისი / ყვითელი (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "ბრინჯაო (#B57947)";
    s[(int)Str::ColorPresetGold] = "ოქრო (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "ნარინჯისფერი (#F25300)";
    s[(int)Str::ColorPresetGreen] = "მწვანე (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (ძირითადი ციფრები 0-9, 10 ფაილი)";
    s[(int)Str::FontPreset11x20] = "11x20 (მეორადი ციფრები 0-9, 10 ფაილი)";
    s[(int)Str::FontPreset7x13] = "7x13 (მცირე სტატუსი 0-9, A-F, 16 ფაილი)";
    s[(int)Str::FontPreset6x10] = "6x10 (მიკრო სტატუსი 0-9, 10 ფაილი)";
    s[(int)Str::FontOs1x] = "1x (პირდაპირი)";
    s[(int)Str::FontOs2x] = "2x (სწრაფი)";
    s[(int)Str::FontOs4x] = "4x (რეკომენდებული)";
    s[(int)Str::FontOs8x] = "8x (ულტრა)";
}

static void InitRomanian() {
    auto& s = g_table[(int)Language::Romanian];
    s[(int)Str::SupportFull] = "Complet";
    s[(int)Str::SupportNone] = "Neacceptat";
    s[(int)Str::SupportPalette] = "Paletă de culori";
    s[(int)Str::SupportMonochrome] = "Ecran monocrom; fără paletă de culori.";
    s[(int)Str::EmulatorHistoryDay] = "Zi din istoric";
    s[(int)Str::EmulatorClockHour] = "Ore";
    s[(int)Str::EmulatorClockMinute] = "Minute";
    s[(int)Str::EmulatorClockField] = "Câmp selectat";
    s[(int)Str::MenuFile] = "Fișier";
    s[(int)Str::MenuEdit] = "Editare";
    s[(int)Str::MenuLanguage] = "Limbă";
    s[(int)Str::MenuHelp] = "Ajutor";
    s[(int)Str::UpdateTitle] = "Actualizări studio";
    s[(int)Str::UpdateVersion] = "Versiune instalată: %s";
    s[(int)Str::UpdateCheck] = "Caută actualizări";
    s[(int)Str::UpdateChecking] = "Se verifică GitHub Releases...";
    s[(int)Str::UpdateCurrent] = "Ai cea mai recentă versiune.";
    s[(int)Str::UpdateDownload] = "Descarcă";
    s[(int)Str::UpdateDownloading] = "Se descarcă și se verifică...";
    s[(int)Str::UpdateInstall] = "Instalează și repornește";
    s[(int)Str::UpdateReady] = "Gata de instalare.";
    s[(int)Str::UpdateChangelog] = "Modificări";
    s[(int)Str::UpdateFailed] = "Actualizare eșuată";
    s[(int)Str::UpdateAvailable] = "Versiune disponibilă: %s";
    s[(int)Str::MenuCreate] = "Nou...";
    s[(int)Str::MenuOpen] = "Deschide...";
    s[(int)Str::MenuImport] = "Importă resurse...";
    s[(int)Str::MenuExport] = "Exportă resurse...";
    s[(int)Str::MenuBuild] = "Generează firmware...";
    s[(int)Str::MenuExit] = "Ieșire";
    s[(int)Str::AppTitle] = "VOOPOO Firmware Studio";
    s[(int)Str::TabResourceStudio] = "Resurse";
    s[(int)Str::TabFontGenerator] = "Generator Fonturi";
    s[(int)Str::TabFirmwareInfo] = "Despre firmware";
    s[(int)Str::TabFirmwareComparison] = "Comparație";
    s[(int)Str::CompareModeTwoFiles] = "Două fișiere";
    s[(int)Str::CompareModeEditor] = "Editor și fișier";
    s[(int)Str::CompareLeft] = "Stânga";
    s[(int)Str::CompareRight] = "Dreapta";
    s[(int)Str::CompareChooseFile] = "Alege fișierul...";
    s[(int)Str::CompareReadOnlyNote] = "Doar vizualizare; include modificările din editor.";
    s[(int)Str::CompareChooseSources] = "Alege ambele surse pentru comparație.";
    s[(int)Str::CompareEditorUnavailable] = "Niciun firmware deschis în editor.";
    s[(int)Str::CompareChanged] = "Modificat";
    s[(int)Str::CompareAdded] = "Adăugat";
    s[(int)Str::CompareRemoved] = "Eliminat";
    s[(int)Str::CompareEqual] = "Identic";
    s[(int)Str::CompareIncompatible] = "Incompatibil";
    s[(int)Str::CompareDecodeError] = "Eroare de decodare";
    s[(int)Str::CompareStatusFilter] = "Stare";
    s[(int)Str::CompareDifferencePreview] = "Diferență";
    s[(int)Str::CompareHighlightPixels] = "Evidențiază diferențele";
    s[(int)Str::CompareDifferentPixels] = "Diferă %zu / %zu pixeli";
    s[(int)Str::CompareMissingResource] = "Resursa lipsește";
    s[(int)Str::CompareIncompatibleHint] = "Tipuri sau dimensiuni diferite: comparația pixelilor nu este disponibilă.";
    s[(int)Str::CompareImageUnavailable] = "Imagine indisponibilă";
    s[(int)Str::CompareTextureError] = "Textura de previzualizare nu a putut fi creată.";
    s[(int)Str::CompareAllStatuses] = "Toate stările";
    s[(int)Str::TabFirmwareEmulator] = "Emulator";
    s[(int)Str::TabFirmwareLayout] = "Aspect UI";
    s[(int)Str::LayoutUnsupported] = "Editarea aspectului UI nu este disponibilă pentru acest firmware.";
    s[(int)Str::LayoutNativeNote] = "Trageți un element sau introduceți X/Y. Previzualizarea execută firmware-ul modificat; schimbările se aplică reviziei hardware selectate.";
    s[(int)Str::LayoutRevision] = "Revizie hardware";
    s[(int)Str::LayoutCheckingRevisions] = "Se verifică interfețele identice…";
    s[(int)Str::LayoutAllRevisions] = "Toate reviziile";
    s[(int)Str::LayoutRevisionGroup] = "Interfață identică: %s";
    s[(int)Str::LayoutRevisionIndividual] = "Revizie: %s";
    s[(int)Str::LayoutResetScope] = "Resetează reviziile selectate";
    s[(int)Str::LayoutSelectionUnavailable] = "Nu există un element compatibil în toate reviziile selectate.";
    s[(int)Str::LayoutRevisionCode] = "HW %04X";
    s[(int)Str::LayoutElements] = "Elemente UI";
    s[(int)Str::LayoutNoElements] = "Acest ecran nu are blocuri deplasabile.";
    s[(int)Str::LayoutSharedBlock] = "Acest bloc împarte coordonatele cu alte ecrane.";
    s[(int)Str::LayoutPositionX] = "X (pixeli)";
    s[(int)Str::LayoutPositionY] = "Y (pixeli)";
    s[(int)Str::LayoutOffset] = "Deplasare: %+d, %+d px";
    s[(int)Str::LayoutGridStep] = "Pas: %d × %d px";
    s[(int)Str::LayoutGridSpacing] = "Spațierea grilei (px)";
    s[(int)Str::LayoutGridConstraint] = "Când niciun punct al grilei nu poate fi reprezentat, au prioritate pașii și limitele native ale coordonatelor.";
    s[(int)Str::LayoutSnapGrid] = "Aliniere la grilă";
    s[(int)Str::LayoutSnapElements] = "Aliniere la marginile și centrele elementelor";
    s[(int)Str::LayoutGuides] = "Ghidaje de deplasare";
    s[(int)Str::LayoutAlign] = "Aliniază la ecran";
    s[(int)Str::LayoutAlignLeft] = "Stânga";
    s[(int)Str::LayoutAlignCenter] = "Centru X";
    s[(int)Str::LayoutAlignRight] = "Dreapta";
    s[(int)Str::LayoutAlignTop] = "Sus";
    s[(int)Str::LayoutAlignMiddle] = "Centru Y";
    s[(int)Str::LayoutAlignBottom] = "Jos";
    s[(int)Str::LayoutNudgeHint] = "Săgeți: pas nativ de pixel; Shift: pas mai mare. Faceți clic pe ecran pentru focalizare.";
    s[(int)Str::LayoutOverlays] = "Contururile elementelor";
    s[(int)Str::LayoutOverlapOverlay] = "Evidențiază suprapunerile";
    s[(int)Str::LayoutOverlaps] = "Elemente intersectate";
    s[(int)Str::LayoutNoOverlaps] = "Nu există intersecții de dreptunghiuri";
    s[(int)Str::LayoutDrawOrderUnknown] = "Ordinea nativă de desenare nu este verificată";
    s[(int)Str::LayoutDrawOrderPair] = "%s (#%d) este desenat după %s (#%d)";
    s[(int)Str::LayoutDrawOrderNote] = "Ordinea apelurilor de redesenare principală, nu straturi permanente. Actualizările pot diferi; numai pixelii efectiv scriși acoperă conținutul anterior.";
    s[(int)Str::LayoutDrawSequence] = "Secvența nativă de desenare";
    s[(int)Str::LayoutDrawInterleaved] = "Apelurile sunt intercalate; suprapunerea depinde de partea fiecărui element.";
    s[(int)Str::EmulatorBefore] = "Înainte (firmware deschis)";
    s[(int)Str::EmulatorAfter] = "După (modificări curente)";
    s[(int)Str::EmulatorSideBySide] = "Alăturat";
    s[(int)Str::EmulatorOriginalError] = "Previzualizarea firmware-ului deschis a eșuat: %s";
    s[(int)Str::MenuSaveProject] = "Salvează proiectul";
    s[(int)Str::MenuSaveProjectAs] = "Salvează proiectul ca...";
    s[(int)Str::MsgProjectSaved] = "Proiect salvat: %s";
    s[(int)Str::MenuBuild] = "Exportă firmware";
    s[(int)Str::BtnOpenFirmware] = "Deschide firmware sau proiect...";
    s[(int)Str::StartupOpenFile] = "Deschide firmware sau proiect...";
    s[(int)Str::UnsavedPrompt] = "Salvați proiectul înainte de a continua?";
    s[(int)Str::UnsavedPrompt] = "Salvați proiectul înainte de a continua?";
    s[(int)Str::LayoutBounds] = "X: %d…%d / Y: %d…%d";
    s[(int)Str::LayoutResetElement] = "Resetează elementul";
    s[(int)Str::LayoutResetRevision] = "Resetează aspectul acestei revizii";
    s[(int)Str::LayoutResetAll] = "Resetează toate modificările de aspect";
    s[(int)Str::LayoutPatchSites] = "Adresele patch-urilor";
    s[(int)Str::LayoutHardware] = "HW %04X · %d × %d";
    s[(int)Str::LayoutPreviewState] = "Starea previzualizării";
    s[(int)Str::LayoutMoveElement] = "Mută elementul UI";
    s[(int)Str::LayoutRba] = "Mod RBA";
    s[(int)Str::LayoutTemperatureControl] = "Controlul temperaturii";
    s[(int)Str::LayoutTemperature] = "Temperatură (°C)";
    s[(int)Str::LayoutRenderFailed] = "Bufferul nativ de imagine nu este disponibil.";
    s[(int)Str::LayoutElementPower] = "Putere";
    s[(int)Str::LayoutElementBattery] = "Baterie";
    s[(int)Str::LayoutElementResistance] = "Rezistență";
    s[(int)Str::LayoutElementDuration] = "Durată";
    s[(int)Str::LayoutElementPuffs] = "Pufuri";
    s[(int)Str::LayoutElementMode] = "Mod";
    s[(int)Str::LayoutElementStatus] = "Stare";
    s[(int)Str::LayoutElementScore] = "Scor";
    s[(int)Str::LayoutElementStatistics] = "Statistici";
    s[(int)Str::LayoutElementVoltage] = "Tensiune";
    s[(int)Str::LayoutElementGraph] = "Grafic";
    s[(int)Str::LayoutElementRank] = "Rang";
    s[(int)Str::LayoutElementModeSelection] = "Selectarea modului";
    s[(int)Str::LayoutElementLogo] = "Siglă";
    s[(int)Str::LayoutChangedElements] = "Elemente UI modificate";
    s[(int)Str::EmulatorSupported] = "Previzualizare UI nativă pentru toate firmware-urile DRAG incluse.";
    s[(int)Str::EmulatorNativeNote] = "Randare nativă cu modificările curente. Fără emularea părții electrice.";
    s[(int)Str::EmulatorWatts] = "Putere (W)";
    s[(int)Str::EmulatorPuffs] = "Pufuri";
    s[(int)Str::EmulatorBattery] = "Baterie (0–5)";
    s[(int)Str::EmulatorResistance] = "Rezistență (mΩ)";
    s[(int)Str::EmulatorDuration] = "Durată (0,01 s)";
    s[(int)Str::EmulatorLocked] = "Blocat";
    s[(int)Str::EmulatorCharging] = "Încărcare";
    s[(int)Str::EmulatorScore] = "Scor total";
    s[(int)Str::EmulatorScoreDisplay] = "Cel mai bun scor";
    s[(int)Str::EmulatorRedraw] = "Redesenează";
    s[(int)Str::EmulatorLandscape] = "Orizontal";
    s[(int)Str::EmulatorScreen] = "Ecran";
    s[(int)Str::EmulatorMainScreen] = "Ecran principal";
    s[(int)Str::FontSystemFonts] = "Fonturi de sistem";
    s[(int)Str::FontUserFonts] = "Fonturi de utilizator";
    s[(int)Str::EmulatorMenuItem] = "Element de meniu";
    s[(int)Str::EmulatorMaterial] = "Material";
    s[(int)Str::EmulatorMenuConfirmed] = "Confirmat";
    s[(int)Str::EmulatorResources] = "Resurse citite de firmware";
    s[(int)Str::EmulatorNoResources] = "Nu s-au înregistrat citiri din catalog.";
    s[(int)Str::EmulatorUidNote] = "UID MCU emulat, nu ID-ul unui dispozitiv conectat.";
    s[(int)Str::EmulatorLoaderVersion] = "Versiune virtuală a încărcătorului";
    s[(int)Str::EmulatorLoaderId] = "Octeți CHIP-ID virtuali";
    s[(int)Str::EmulatorLoaderNote] = "Datele încărcătorului sunt emulate, nu citite din firmware sau de pe dispozitiv.";
    s[(int)Str::TabFirmwarePatches] = "Patch-uri";
    s[(int)Str::PatchBootLogo] = "Ecran de pornire";
    s[(int)Str::PatchShutdownLogo] = "Ecran de oprire";
    s[(int)Str::PatchDeviceInfo] = "Ecran CHIP ID / versiune";
    s[(int)Str::PatchDeviceInfoNote] = "Doar imagine statică. Înlocuiește ecranul de service, nu ecranul principal de putere și baterie.";
    s[(int)Str::PatchLogoNote] = "Imagini independente de 80 × 160 pentru toate reviziile DRAG S Score / Factory. Neverificat pe dispozitiv.";
    s[(int)Str::PatchUnsupported] = "Acest patch necesită firmware DRAG S Score sau Factory.";
    s[(int)Str::PatchEnabled] = "Activează sigla proprie";
    s[(int)Str::PatchCanvas] = "Compoziție · 80 × 160";
    s[(int)Str::PatchNativePreview] = "Previzualizare nativă a ecranului";
    s[(int)Str::PatchPreviewNote] = "Previzualizarea nativă arată setările aplicate, nu schița compoziției.";
    s[(int)Str::PatchFit] = "Încadrează";
    s[(int)Str::PatchCover] = "Acoperă";
    s[(int)Str::PatchStretch] = "Întinde";
    s[(int)Str::PatchScale] = "Scară";
    s[(int)Str::PatchAnimation] = "Animație de intrare";
    s[(int)Str::PatchNone] = "Fără";
    s[(int)Str::PatchFade] = "Estompare";
    s[(int)Str::PatchDuration] = "Durată (ms)";
    s[(int)Str::PatchAutoHardware] = "Automat";
    s[(int)Str::PatchAnimationPhase] = "Faza animației";
    s[(int)Str::PatchPlay] = "Redă";
    s[(int)Str::EmulatorModeScreens] = "Ecrane";
    s[(int)Str::EmulatorModeDevice] = "Dispozitiv";
    s[(int)Str::DeviceUnsupported] = "Modul dispozitiv acceptă DRAG S Score / HW 3103.";
    s[(int)Str::DeviceNote] = "Firmware executat de la Reset cu periferice virtuale. Încălzirea este simulată; siguranța electrică nu este verificată.";
    s[(int)Str::DeviceReset] = "Repornire";
    s[(int)Str::DevicePause] = "Pauză";
    s[(int)Str::DeviceResume] = "Pornire";
    s[(int)Str::DeviceStep] = "Pas";
    s[(int)Str::DeviceChanged] = "Modificările actuale nu sunt încă încărcate.";
    s[(int)Str::DeviceApply] = "Repornire cu modificări";
    s[(int)Str::DeviceButtons] = "Țineți butoanele sau spațiu / ↑ / ↓.";
    s[(int)Str::DeviceAtomizerConnected] = "Rezistență conectată";
    s[(int)Str::DeviceTime] = "Timp virtual";
    s[(int)Str::DeviceSleeping] = "Procesor în repaus";
    s[(int)Str::DeviceLoading] = "Se încarcă dispozitivul...";
    s[(int)Str::DeviceLog] = "Acces la periferice";
    s[(int)Str::BadgeNoFirmware] = "| [Niciun firmware deschis - Trageți fișierul .zwdf/.xwdf aici]";
    s[(int)Str::BadgeFirmwareInfo] = "| %s | %zu elemente | %zu înregistrări";
    s[(int)Str::BadgeUsbConnected] = "[USB: %s (ID: %s)]";
    s[(int)Str::BtnOpenFirmware] = "Deschide Firmware...";
    s[(int)Str::BtnExportAll] = "Exportă toate resursele în dosar...";
    s[(int)Str::BtnImportFolder] = "Importă dosar cu PNG-uri...";
    s[(int)Str::BtnBuildFirmware] = " CONSTRUIEȘTE FIRMWARE NOU ";
    s[(int)Str::BtnRevertAllStaged] = "Anulează toate modificările";
    s[(int)Str::GroupAll] = "Toate grupurile";
    s[(int)Str::GroupAlerts] = "Alerte (Dialoguri și pictograme)";
    s[(int)Str::GroupFonts] = "Fonturi (6x10, 7x13, 11x20, 19x40)";
    s[(int)Str::GroupScore] = "Scor și ranguri";
    s[(int)Str::GroupStatusLabels] = "Stare și etichete UI";
    s[(int)Str::GroupControls] = "Comenzi și baterie";
    s[(int)Str::GroupScreens] = "Ecrane și sigle de pornire";
    s[(int)Str::GroupAnimations] = "Animații";
    s[(int)Str::PanelResourcesHeader] = "Resurse";
    s[(int)Str::LabelSearch] = "Căutare";
    s[(int)Str::SearchPlaceholder] = "filtrează după titlu, id, adresă...";
    s[(int)Str::NoFirmwareLoadedList] = "Niciun firmware încărcat.\nFișier > Nou / Deschide";
    s[(int)Str::NoResourcesMatchFilter] = "Nicio resursă nu corespunde filtrului de căutare.";
    s[(int)Str::PanelResourcePreviewHeader] = "Previzualizare";
    s[(int)Str::SelectResourcePrompt] = "Selectați o resursă din lista din stânga pentru previzualizare și editare.";
    s[(int)Str::LabelId] = "ID";
    s[(int)Str::LabelGroup] = "Grup";
    s[(int)Str::LabelColor] = "Culoare";
    s[(int)Str::LabelColorCustom] = "Personalizat";
    s[(int)Str::LabelFlashAddress] = "Adresă Flash";
    s[(int)Str::LabelDimensions] = "Dimensiuni";
    s[(int)Str::LabelBytes] = "Octeți";
    s[(int)Str::LabelRecords] = "Înregistrări";
    s[(int)Str::StatusModifiedStaged] = "Stare: MODIFICAT (pregătit în memorie)";
    s[(int)Str::StatusOriginal] = "Stare: ORIGINAL (din firmware)";
    s[(int)Str::StatusStockOriginal] = "Stare: ORIGINAL DIN FABRICĂ (conform A050/E050)";
    s[(int)Str::StatusCustomDiffers] = "Stare: MODIFICAT (diferă de cel din fabrică)";
    s[(int)Str::BtnRestoreStock] = "Restaurează la starea din fabrică";
    s[(int)Str::LabelCompareWithStock] = "Comparație cu firmware-ul etalon din fabrică:";
    s[(int)Str::LabelActivePreview] = "Activ curent:";
    s[(int)Str::LabelStockReferencePreview] = "Referință din fabrică:";
    s[(int)Str::FilterOnlyCustom] = "Afișează doar resursele modificate";
    s[(int)Str::MsgRestoredToStock] = "Resursa '%s' a fost restaurată la starea din fabrică.";
    s[(int)Str::BtnReplaceFromPng] = "Înlocuiește din PNG...";
    s[(int)Str::BtnRestoreOriginal] = "Restaurează originalul";
    s[(int)Str::BtnExportThisPng] = "Exportă acest PNG...";
    s[(int)Str::BtnExportHorizontal] = "Exportă orizontal (136x22)...";
    s[(int)Str::SectionPaletteTheme] = "Paletă de culori și randare (RGB565)";
    s[(int)Str::LabelTheme] = "Temă";
    s[(int)Str::BtnApply] = "Aplică";
    s[(int)Str::BtnResetAll] = "Resetează tot";
    s[(int)Str::LabelRole] = "Rol";
    s[(int)Str::BtnResetRole] = "Resetează rolul";
    s[(int)Str::PromptLoadFwColors] = "Încărcați firmware-ul pentru a inspecta și modifica culorile.";
    s[(int)Str::LabelPreviewTint] = "Nuanță previzualizare";
    s[(int)Str::InfoDigitsMonochrome] = "Cifrele sunt măști monocrome. Culoarea lor este setată prin codul firmware.";
    s[(int)Str::InfoPreviewNotSaved] = "Nuanța de previzualizare nu este salvată în firmware. Exportul PNG RGB565 o include.";
    s[(int)Str::LabelPreviewOnly] = "Doar previzualizare:";
    s[(int)Str::LabelPresets] = "Presetări...";
    s[(int)Str::BtnResetColor] = "Resetează culoarea";
    s[(int)Str::LabelZoom] = "Zoom previzualizare";
    s[(int)Str::LabelPixelGrid] = "Grilă pixeli";
    s[(int)Str::LabelHorizontalPreview] = "Previzualizare orizontală lizibilă (136x22):";
    s[(int)Str::FontSec1Title] = "1. Selectare font";
    s[(int)Str::FontSec1WindowsFont] = "Font Windows";
    s[(int)Str::FontSec1BrowseTtf] = "Răsfoiește fișier TTF/OTF...";
    s[(int)Str::FontSec1ActiveFont] = "Font activ: %s";
    s[(int)Str::FontSec2Title] = "2. Presetare afișaj";
    s[(int)Str::FontSec3Title] = "3. Parametri de randare";
    s[(int)Str::FontSec3Threshold] = "Prag luminozitate";
    s[(int)Str::FontSec3ThresholdHint] = "Mai mic = mai îngroșat, Mai mare = mai subțire";
    s[(int)Str::FontSec3Stretch] = "Întinde pe toată celula";
    s[(int)Str::FontSec3Baseline] = "Aliniere linie de bază";
    s[(int)Str::FontSec3CenterV] = "Centrează vertical";
    s[(int)Str::FontSec3PaddingX] = "Spațiere X";
    s[(int)Str::FontSec3PaddingY] = "Spațiere Y";
    s[(int)Str::FontSec3Oversampling] = "Oversampling";
    s[(int)Str::FontSec4Title] = "4. Integrare în firmware";
    s[(int)Str::FontSec4ApplyPreset] = " > APLICĂ GLIFELE LA FIRMWARE-UL DESCHIS < ";
    s[(int)Str::FontSec4ApplyAll] = "Aplică toate fonturile";
    s[(int)Str::FontSec5Title] = "5. Export în PNG";
    s[(int)Str::FontSec5OutputFolder] = "Dosar de ieșire";
    s[(int)Str::FontSec5BrowseFolder] = "Răsfoiește dosar...";
    s[(int)Str::FontSec5OpenExplorer] = "Deschide în Explorer";
    s[(int)Str::FontSec5ExportAllPreset] = "Exportă toate presetările";
    s[(int)Str::FontSec5ExportButton] = "Salvează fișierele PNG pe disc";
    s[(int)Str::FontRightLivePreview] = "Previzualizare în timp real";
    s[(int)Str::FontRightNoGlyphs] = "Nicio glifă încărcată.";
    s[(int)Str::FontPresetInfo] = "Presetare: %s (%dx%d px, %d glife, dimensiune: %.1f px)";
    s[(int)Str::FwInfoSpecTitle] = "Specificație pachet VOOPOO DRAG S / DRAG X / DRAG 2";
    s[(int)Str::FwInfoNoFirmware] = "Niciun firmware încărcat. Deschideți un fișier .zwdf/.xwdf în fila 'Studio Resurse'.";
    s[(int)Str::FwInfoFilePath] = "Cale fișier: %s";
    s[(int)Str::FwInfoSha256] = "SHA-256 sursă: %s";
    s[(int)Str::FwInfoPackageSize] = "Dimensiune pachet: %zu octeți";
    s[(int)Str::FwInfoHwRevisions] = "Revizii hardware: %zu înregistrări";
    s[(int)Str::FwInfoRecognizedResources] = "Resurse recunoscute: %zu (fonturi, pictograme, ranguri, etichete, ecrane)";
    s[(int)Str::FwInfoRecordsTable] = "Tabel înregistrări hardware";
    s[(int)Str::FwInfoColRecord] = "Înregistrare #";
    s[(int)Str::FwInfoColDevCode] = "Cod dispozitiv";
    s[(int)Str::FwInfoColPayload] = "Dimensiune date";
    s[(int)Str::FwInfoColImage] = "Dimensiune imagine";
    s[(int)Str::FwInfoColFlashBase] = "Adresă bază Flash";
    s[(int)Str::FwInfoColMSP] = "Indicator stivă (MSP)";
    s[(int)Str::FwInfoColResetVector] = "Vector resetare";
    s[(int)Str::BtnRevertAllModifications] = "Anulează toate modificările";
    s[(int)Str::MsgReadyOpenFw] = "Pregătit. Deschideți un fișier .zwdf/.xwdf sau generați fonturi personalizate.";
    s[(int)Str::MsgSuccessOpened] = "Deschis: %s";
    s[(int)Str::MsgOpenError] = "Eroare la deschidere: %s";
    s[(int)Str::MsgAllReverted] = "Toate modificările pregătite au fost anulate la starea inițială.";
    s[(int)Str::MsgExportSuccess] = "Au fost exportate %d resurse în: %s";
    s[(int)Str::MsgExportError] = "Eroare la export: %s";
    s[(int)Str::MsgImportSuccess] = "Au fost importate %d fișiere PNG din dosar.";
    s[(int)Str::MsgImportError] = "Eroare la import: %s";
    s[(int)Str::MsgBuildSuccess] = "Construire REUȘITĂ: %s";
    s[(int)Str::MsgBuildError] = "Eroare la construire: %s";
    s[(int)Str::MsgFontNotLoaded] = "Eroare: Niciun font încărcat în generator!";
    s[(int)Str::MsgFontInjectedPreset] = "Au fost injectate %d glife (%s) în memoria firmware-ului!";
    s[(int)Str::MsgFontInjectedAll] = "Glife aplicate: %d.";
    s[(int)Str::MsgFontExportSuccess] = "Au fost exportate cu succes %d fișiere PNG în: %s";
    s[(int)Str::MsgFontExportSuccessSingle] = "Au fost exportate cu succes %d fișiere pentru %s în: %s";
    s[(int)Str::MsgFontExportError] = "Eroare la export: %s";
    s[(int)Str::ModalBuildReportTitle] = "Raport construire";
    s[(int)Str::ModalBuildSuccessBanner] = "CONSTRUIREA ȘI VERIFICAREA FIRMWARE-ULUI AU REUȘIT!";
    s[(int)Str::ModalSavedFile] = "Fișier salvat: %s";
    s[(int)Str::ModalOutputSize] = "Dimensiune ieșire: %zu octeți";
    s[(int)Str::ModalSha256] = "SHA-256: %s";
    s[(int)Str::ModalModBitmaps] = "Resurse imagine modificate: %zu";
    s[(int)Str::ModalModColorGroups] = "Grupuri de culori modificate: %zu";
    s[(int)Str::ModalPatchedRanges] = "Intervale de octeți modificate: %zu în %zu înregistrări";
    s[(int)Str::ModalUntouchedBytes] = "Octeți neatinși verificați: %zu (identici cu originalul)";
    s[(int)Str::ModalReportNote] = "Un raport detaliat a fost salvat ca .report.json lângă firmware.";
    s[(int)Str::BtnOk] = "OK";
    s[(int)Str::BtnOpenInExplorer] = "Deschide în Explorer";
    s[(int)Str::BtnCopyClipboard] = "Copiază imaginea";
    s[(int)Str::BtnPasteClipboard] = "Lipește imaginea";
    s[(int)Str::BtnUndo] = "Anulează";
    s[(int)Str::BtnRedo] = "Refă";
    s[(int)Str::TooltipUndo] = "Anulează: %s (Ctrl+Z)";
    s[(int)Str::TooltipRedo] = "Refă: %s (Ctrl+Y)";
    s[(int)Str::TooltipUndoEmpty] = "Nimic de anulat";
    s[(int)Str::TooltipRedoEmpty] = "Nimic de refăcut";
    s[(int)Str::LabelPixelTools] = "Instrumente pixeli:";
    s[(int)Str::ToolPencil] = "Creion";
    s[(int)Str::ToolEraser] = "Radieră";
    s[(int)Str::BtnInvertPixels] = "Inversează";
    s[(int)Str::BtnClearPixels] = "Golește";
    s[(int)Str::BtnFlipH] = "Răstoarnă O";
    s[(int)Str::BtnFlipV] = "Răstoarnă V";
    s[(int)Str::MsgCopiedToClipboard] = "Imaginea %s a fost copiată în clipboard.";
    s[(int)Str::MsgPastedFromClipboard] = "Imaginea din clipboard a fost lipită pentru %s!";
    s[(int)Str::MsgClipboardNoImage] = "Nicio imagine găsită în clipboard.";
    s[(int)Str::MsgUndoPerformed] = "Anulat: %s";
    s[(int)Str::MsgRedoPerformed] = "Refăcut: %s";
    s[(int)Str::BadgeFirmwareSummary] = "| %s (%s) | %zu înregistrări | %zu modificări";
    s[(int)Str::BadgeProjectSummary] = "| Proiect: %s (%s) | %zu înregistrări | %zu modificări";
    s[(int)Str::LabelCategory] = "Categorie";
    s[(int)Str::LabelHex] = "Hex";
    s[(int)Str::LabelMetaDetailsCustom] = "ID: %s  |  Grup: %s  |  Culoare: Personalizată (%s)";
    s[(int)Str::LabelMetaDetails] = "ID: %s  |  Grup: %s  |  Culoare: %s";
    s[(int)Str::LabelMetaHardware] = "Adresă Flash: 0x%08X  |  Dimensiuni: %u x %u px  |  Octeți: %zu  |  Înregistrări: %zu";
    s[(int)Str::LabelPixelCoord] = "Pixel: (%d, %d)";
    s[(int)Str::LabelPixelHelp] = "| Clic stânga: %s  |  Clic dreapta: Șterge";
    s[(int)Str::LabelDrawAction] = "Desenează";
    s[(int)Str::LabelEraseAction] = "Șterge";
    s[(int)Str::LabelFontPreset] = "Presetare";
    s[(int)Str::LabelFontPixelGrid] = "Grilă pixeli";
    s[(int)Str::MsgAppliedColorTheme] = "Tema de culori '%s' a fost aplicată pe toate resursele potrivite.";
    s[(int)Str::MsgImportedMatchingPngs] = "Au fost importate %zu fișiere PNG potrivite din dosar.";
    s[(int)Str::MsgOpenedFwDragDrop] = "Firmware deschis prin Drag & Drop: %s";
    s[(int)Str::MsgDragDropError] = "Eroare Drag & Drop: %s";
    s[(int)Str::MsgLoadedFontDragDrop] = "Font încărcat prin Drag & Drop: %s";
    s[(int)Str::MsgFailedLoadFont] = "Nu s-a putut încărca fișierul fontului: %s";
    s[(int)Str::MsgPngMatchesOriginal] = "Fișierul PNG selectat pentru %s este identic cu originalul.";
    s[(int)Str::MsgStagedReplacement] = "Înlocuirea PNG pentru %s a fost pregătită!";
    s[(int)Str::MsgPngLoadError] = "Eroare la încărcarea PNG: %s";
    s[(int)Str::MsgReplacedMatchedOriginal] = "Resursa înlocuită %s este identică cu originalul.";
    s[(int)Str::MsgStagedReplacementCustom] = "Înlocuirea PNG personalizată pentru %s a fost pregătită!";
    s[(int)Str::MsgStagedRgbReplacement] = "Înlocuirea RGB565 PNG personalizată pentru %s a fost pregătită!";
    s[(int)Str::MsgRestoredOriginal] = "Resursa originală din firmware pentru %s a fost restaurată.";
    s[(int)Str::MsgSavedAssetPng] = "Salvat %s ca %s";
    s[(int)Str::MsgSaveError] = "Eroare la salvare: %s";
    s[(int)Str::MsgSavedBootLogo] = "Sigla orizontală a fost salvată ca %s";
    s[(int)Str::MsgRefuseOverwrite] = "Scrierea peste fișierul original încărcat este refuzată. Selectați o altă cale.";
    s[(int)Str::MsgColorPatchError] = "Eroare la aplicarea culorii: %s";
    s[(int)Str::MsgPackError] = "Eroare la împachetare: %s";
    s[(int)Str::MsgImportedChangedPngs] = "Au fost importate %zu PNG-uri modificate (%zu identice cu originalul).";
    s[(int)Str::LabelBuiltin] = "integrat";
    s[(int)Str::MsgCannotLoadBuiltinFw] = "Nu s-a putut încărca firmware-ul integrat: %s";
    s[(int)Str::MsgLoadedBuiltinFw] = "Firmware integrat încărcat: %s";
    s[(int)Str::ThemeFactoryDefault] = "Standard din fabrică";
    s[(int)Str::ThemeFactoryDefaultDesc] = "Stilul original Voopoo roșu și galben din fabrică";
    s[(int)Str::ThemeCyberpunkNeon] = "Cyberpunk Neon";
    s[(int)Str::ThemeCyberpunkNeonDesc] = "Cyan electric, roz neon și violet intens";
    s[(int)Str::ThemeMatrixGreen] = "Verde Matrix";
    s[(int)Str::ThemeMatrixGreenDesc] = "Fosfor terminal CRT și verde smarald";
    s[(int)Str::ThemeArcticIceBlue] = "Gheață arctică";
    s[(int)Str::ThemeArcticIceBlueDesc] = "Cyan glaciar, albastru adânc și strălucire azurie";
    s[(int)Str::ThemeAmberFire] = "Foc de chihlimbar";
    s[(int)Str::ThemeAmberFireDesc] = "Portocaliu lavă, flacără aurie și jăratic";
    s[(int)Str::ThemePurpleNebula] = "Nebuloasă purpurie";
    s[(int)Str::ThemePurpleNebulaDesc] = "Violet adânc, lavandă și ametist neon";
    s[(int)Str::ThemeStealthDarkGray] = "Gri închis Stealth";
    s[(int)Str::ThemeStealthDarkGrayDesc] = "Grafit mat, ardezie și metal de pistol";
    s[(int)Str::RoleAccentRed] = "Accent / mod roșu";
    s[(int)Str::RoleAccentRedScope] = "Accent principal UI în modurile standard — 8 zone";
    s[(int)Str::RoleAccentCyan] = "Accent / mod cyan";
    s[(int)Str::RoleAccentCyanScope] = "Accent principal UI în stările SMART/RBA — 7 zone";
    s[(int)Str::RoleHighlightYellow] = "Evidențiere / galben";
    s[(int)Str::RoleHighlightYellowScope] = "Bandă de scor și panouri de service — 4 zone";
    s[(int)Str::RoleRankBronze] = "Rang / bronz";
    s[(int)Str::RoleRankBronzeScope] = "Paletă rang Bronz";
    s[(int)Str::RoleRankGold] = "Rang / aur";
    s[(int)Str::RoleRankGoldScope] = "Paletă rang Aur";
    s[(int)Str::RoleRankIce] = "Rang / platină (gheață)";
    s[(int)Str::RoleRankIceScope] = "Paletă rang Platină / Gheață";
    s[(int)Str::RoleRankCyan] = "Rang / cyan";
    s[(int)Str::RoleRankCyanScope] = "Paletă rang Cyan";
    s[(int)Str::RoleRankOrange] = "Rang / portocaliu";
    s[(int)Str::RoleRankOrangeScope] = "Paletă rang Portocaliu";
    s[(int)Str::RoleRankRed] = "Rang / roșu";
    s[(int)Str::RoleRankRedScope] = "Paletă rang Roșu";
    s[(int)Str::RoleDigitsWattage] = "Cifre putere";
    s[(int)Str::RoleDigitsWattageScope] = "Afișaj mare de putere pe ecranul principal (19x40)";
    s[(int)Str::RoleDigitsStats] = "Cifre statistici";
    s[(int)Str::ColorPresetWhite] = "Alb (#FFFFFF)";
    s[(int)Str::ColorPresetMainRed] = "Principal / Roșu (#F30133)";
    s[(int)Str::ColorPresetSmartCyan] = "SMART / Cyan (#00DBFF)";
    s[(int)Str::ColorPresetServiceYellow] = "Service / Galben (#F3DD01)";
    s[(int)Str::ColorPresetBronze] = "Bronz (#B57947)";
    s[(int)Str::ColorPresetGold] = "Aur (#F4DC00)";
    s[(int)Str::ColorPresetOrange] = "Portocaliu (#F25300)";
    s[(int)Str::ColorPresetGreen] = "Verde (#00FF66)";
    s[(int)Str::FontPreset19x40] = "19x40 (Cifre principale 0-9, 10 fișiere)";
    s[(int)Str::FontPreset11x20] = "11x20 (Cifre secundare 0-9, 10 fișiere)";
    s[(int)Str::FontPreset7x13] = "7x13 (Stare mică 0-9, A-F, 16 fișiere)";
    s[(int)Str::FontPreset6x10] = "6x10 (Micro stare 0-9, 10 fișiere)";
    s[(int)Str::FontOs1x] = "1x (Direct)";
    s[(int)Str::FontOs2x] = "2x (Rapid)";
    s[(int)Str::FontOs4x] = "4x (Recomandat)";
    s[(int)Str::FontOs8x] = "8x (Ultra)";
}

static void InitSettingsTranslations() {
    constexpr Str keys[]{
        Str::SettingsTitle, Str::SettingsGeneral, Str::SettingsInterfaceEmulator,
        Str::SettingsDefaultsNote, Str::SettingsImmediateNote, Str::SettingsSmoothAnimations,
        Str::SettingsLogReviewSeconds, Str::SettingsDefaultTarget, Str::SettingsDefaultBootStages,
        Str::SettingsDefaultZooms, Str::SettingsExpandedSections, Str::SettingsResetDefaults,
        Str::SettingsSaveError, Str::SettingsRollbackError
    };
    struct Translation { Language language; const char* text[14]; };
    static constexpr Translation translations[]{
        {Language::Russian, {
            "Настройки", "Общие", "Интерфейс / Эмулятор",
            "Значения по умолчанию — для новых видов и документов; масштабы — при следующем запуске или создании нового окна. Текущие масштабы и содержимое проекта не изменятся.",
            "Язык, переходы и время просмотра завершённого журнала применяются после сохранения.",
            "Плавные переходы интерфейса", "Просмотр завершённого журнала",
            "Экран при открытии", "Этапы экрана запуска", "Масштаб по умолчанию", "Раскрытые разделы при открытии",
            "Сбросить до заводских",
            "Не удалось сохранить настройки. Проверьте доступ к studio_config.ini и его папке. Изменения не применены.",
            "Не удалось сохранить настройки и восстановить прежний язык в файле. Проверьте права на studio_config.ini и сохраните снова."
        }},
        {Language::Ukrainian, {
            "Налаштування", "Загальні", "Інтерфейс / Емулятор",
            "Типові значення — лише для нових подань і документів. Поточна робота та збережені проєкти не зміняться.",
            "Мова, переходи та час перегляду завершеного журналу застосовуються після збереження.",
            "Плавні переходи інтерфейсу", "Перегляд завершеного журналу",
            "Початковий екран", "Етапи екрана запуску", "Типовий масштаб", "Початково розгорнуті розділи",
            "Скинути чернетку (мова системи)",
            "Не вдалося зберегти налаштування. Перевірте доступ до studio_config.ini та його теки. Зміни не застосовано.",
            "Не вдалося зберегти налаштування та відновити попередню мову у файлі. Перевірте права на studio_config.ini і збережіть знову."
        }},
        {Language::Polish, {
            "Ustawienia", "Ogólne", "Interfejs / Emulator",
            "Domyślne wartości dotyczą tylko nowych widoków i dokumentów. Bieżąca praca i zapisane projekty nie zmienią się.",
            "Język, przejścia i czas przeglądania ukończonego dziennika są stosowane po zapisaniu.",
            "Płynne przejścia interfejsu", "Czas przeglądania ukończonego dziennika",
            "Początkowy ekran", "Etapy ekranu startowego", "Domyślne powiększenie", "Początkowo rozwinięte sekcje",
            "Resetuj szkic (język systemu)",
            "Nie można zapisać ustawień. Sprawdź uprawnienia do studio_config.ini i jego folderu. Zmiany nie zostały zastosowane.",
            "Nie można zapisać ustawień ani przywrócić poprzedniego języka w pliku. Sprawdź uprawnienia i zapisz ponownie."
        }},
        {Language::Kazakh, {
            "Баптаулар", "Жалпы", "Интерфейс / Эмулятор",
            "Әдепкі мәндер тек жаңа көріністер мен құжаттарға қолданылады. Ағымдағы жұмыс пен сақталған жобалар өзгермейді.",
            "Тіл, ауысулар және аяқталған журналды қарау уақыты сақтағаннан кейін қолданылады.",
            "Интерфейстің бірқалыпты ауысулары", "Аяқталған журналды қарау уақыты",
            "Бастапқы экран", "Іске қосу экранының кезеңдері", "Әдепкі масштаб", "Бастапқыда ашық бөлімдер",
            "Нобайды қалпына келтіру (жүйе тілі)",
            "Баптаулар сақталмады. studio_config.ini файлы мен қалтасына жазу рұқсатын тексеріңіз. Өзгерістер қолданылмады.",
            "Баптауларды сақтау және файлдағы алдыңғы тілді қалпына келтіру мүмкін болмады. Рұқсаттарды тексеріп, қайта сақтаңыз."
        }},
        {Language::German, {
            "Einstellungen", "Allgemein", "Oberfläche / Emulator",
            "Vorgaben gelten nur für neue Ansichten und Dokumente. Bestehende Arbeit und gespeicherte Projekte bleiben unverändert.",
            "Sprache, Übergänge und Anzeigedauer abgeschlossener Protokolle werden beim Speichern übernommen.",
            "Sanfte Oberflächenübergänge", "Anzeigedauer abgeschlossener Protokolle",
            "Anfangs ausgewählter Bildschirm", "Startbildschirm-Phasen", "Standardzoom", "Anfangs aufgeklappte Abschnitte",
            "Entwurf zurücksetzen (Systemsprache)",
            "Einstellungen konnten nicht gespeichert werden. Schreibrechte für studio_config.ini und den Ordner prüfen. Änderungen wurden nicht übernommen.",
            "Speichern und Wiederherstellen der vorherigen Sprache in der Datei fehlgeschlagen. Berechtigungen prüfen und erneut speichern."
        }},
        {Language::Spanish, {
            "Ajustes", "General", "Interfaz / Emulador",
            "Los valores predeterminados solo afectan a vistas y documentos nuevos. No cambian el trabajo actual ni los proyectos guardados.",
            "El idioma, las transiciones y el tiempo de revisión del registro se aplican al guardar.",
            "Transiciones suaves de la interfaz", "Tiempo de revisión del registro finalizado",
            "Pantalla seleccionada al abrir", "Etapas de la pantalla de inicio", "Zoom predeterminado", "Secciones abiertas al iniciar",
            "Restablecer borrador (idioma del sistema)",
            "No se pudieron guardar los ajustes. Compruebe los permisos de studio_config.ini y su carpeta. No se aplicaron los cambios.",
            "No se pudieron guardar los ajustes ni restaurar el idioma anterior en el archivo. Compruebe los permisos y guarde de nuevo."
        }},
        {Language::French, {
            "Paramètres", "Général", "Interface / Émulateur",
            "Les valeurs par défaut concernent uniquement les nouvelles vues et les nouveaux documents. Le travail actuel et les projets enregistrés restent inchangés.",
            "La langue, les transitions et la durée de consultation du journal terminé s'appliquent à l'enregistrement.",
            "Transitions fluides de l'interface", "Durée de consultation du journal terminé",
            "Écran sélectionné à l'ouverture", "Étapes de l'écran de démarrage", "Zoom par défaut", "Sections dépliées à l'ouverture",
            "Réinitialiser le brouillon (langue système)",
            "Impossible d'enregistrer les paramètres. Vérifiez les droits sur studio_config.ini et son dossier. Les modifications n'ont pas été appliquées.",
            "Impossible d'enregistrer les paramètres et de restaurer l'ancienne langue dans le fichier. Vérifiez les droits et réessayez."
        }},
        {Language::Belarusian, {
            "Налады", "Агульныя", "Інтэрфейс / Эмулятар",
            "Прадвызначаныя значэнні — толькі для новых выглядаў і дакументаў. Бягучая праца і захаваныя праекты не зменяцца.",
            "Мова, пераходы і час прагляду завершанага журнала ўжываюцца пасля захавання.",
            "Плаўныя пераходы інтэрфейсу", "Прагляд завершанага журнала",
            "Пачатковы экран", "Этапы экрана запуску", "Прадвызначаны маштаб", "Разгорнутыя раздзелы пры адкрыцці",
            "Скінуць чарнавік (мова сістэмы)",
            "Не ўдалося захаваць налады. Праверце доступ да studio_config.ini і яго папкі. Змены не ўжытыя.",
            "Не ўдалося захаваць налады і аднавіць ранейшую мову ў файле. Праверце дазволы і захавайце зноў."
        }},
        {Language::Uzbek, {
            "Sozlamalar", "Umumiy", "Interfeys / Emulyator",
            "Standart qiymatlar faqat yangi ko‘rinish va hujjatlarga qo‘llanadi. Joriy ish va saqlangan loyihalar o‘zgarmaydi.",
            "Til, o‘tishlar va tugallangan jurnalni ko‘rish vaqti saqlanganda qo‘llanadi.",
            "Interfeysning silliq o‘tishlari", "Tugallangan jurnalni ko‘rish vaqti",
            "Dastlab tanlangan ekran", "Ishga tushish ekrani bosqichlari", "Standart masshtab", "Dastlab ochiq bo‘limlar",
            "Qoralamani tiklash (tizim tili)",
            "Sozlamalar saqlanmadi. studio_config.ini va uning jildiga yozish huquqini tekshiring. O‘zgarishlar qo‘llanmadi.",
            "Sozlamalarni saqlash va fayldagi avvalgi tilni tiklash imkonsiz. Ruxsatlarni tekshirib, qayta saqlang."
        }},
        {Language::Azerbaijani, {
            "Parametrlər", "Ümumi", "İnterfeys / Emulyator",
            "Standart dəyərlər yalnız yeni görünüş və sənədlərə tətbiq olunur. Cari iş və saxlanmış layihələr dəyişmir.",
            "Dil, keçidlər və tamamlanmış jurnala baxış müddəti saxlayarkən tətbiq olunur.",
            "İnterfeysin hamar keçidləri", "Tamamlanmış jurnala baxış müddəti",
            "İlkin seçilmiş ekran", "Başlanğıc ekranının mərhələləri", "Standart miqyas", "İlkin açıq bölmələr",
            "Qaralamanı sıfırla (sistem dili)",
            "Parametrlər saxlanmadı. studio_config.ini və qovluğuna yazma icazəsini yoxlayın. Dəyişikliklər tətbiq olunmadı.",
            "Parametrləri saxlamaq və fayldakı əvvəlki dili bərpa etmək mümkün olmadı. İcazələri yoxlayıb yenidən saxlayın."
        }},
        {Language::Kyrgyz, {
            "Жөндөөлөр", "Жалпы", "Интерфейс / Эмулятор",
            "Демейки маанилер жаңы көрүнүштөргө жана документтерге гана колдонулат. Учурдагы иш жана сакталган долбоорлор өзгөрбөйт.",
            "Тил, өтүүлөр жана бүткөн журналды кароо убактысы сактагандан кийин колдонулат.",
            "Интерфейстин жумшак өтүүлөрү", "Бүткөн журналды кароо убактысы",
            "Баштапкы экран", "Ишке кирүү экранынын этаптары", "Демейки масштаб", "Башында ачык бөлүмдөр",
            "Долбоорду баштапкыга кайтаруу (система тили)",
            "Жөндөөлөр сакталган жок. studio_config.ini жана папкасына жазуу укугун текшериңиз. Өзгөртүүлөр колдонулган жок.",
            "Жөндөөлөрдү сактоо жана файлдагы мурунку тилди калыбына келтирүү мүмкүн болгон жок. Уруксаттарды текшерип, кайра сактаңыз."
        }},
        {Language::Tajik, {
            "Танзимот", "Умумӣ", "Интерфейс / Эмулятор",
            "Қиматҳои пешфарз танҳо барои намудҳо ва ҳуҷҷатҳои нав мебошанд. Кори ҷорӣ ва лоиҳаҳои сабтшуда тағйир намеёбанд.",
            "Забон, гузаришҳо ва вақти дидани сабти анҷомёфта пас аз сабт татбиқ мешаванд.",
            "Гузаришҳои ҳамвори интерфейс", "Вақти дидани сабти анҷомёфта",
            "Экрани ибтидоӣ", "Марҳилаҳои экрани оғоз", "Миқёси пешфарз", "Бахшҳои дар оғоз кушода",
            "Барқарор кардани сиёҳнавис (забони система)",
            "Танзимот сабт нашуд. Иҷозати навиштан ба studio_config.ini ва ҷузвдони онро санҷед. Тағйирот татбиқ нашуд.",
            "Сабти танзимот ва барқарор кардани забони қаблӣ дар файл номумкин шуд. Иҷозатҳоро санҷида, боз сабт кунед."
        }},
        {Language::Armenian, {
            "Կարգավորումներ", "Ընդհանուր", "Միջերես / Էմուլյատոր",
            "Լռելյայն արժեքները վերաբերում են միայն նոր տեսքերին և փաստաթղթերին։ Ընթացիկ աշխատանքն ու պահված նախագծերը չեն փոխվում։",
            "Լեզուն, անցումները և ավարտված մատյանի դիտման ժամանակը կիրառվում են պահելիս։",
            "Միջերեսի սահուն անցումներ", "Ավարտված մատյանի դիտման ժամանակը",
            "Սկզբնական էկրան", "Մեկնարկային էկրանի փուլերը", "Լռելյայն մասշտաբ", "Սկզբում բացված բաժիններ",
            "Վերակայել սևագիրը (համակարգի լեզու)",
            "Չհաջողվեց պահել կարգավորումները։ Ստուգեք studio_config.ini-ի և պանակի գրելու թույլտվությունները։ Փոփոխությունները չեն կիրառվել։",
            "Չհաջողվեց պահել կարգավորումները և վերականգնել նախկին լեզուն ֆայլում։ Ստուգեք թույլտվությունները և նորից պահեք։"
        }},
        {Language::Georgian, {
            "პარამეტრები", "ზოგადი", "ინტერფეისი / ემულატორი",
            "ნაგულისხმევი მნიშვნელობები მხოლოდ ახალ ხედებსა და დოკუმენტებს ეხება. მიმდინარე სამუშაო და შენახული პროექტები არ შეიცვლება.",
            "ენა, გადასვლები და დასრულებული ჟურნალის ნახვის დრო შენახვისას გამოიყენება.",
            "ინტერფეისის რბილი გადასვლები", "დასრულებული ჟურნალის ნახვის დრო",
            "საწყისი ეკრანი", "ჩატვირთვის ეკრანის ეტაპები", "ნაგულისხმევი მასშტაბი", "თავდაპირველად გაშლილი სექციები",
            "მონახაზის აღდგენა (სისტემის ენა)",
            "პარამეტრები ვერ შეინახა. შეამოწმეთ studio_config.ini-სა და საქაღალდის ჩაწერის უფლებები. ცვლილებები არ გამოყენებულა.",
            "პარამეტრები ვერ შეინახა და ფაილში წინა ენა ვერ აღდგა. შეამოწმეთ უფლებები და ხელახლა შეინახეთ."
        }},
        {Language::Romanian, {
            "Setări", "General", "Interfață / Emulator",
            "Valorile implicite se aplică doar vizualizărilor și documentelor noi. Lucrul curent și proiectele salvate nu se modifică.",
            "Limba, tranzițiile și durata de consultare a jurnalului finalizat se aplică la salvare.",
            "Tranziții fluide ale interfeței", "Durata consultării jurnalului finalizat",
            "Ecranul selectat inițial", "Etapele ecranului de pornire", "Zoom implicit", "Secțiuni extinse inițial",
            "Resetează schița (limba sistemului)",
            "Setările nu au putut fi salvate. Verificați drepturile de scriere pentru studio_config.ini și dosarul său. Modificările nu au fost aplicate.",
            "Setările nu au putut fi salvate și limba anterioară nu a putut fi restaurată în fișier. Verificați permisiunile și salvați din nou."
        }}
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
}

static void InitEffectDirectionTranslations() {
    constexpr Str keys[]{Str::PatchWipe, Str::PatchSoftWipe, Str::PatchSlide, Str::PatchBlinds,
        Str::PatchDirection, Str::PatchDirectionDown, Str::PatchDirectionUp,
        Str::PatchDirectionRight, Str::PatchDirectionLeft, Str::PatchDirectionDownRight,
        Str::PatchDirectionDownLeft, Str::PatchDirectionUpRight, Str::PatchDirectionUpLeft};
    struct Translation { Language language; const char* text[13]; };
    static constexpr Translation translations[]{
        {Language::English, {"Hard wipe", "Soft wipe", "Slide", "Blinds", "Direction",
            "Down", "Up", "Right", "Left", "Down-right", "Down-left", "Up-right", "Up-left"}},
        {Language::Russian, {"Жёсткая шторка", "Мягкая шторка", "Сдвиг", "Жалюзи", "Направление",
            "Вниз", "Вверх", "Вправо", "Влево", "Вниз-вправо", "Вниз-влево", "Вверх-вправо", "Вверх-влево"}},
        {Language::Ukrainian, {"Жорстка шторка", "М’яка шторка", "Зсув", "Жалюзі", "Напрямок",
            "Вниз", "Вгору", "Праворуч", "Ліворуч", "Вниз-праворуч", "Вниз-ліворуч", "Вгору-праворуч", "Вгору-ліворуч"}},
        {Language::Belarusian, {"Жорсткая шторка", "Мяккая шторка", "Зрух", "Жалюзі", "Напрамак",
            "Уніз", "Уверх", "Управа", "Улева", "Уніз-управа", "Уніз-улева", "Уверх-управа", "Уверх-улева"}},
        {Language::Kazakh, {"Қатты перде", "Жұмсақ перде", "Сырғу", "Жалюзи", "Бағыт",
            "Төмен", "Жоғары", "Оңға", "Солға", "Төмен-оңға", "Төмен-солға", "Жоғары-оңға", "Жоғары-солға"}},
        {Language::Uzbek, {"Qattiq parda", "Yumshoq parda", "Siljish", "Jalyuzi", "Yo‘nalish",
            "Pastga", "Yuqoriga", "O‘ngga", "Chapga", "Pastga-o‘ngga", "Pastga-chapga", "Yuqoriga-o‘ngga", "Yuqoriga-chapga"}},
        {Language::Azerbaijani, {"Sərt pərdə", "Yumşaq pərdə", "Sürüşmə", "Jalüz", "İstiqamət",
            "Aşağı", "Yuxarı", "Sağa", "Sola", "Aşağı-sağa", "Aşağı-sola", "Yuxarı-sağa", "Yuxarı-sola"}},
        {Language::Kyrgyz, {"Катуу көшөгө", "Жумшак көшөгө", "Жылдыруу", "Жалюзи", "Багыт",
            "Ылдый", "Өйдө", "Оңго", "Солго", "Ылдый-оңго", "Ылдый-солго", "Өйдө-оңго", "Өйдө-солго"}},
        {Language::Tajik, {"Пардаи сахт", "Пардаи нарм", "Лағжиш", "Жалюзи", "Самт",
            "Ба поён", "Ба боло", "Ба рост", "Ба чап", "Ба поён-рост", "Ба поён-чап", "Ба боло-рост", "Ба боло-чап"}},
        {Language::Armenian, {"Կոշտ վարագույր", "Փափուկ վարագույր", "Սահք", "Շերտավարագույր", "Ուղղություն",
            "Ներքև", "Վերև", "Աջ", "Ձախ", "Ներքև-աջ", "Ներքև-ձախ", "Վերև-աջ", "Վերև-ձախ"}},
        {Language::Georgian, {"მკვეთრი ფარდა", "რბილი ფარდა", "სრიალი", "ჟალუზი", "მიმართულება",
            "ქვემოთ", "ზემოთ", "მარჯვნივ", "მარცხნივ", "ქვემოთ-მარჯვნივ", "ქვემოთ-მარცხნივ", "ზემოთ-მარჯვნივ", "ზემოთ-მარცხნივ"}},
        {Language::Romanian, {"Ștergere fermă", "Ștergere lină", "Glisare", "Jaluzele", "Direcție",
            "Jos", "Sus", "Dreapta", "Stânga", "Jos-dreapta", "Jos-stânga", "Sus-dreapta", "Sus-stânga"}},
        {Language::Polish, {"Ostre odsłanianie", "Łagodne odsłanianie", "Przesuwanie", "Żaluzje", "Kierunek",
            "W dół", "W górę", "W prawo", "W lewo", "W dół i w prawo", "W dół i w lewo", "W górę i w prawo", "W górę i w lewo"}},
        {Language::German, {"Hartes Wischen", "Weiches Wischen", "Schieben", "Jalousien", "Richtung",
            "Abwärts", "Aufwärts", "Rechts", "Links", "Abwärts-rechts", "Abwärts-links", "Aufwärts-rechts", "Aufwärts-links"}},
        {Language::Spanish, {"Barrido duro", "Barrido suave", "Deslizamiento", "Persianas", "Dirección",
            "Abajo", "Arriba", "Derecha", "Izquierda", "Abajo-derecha", "Abajo-izquierda", "Arriba-derecha", "Arriba-izquierda"}},
        {Language::French, {"Balayage net", "Balayage doux", "Glissement", "Stores", "Direction",
            "Bas", "Haut", "Droite", "Gauche", "Bas-droite", "Bas-gauche", "Haut-droite", "Haut-gauche"}},
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
}

static void InitPatchPasteTranslations() {
    constexpr Str keys[]{Str::PatchPasteEntranceEffect, Str::PatchPasteExitEffect, Str::PatchPasteDurations,
        Str::PatchHistogramComposition};
    struct Translation { Language language; const char* text[4]; };
    static constexpr Translation translations[]{
        {Language::English, {"Entrance effect", "Exit effect", "Durations", "Composed image histogram"}},
        {Language::Russian, {"Эффект появления", "Эффект исчезновения", "Длительности", "Гистограмма готового изображения"}},
        {Language::Ukrainian, {"Ефект появи", "Ефект зникнення", "Тривалості", "Гістограма готового зображення"}},
        {Language::Belarusian, {"Эфект з'яўлення", "Эфект знікнення", "Працягласці", "Гістаграма гатовага відарыса"}},
        {Language::Kazakh, {"Пайда болу әсері", "Жоғалу әсері", "Ұзақтықтар", "Дайын кескін гистограммасы"}},
        {Language::Uzbek, {"Paydo bo‘lish effekti", "Yo‘qolish effekti", "Davomiyliklar", "Tayyor tasvir gistogrammasi"}},
        {Language::Azerbaijani, {"Giriş effekti", "Çıxış effekti", "Müddətlər", "Hazır təsvirin histoqramı"}},
        {Language::Kyrgyz, {"Пайда болуу эффекти", "Жоголуу эффекти", "Узактыктар", "Даяр сүрөттүн гистограммасы"}},
        {Language::Tajik, {"Эффекти пайдоиш", "Эффекти нопадидшавӣ", "Давомнокиҳо", "Гистограммаи тасвири тайёр"}},
        {Language::Armenian, {"Հայտնվելու էֆեկտ", "Անհետանալու էֆեկտ", "Տևողություններ", "Պատրաստի պատկերի հիստոգրամ"}},
        {Language::Georgian, {"გამოჩენის ეფექტი", "გაქრობის ეფექტი", "ხანგრძლივობები", "მზა გამოსახულების ჰისტოგრამა"}},
        {Language::Romanian, {"Efect de intrare", "Efect de ieșire", "Durate", "Histograma imaginii compuse"}},
        {Language::Polish, {"Efekt wejścia", "Efekt wyjścia", "Czasy trwania", "Histogram gotowego obrazu"}},
        {Language::German, {"Einblendeffekt", "Ausblendeffekt", "Dauern", "Histogramm des fertigen Bildes"}},
        {Language::Spanish, {"Efecto de entrada", "Efecto de salida", "Duraciones", "Histograma de la imagen compuesta"}},
        {Language::French, {"Effet d’entrée", "Effet de sortie", "Durées", "Histogramme de l’image composée"}},
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
}

static void InitDeviceShortcutTranslations() {
    constexpr Str keys[]{Str::DeviceShortcuts, Str::DeviceShortcutHold, Str::DeviceShortcutPower, Str::DeviceShortcutMode, Str::DeviceShortcutLock, Str::DeviceShortcutClear, Str::DeviceShortcutChipId, Str::DeviceShortcutHistory, Str::DeviceShortcutClock, Str::DeviceShortcutToggleHistory, Str::DeviceShortcutExit, Str::DeviceShortcutClockField, Str::DeviceShortcutMain, Str::DeviceShortcutHistoryContext, Str::DeviceShortcutClockContext};
    struct Translation { Language language; const char* text[15]; };
    static constexpr Translation translations[]{
        {Language::English, {"Native shortcuts", "hold", "Power on/off", "Change mode", "Lock/unlock buttons", "Clear puff counter", "CHIP ID / version", "Open history", "Set clock", "History / CHIP ID", "Return to Main", "Select hours/minutes", "Main screen", "History page", "Clock setup"}},
        {Language::Russian, {"Родные комбинации", "удерживать", "Включение/выключение", "Сменить режим", "Заблокировать/разблокировать кнопки", "Сбросить счётчик тяг", "CHIP ID / версия", "Открыть историю", "Настроить часы", "История / CHIP ID", "Вернуться на главный экран", "Выбрать часы/минуты", "Главный экран", "Экран истории", "Настройка часов"}},
        {Language::Ukrainian, {"Штатні комбінації", "утримувати", "Увімкнення/вимкнення", "Змінити режим", "Заблокувати/розблокувати кнопки", "Скинути лічильник затяжок", "CHIP ID / версія", "Відкрити історію", "Налаштувати годинник", "Історія / CHIP ID", "Повернутися на головний екран", "Вибрати години/хвилини", "Головний екран", "Екран історії", "Налаштування годинника"}},
        {Language::Polish, {"Fabryczne skróty", "przytrzymaj", "Włącz/wyłącz", "Zmień tryb", "Zablokuj/odblokuj przyciski", "Wyzeruj licznik zaciągnięć", "CHIP ID / wersja", "Otwórz historię", "Ustaw zegar", "Historia / CHIP ID", "Wróć do ekranu głównego", "Wybierz godziny/minuty", "Ekran główny", "Historia", "Ustawianie zegara"}},
        {Language::Kazakh, {"Штаттық тіркесімдер", "ұстап тұру", "Қосу/өшіру", "Режимді ауыстыру", "Түймелерді құлыптау/ашу", "Тарту санауышын тазалау", "CHIP ID / нұсқа", "Тарихты ашу", "Сағатты орнату", "Тарих / CHIP ID", "Негізгі экранға оралу", "Сағат/минут таңдау", "Негізгі экран", "Тарих экраны", "Сағатты орнату"}},
        {Language::German, {"Native Tastenkombinationen", "halten", "Ein-/Ausschalten", "Modus wechseln", "Tasten sperren/entsperren", "Zugzähler zurücksetzen", "CHIP ID / Version", "Verlauf öffnen", "Uhr einstellen", "Verlauf / CHIP ID", "Zur Hauptansicht", "Stunden/Minuten wählen", "Hauptansicht", "Verlauf", "Uhr einstellen"}},
        {Language::Spanish, {"Combinaciones originales", "mantener", "Encender/apagar", "Cambiar modo", "Bloquear/desbloquear botones", "Borrar contador de caladas", "CHIP ID / versión", "Abrir historial", "Ajustar reloj", "Historial / CHIP ID", "Volver a la pantalla principal", "Elegir horas/minutos", "Pantalla principal", "Historial", "Ajuste del reloj"}},
        {Language::French, {"Raccourcis d’origine", "maintenir", "Marche/arrêt", "Changer de mode", "Verrouiller/déverrouiller les boutons", "Réinitialiser le compteur de bouffées", "CHIP ID / version", "Ouvrir l’historique", "Régler l’horloge", "Historique / CHIP ID", "Revenir à l’écran principal", "Choisir heures/minutes", "Écran principal", "Historique", "Réglage de l’horloge"}},
        {Language::Belarusian, {"Штатныя спалучэнні", "утрымліваць", "Уключэнне/выключэнне", "Змяніць рэжым", "Заблакаваць/разблакаваць кнопкі", "Скінуць лічыльнік зацяжак", "CHIP ID / версія", "Адкрыць гісторыю", "Наладзіць гадзіннік", "Гісторыя / CHIP ID", "Вярнуцца на галоўны экран", "Выбраць гадзіны/хвіліны", "Галоўны экран", "Экран гісторыі", "Налада гадзінніка"}},
        {Language::Uzbek, {"Asl tugma birikmalari", "ushlab turing", "Yoqish/o‘chirish", "Rejimni almashtirish", "Tugmalarni qulflash/ochish", "Tortish hisoblagichini tozalash", "CHIP ID / versiya", "Tarixni ochish", "Soatni sozlash", "Tarix / CHIP ID", "Asosiy ekranga qaytish", "Soat/daqiqani tanlash", "Asosiy ekran", "Tarix ekrani", "Soatni sozlash"}},
        {Language::Azerbaijani, {"Orijinal düymə birləşmələri", "saxlayın", "Yandır/söndür", "Rejimi dəyiş", "Düymələri kilidlə/aç", "Çəkmə sayğacını sıfırla", "CHIP ID / versiya", "Tarixçəni aç", "Saatı qur", "Tarixçə / CHIP ID", "Əsas ekrana qayıt", "Saat/dəqiqə seç", "Əsas ekran", "Tarixçə ekranı", "Saatın qurulması"}},
        {Language::Kyrgyz, {"Штаттык айкалыштар", "кармап туруу", "Күйгүзүү/өчүрүү", "Режимди алмаштыруу", "Баскычтарды кулпулоо/ачуу", "Тартуу эсептегичин тазалоо", "CHIP ID / нуска", "Тарыхты ачуу", "Саатты коюу", "Тарых / CHIP ID", "Башкы экранга кайтуу", "Саат/мүнөт тандоо", "Башкы экран", "Тарых экраны", "Саатты коюу"}},
        {Language::Tajik, {"Комбинатсияҳои аслӣ", "нигоҳ доред", "Фаъол/хомӯш", "Иваз кардани реҷа", "Қулф/кушодани тугмаҳо", "Тоза кардани ҳисобкунаки кашишҳо", "CHIP ID / версия", "Кушодани таърих", "Танзими соат", "Таърих / CHIP ID", "Бозгашт ба экрани асосӣ", "Интихоби соат/дақиқа", "Экрани асосӣ", "Экрани таърих", "Танзими соат"}},
        {Language::Armenian, {"Բնօրինակ համակցություններ", "պահել", "Միացնել/անջատել", "Փոխել ռեժիմը", "Կողպել/բացել կոճակները", "Զրոյացնել քաշումների հաշվիչը", "CHIP ID / տարբերակ", "Բացել պատմությունը", "Կարգավորել ժամը", "Պատմություն / CHIP ID", "Վերադառնալ գլխավոր էկրան", "Ընտրել ժամերը/րոպեները", "Գլխավոր էկրան", "Պատմություն", "Ժամի կարգավորում"}},
        {Language::Georgian, {"მშობლიური კომბინაციები", "გეჭიროთ", "ჩართვა/გამორთვა", "რეჟიმის შეცვლა", "ღილაკების დაბლოკვა/განბლოკვა", "ჩასუნთქვების მრიცხველის განულება", "CHIP ID / ვერსია", "ისტორიის გახსნა", "საათის დაყენება", "ისტორია / CHIP ID", "მთავარ ეკრანზე დაბრუნება", "საათების/წუთების არჩევა", "მთავარი ეკრანი", "ისტორიის ეკრანი", "საათის დაყენება"}},
        {Language::Romanian, {"Combinații originale", "ține apăsat", "Pornire/oprire", "Schimbă modul", "Blochează/deblochează butoanele", "Resetează contorul de pufuri", "CHIP ID / versiune", "Deschide istoricul", "Reglează ceasul", "Istoric / CHIP ID", "Revino la ecranul principal", "Selectează ore/minute", "Ecran principal", "Istoric", "Reglarea ceasului"}},
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
}

static void InitDeviceMenuTranslations() {
    constexpr Str keys[]{
        Str::PatchDeviceMenu, Str::PatchMenuUnsupported, Str::PatchMenuEnabled,
        Str::PatchMenuDefaultVoltage, Str::PatchMenuBattery, Str::PatchMenuOutput,
        Str::PatchMenuGesture, Str::PatchMenuControls, Str::PatchMenuInfo, Str::PatchMenuRuntime,
        Str::PatchMemoryMenu, Str::DeviceShortcutWakeStop, Str::DeviceShortcutClockAdjust,
        Str::DevicePatchedShortcuts, Str::DeviceShortcutMenu, Str::DeviceAirflow,
        Str::PatchMenuDirection, Str::PatchMenuPlusDown, Str::PatchMenuPlusUp,
        Str::PatchMenuItems, Str::PatchMenuInformation, Str::PatchMenuVoltage, Str::PatchMenuExit,
        Str::PatchMenuMandatory, Str::PatchMenuMoveUp, Str::PatchMenuMoveDown
    };
    struct Translation { Language language; const char* text[26]; };
    static constexpr Translation translations[]{
        {Language::English, {
            "Device menu", "Available for bundled DRAG S Score and Factory hardware revisions only.",
            "Enable additional device menu", "Initial voltage display", "Battery", "At the coil",
            "Main screen only: briefly press and release each button: + − + − +. Simultaneous +/− and original shortcuts stay unchanged.",
            "Physical +/− buttons follow the selected navigation direction; Fire activates the selected menu item. Physical Fire and automatic draw cannot heat while the menu is open.",
            "INFO is read-only: native battery voltage (mV), coil resistance (mΩ), and last puff duration (cs). Stock score and puff counters are not replaced.",
            "The voltage choice can be changed on the device and can reset after a cold reset or battery removal. The project saves menu enablement and the initial choice.",
            "Δ Device menu", "Stop output / wake display (no menu)", "Adjust selected hours/minutes",
            "Patch shortcuts", "Open device menu", "Airflow sensor",
            "Physical button navigation", "+ down, − up", "+ up, − down",
            "Menu items (top to bottom)", "Information", "Voltage", "Exit", "Required",
            "Move up", "Move down"}},
        {Language::Russian, {
            "Меню устройства", "Доступно только для встроенных ревизий DRAG S Score и Factory.",
            "Включить дополнительное меню", "Начальное отображение напряжения", "Батарея", "На испарителе",
            "Только на главном экране: коротко нажмите и отпустите каждую кнопку: + − + − +. Одновременные +/− и штатные комбинации не меняются.",
            "Физические кнопки +/− следуют выбранному направлению; Fire — выполнить выбранный пункт меню. Кнопка Fire и автозатяжка не включают нагрев, пока меню открыто.",
            "INFO только для чтения: родные напряжение батареи (мВ), сопротивление спирали (мОм) и длительность последней тяги (сс). Штатные счётчики очков и тяг не заменяются.",
            "Выбор напряжения можно менять на устройстве; он может сброситься при холодном сбросе или извлечении батареи. Проект сохраняет включение меню и начальный выбор.",
            "Δ Меню устройства", "Остановить нагрев / разбудить экран; без меню", "Изменить выбранные часы/минуты",
            "Комбинации патча", "Открыть меню устройства", "Датчик затяжки",
            "Направление физических кнопок", "+ вниз, − вверх", "+ вверх, − вниз",
            "Пункты меню (сверху вниз)", "Информация", "Напряжение", "Выход", "Обязательный",
            "Переместить вверх", "Переместить вниз"}},
        {Language::Ukrainian, {
            "Меню пристрою", "Доступно лише для вбудованих ревізій DRAG S Score і Factory.",
            "Увімкнути додаткове меню", "Початкове відображення напруги", "Батарея", "На випарнику",
            "Лише на головному екрані: коротко натисніть і відпустіть кожну кнопку: + − + − +. Одночасні +/− та штатні комбінації не змінюються.",
            "Фізичні кнопки +/− діють у вибраному напрямку; Fire — виконати вибраний пункт меню. Fire та автозатяжка не вмикають нагрівання, поки меню відкрите.",
            "INFO лише для читання: рідні напруга батареї (мВ), опір спіралі (мОм) і тривалість останньої затяжки (сс). Штатні лічильники очок і затяжок не замінюються.",
            "Вибір напруги можна змінити на пристрої; він може скинутися після холодного скидання або виймання батареї. Проєкт зберігає ввімкнення меню та початковий вибір.",
            "Δ Меню пристрою", "Зупинити нагрівання / розбудити екран; без меню", "Змінити вибрані години/хвилини",
            "Комбінації патча", "Відкрити меню пристрою", "Датчик затяжки",
            "Напрямок фізичних кнопок", "+ вниз, − вгору", "+ вгору, − вниз",
            "Пункти меню (згори вниз)", "Інформація", "Напруга", "Вихід", "Обов’язковий",
            "Перемістити вгору", "Перемістити вниз"}},
        {Language::Polish, {
            "Menu urządzenia", "Dostępne tylko dla wbudowanych rewizji DRAG S Score i Factory.",
            "Włącz dodatkowe menu", "Początkowy odczyt napięcia", "Bateria", "Na grzałce",
            "Tylko ekran główny: krótko naciśnij i zwolnij każdy przycisk: + − + − +. Jednoczesne +/− i fabryczne skróty pozostają bez zmian.",
            "Fizyczne przyciski +/− działają w wybranym kierunku; Fire uruchamia wybraną pozycję menu. Fire i automatyczne zaciąganie nie grzeją, gdy menu jest otwarte.",
            "INFO jest tylko do odczytu: natywne napięcie baterii (mV), opór grzałki (mΩ) i czas ostatniego zaciągnięcia (cs). Fabryczne liczniki punktów i zaciągnięć nie są zastępowane.",
            "Napięcie można przełączyć na urządzeniu; wybór może się zresetować po zimnym resecie lub wyjęciu baterii. Projekt zapisuje włączenie menu i wybór początkowy.",
            "Δ Menu urządzenia", "Zatrzymaj grzanie / obudź ekran (bez menu)", "Zmień wybrane godziny/minuty",
            "Skróty poprawki", "Otwórz menu urządzenia", "Czujnik zaciągania",
            "Kierunek fizycznych przycisków", "+ w dół, − w górę", "+ w górę, − w dół",
            "Pozycje menu (od góry)", "Informacje", "Napięcie", "Wyjście", "Wymagane",
            "Przenieś w górę", "Przenieś w dół"}},
        {Language::Kazakh, {
            "Құрылғы мәзірі", "Тек кірістірілген DRAG S Score және Factory аппараттық нұсқалары үшін.",
            "Қосымша мәзірді қосу", "Бастапқы кернеу көрсеткіші", "Батарея", "Буландырғышта",
            "Тек негізгі экранда: әр түймені қысқа басып босатыңыз: + − + − +. Бір мезгілдегі +/− және штаттық тіркесімдер өзгермейді.",
            "Физикалық +/− түймелері таңдалған бағытта жылжытады; Fire таңдалған мәзір тармағын орындайды. Мәзір ашықта Fire және автоматты тарту қыздыруды қоспайды.",
            "INFO тек оқуға арналған: төл батарея кернеуі (mV), орам кедергісі (mΩ) және соңғы тарту ұзақтығы (cs). Штаттық ұпай және тарту санауыштары ауыстырылмайды.",
            "Кернеу түрін құрылғыда өзгертуге болады; суық қалпына келтіруде не батарея алынғанда таңдау қалпына келуі мүмкін. Жоба мәзірдің қосылуын және бастапқы таңдауды сақтайды.",
            "Δ Құрылғы мәзірі", "Қыздыруды тоқтату / экранды ояту (мәзірсіз)", "Таңдалған сағат/минутты өзгерту",
            "Патч тіркесімдері", "Құрылғы мәзірін ашу", "Тарту датчигі",
            "Физикалық түймелердің бағыты", "+ төмен, − жоғары", "+ жоғары, − төмен",
            "Мәзір тармақтары (жоғарыдан төмен)", "Ақпарат", "Кернеу", "Шығу", "Міндетті",
            "Жоғары жылжыту", "Төмен жылжыту"}},
        {Language::German, {
            "Gerätemenü", "Nur für die enthaltenen DRAG S Score- und Factory-Hardwarerevisionen.",
            "Zusätzliches Gerätemenü aktivieren", "Anfängliche Spannungsanzeige", "Akku", "Am Verdampfer",
            "Nur in der Hauptansicht: jede Taste kurz drücken und loslassen: + − + − +. Gleichzeitiges +/− und originale Tastenkombinationen bleiben unverändert.",
            "Die physischen +/−-Tasten folgen der gewählten Richtung; Fire aktiviert den gewählten Menüpunkt. Fire und Zugautomatik können bei geöffnetem Menü nicht heizen.",
            "INFO ist schreibgeschützt: native Akkuspannung (mV), Coil-Widerstand (mΩ) und letzte Zugdauer (cs). Originale Punkte- und Zugzähler werden nicht ersetzt.",
            "Die Spannungsauswahl ist am Gerät änderbar und kann bei einem Kaltstart oder beim Entfernen des Akkus zurückgesetzt werden. Das Projekt speichert Menüaktivierung und Anfangsauswahl.",
            "Δ Gerätemenü", "Heizen stoppen / Display wecken (kein Menü)", "Gewählte Stunden/Minuten ändern",
            "Patch-Tastenkombinationen", "Gerätemenü öffnen", "Zugsensor",
            "Navigation mit physischen Tasten", "+ abwärts, − aufwärts", "+ aufwärts, − abwärts",
            "Menüpunkte (von oben nach unten)", "Informationen", "Spannung", "Beenden", "Erforderlich",
            "Nach oben verschieben", "Nach unten verschieben"}},
        {Language::Spanish, {
            "Menú del dispositivo", "Solo para las revisiones DRAG S Score y Factory incluidas.",
            "Activar menú adicional", "Lectura inicial de voltaje", "Batería", "En la resistencia",
            "Solo en la pantalla principal: pulsa brevemente y suelta cada botón: + − + − +. +/− simultáneos y los atajos originales no cambian.",
            "Los botones físicos +/− siguen la dirección elegida; Fire activa el elemento seleccionado. Fire y la calada automática no pueden calentar mientras el menú está abierto.",
            "INFO es de solo lectura: voltaje nativo de batería (mV), resistencia de bobina (mΩ) y duración de la última calada (cs). No se sustituyen los contadores originales de puntos y caladas.",
            "La selección de voltaje se cambia en el dispositivo y puede reiniciarse tras un reinicio en frío o al retirar la batería. El proyecto guarda la activación del menú y la selección inicial.",
            "Δ Menú del dispositivo", "Detener calentamiento / despertar pantalla (sin menú)", "Ajustar horas/minutos seleccionados",
            "Atajos del parche", "Abrir menú del dispositivo", "Sensor de calada",
            "Dirección de los botones físicos", "+ abajo, − arriba", "+ arriba, − abajo",
            "Elementos del menú (de arriba abajo)", "Información", "Voltaje", "Salir", "Obligatorio",
            "Mover arriba", "Mover abajo"}},
        {Language::French, {
            "Menu de l’appareil", "Réservé aux révisions DRAG S Score et Factory intégrées.",
            "Activer le menu supplémentaire", "Affichage initial de tension", "Batterie", "À la résistance",
            "Écran principal uniquement : appuyez brièvement puis relâchez chaque bouton : + − + − +. +/− simultanés et les raccourcis d’origine restent inchangés.",
            "Les boutons physiques +/− suivent le sens choisi ; Fire active l’élément sélectionné. Fire et le tirage automatique ne chauffent pas tant que le menu est ouvert.",
            "INFO est en lecture seule : tension native de batterie (mV), résistance de bobine (mΩ) et durée de la dernière bouffée (cs). Les compteurs d’origine de points et de bouffées ne sont pas remplacés.",
            "La tension se choisit sur l’appareil ; ce choix peut être réinitialisé au redémarrage à froid ou au retrait de la batterie. Le projet enregistre l’activation du menu et le choix initial.",
            "Δ Menu de l’appareil", "Arrêter la chauffe / réveiller l’écran (sans menu)", "Régler les heures/minutes sélectionnées",
            "Raccourcis du patch", "Ouvrir le menu de l’appareil", "Capteur de tirage",
            "Sens des boutons physiques", "+ bas, − haut", "+ haut, − bas",
            "Éléments du menu (de haut en bas)", "Informations", "Tension", "Quitter", "Obligatoire",
            "Monter", "Descendre"}},
        {Language::Belarusian, {
            "Меню прылады", "Толькі для ўбудаваных рэвізій DRAG S Score і Factory.",
            "Уключыць дадатковае меню", "Пачатковае адлюстраванне напружання", "Батарэя", "На выпарніку",
            "Толькі на галоўным экране: коратка націсніце і адпусціце кожную кнопку: + − + − +. Адначасовыя +/− і штатныя спалучэнні не змяняюцца.",
            "Фізічныя кнопкі +/− дзейнічаюць у выбраным кірунку; Fire — выканаць выбраны пункт меню. Fire і аўтазацяжка не ўключаюць нагрэў, пакуль меню адкрыта.",
            "INFO толькі для чытання: родныя напружанне батарэі (мВ), супраціў спіралі (мОм) і працягласць апошняй зацяжкі (сс). Штатныя лічыльнікі ачкоў і зацяжак не замяняюцца.",
            "Выбар напружання можна змяніць на прыладзе; ён можа скінуцца пры халодным скідзе або выманні батарэі. Праект захоўвае ўключэнне меню і пачатковы выбар.",
            "Δ Меню прылады", "Спыніць нагрэў / абудзіць экран; без меню", "Змяніць выбраныя гадзіны/хвіліны",
            "Спалучэнні патча", "Адкрыць меню прылады", "Датчык зацяжкі",
            "Кірунак фізічных кнопак", "+ уніз, − уверх", "+ уверх, − уніз",
            "Пункты меню (зверху ўніз)", "Інфармацыя", "Напружанне", "Выхад", "Абавязковы",
            "Перамясціць уверх", "Перамясціць уніз"}},
        {Language::Uzbek, {
            "Qurilma menyusi", "Faqat ichki DRAG S Score va Factory apparat tahrirlari uchun.",
            "Qo‘shimcha menyuni yoqish", "Boshlang‘ich kuchlanish ko‘rsatkichi", "Batareya", "Bug‘latgichda",
            "Faqat asosiy ekranda: har bir tugmani qisqa bosib qo‘yib yuboring: + − + − +. Bir vaqtdagi +/− va asl birikmalar o‘zgarmaydi.",
            "Jismoniy +/− tugmalari tanlangan yo‘nalishda yuradi; Fire tanlangan menyu bandini bajaradi. Menyu ochiqligida Fire va avtomatik tortish qizdirishni yoqmaydi.",
            "INFO faqat o‘qish uchun: asl batareya kuchlanishi (mV), spiral qarshiligi (mΩ) va oxirgi tortish davomiyligi (cs). Asl ball va tortish hisoblagichlari almashtirilmaydi.",
            "Kuchlanish tanlovi qurilmada o‘zgartiriladi; sovuq qayta ishga tushirishda yoki batareya chiqarilganda tiklanishi mumkin. Loyiha menyu yoqilganini va boshlang‘ich tanlovni saqlaydi.",
            "Δ Qurilma menyusi", "Qizdirishni to‘xtatish / ekranni uyg‘otish (menyusiz)", "Tanlangan soat/daqiqani o‘zgartirish",
            "Patch birikmalari", "Qurilma menyusini ochish", "Tortish sensori",
            "Jismoniy tugmalar yo‘nalishi", "+ pastga, − yuqoriga", "+ yuqoriga, − pastga",
            "Menyu bandlari (yuqoridan pastga)", "Ma’lumot", "Kuchlanish", "Chiqish", "Majburiy",
            "Yuqoriga ko‘chirish", "Pastga ko‘chirish"}},
        {Language::Azerbaijani, {
            "Cihaz menyusu", "Yalnız daxili DRAG S Score və Factory aparat versiyaları üçün.",
            "Əlavə menyunu aktivləşdir", "İlkin gərginlik göstəricisi", "Batareya", "Buxarlandırıcıda",
            "Yalnız əsas ekranda: hər düyməni qısa basıb buraxın: + − + − +. Eyni vaxtda +/− və orijinal birləşmələr dəyişmir.",
            "Fiziki +/− düymələri seçilmiş istiqamətdə hərəkət edir; Fire seçilmiş menyu elementini işə salır. Menyu açıqkən Fire və avtomatik çəkmə qızdırmanı işə salmır.",
            "INFO yalnız oxumaq üçündür: orijinal batareya gərginliyi (mV), spiral müqaviməti (mΩ) və son çəkmənin müddəti (cs). Orijinal xal və çəkmə sayğacları əvəz edilmir.",
            "Gərginlik seçimi cihazda dəyişir; soyuq sıfırlamada və ya batareya çıxarıldıqda sıfırlana bilər. Layihə menyunun aktivliyini və ilkin seçimi saxlayır.",
            "Δ Cihaz menyusu", "Qızdırmanı dayandır / ekranı oyat (menyusuz)", "Seçilmiş saat/dəqiqəni dəyiş",
            "Patç birləşmələri", "Cihaz menyusunu aç", "Çəkmə sensoru",
            "Fiziki düymələrin istiqaməti", "+ aşağı, − yuxarı", "+ yuxarı, − aşağı",
            "Menyu elementləri (yuxarıdan aşağı)", "Məlumat", "Gərginlik", "Çıxış", "Məcburi",
            "Yuxarı köçür", "Aşağı köçür"}},
        {Language::Kyrgyz, {
            "Түзмөк менюсу", "Камтылган DRAG S Score жана Factory аппараттык нускалары үчүн гана.",
            "Кошумча менюну күйгүзүү", "Баштапкы чыңалуу көрсөткүчү", "Батарея", "Бууланткычта",
            "Башкы экранда гана: ар бир баскычты кыска басып коё бериңиз: + − + − +. Бир убактагы +/− жана штаттык айкалыштар өзгөрбөйт.",
            "Физикалык +/− баскычтары тандалган багытта жылдырат; Fire тандалган меню пунктун аткарат. Меню ачыкта Fire жана автоматтык тартуу ысытууну күйгүзбөйт.",
            "INFO окуу үчүн гана: түпкү батарея чыңалуусу (mV), спираль каршылыгы (mΩ) жана акыркы тартуу узактыгы (cs). Штаттык упай жана тартуу эсептегичтери алмаштырылбайт.",
            "Чыңалуу тандоосун түзмөктө өзгөртсө болот; муздак кайра коюуда же батарея алынганда баштапкыга кайтышы мүмкүн. Долбоор менюну күйгүзүүнү жана баштапкы тандоону сактайт.",
            "Δ Түзмөк менюсу", "Ысытууну токтотуу / экранды ойготуу (менюсуз)", "Тандалган саат/мүнөттү өзгөртүү",
            "Патч айкалыштары", "Түзмөк менюсун ачуу", "Тартуу сенсору",
            "Физикалык баскычтардын багыты", "+ ылдый, − өйдө", "+ өйдө, − ылдый",
            "Меню пункттары (өйдөдөн ылдый)", "Маалымат", "Чыңалуу", "Чыгуу", "Милдеттүү",
            "Өйдө жылдыруу", "Ылдый жылдыруу"}},
        {Language::Tajik, {
            "Менюи дастгоҳ", "Танҳо барои нусхаҳои сахтафзории дарунсохти DRAG S Score ва Factory.",
            "Фаъол кардани менюи иловагӣ", "Намоиши ибтидоии шиддат", "Батарея", "Дар бухоркунанда",
            "Танҳо дар экрани асосӣ: ҳар тугмаро кӯтоҳ пахш карда раҳо кунед: + − + − +. +/− ҳамзамон ва комбинатсияҳои аслӣ тағйир намеёбанд.",
            "Тугмаҳои ҷисмонии +/− ба самти интихобшуда ҳаракат мекунанд; Fire банди интихобшудаи менюро иҷро мекунад. Ҳангоми кушода будани меню Fire ва кашиши худкор гармкуниро фаъол намекунанд.",
            "INFO танҳо барои хондан: шиддати аслии батарея (mV), муқовимати спирал (mΩ) ва давомнокии кашиши охирин (cs). Ҳисобкунакҳои аслии холҳо ва кашишҳо иваз намешаванд.",
            "Интихоби шиддат дар дастгоҳ тағйир меёбад; баъди азнавоғозии сард ё гирифтани батарея метавонад барқарор шавад. Лоиҳа фаъолии меню ва интихоби ибтидоиро нигоҳ медорад.",
            "Δ Менюи дастгоҳ", "Қатъи гармкунӣ / бедор кардани экран (бе меню)", "Тағйири соат/дақиқаи интихобшуда",
            "Комбинатсияҳои патч", "Кушодани менюи дастгоҳ", "Сенсори кашиш",
            "Самти тугмаҳои ҷисмонӣ", "+ поён, − боло", "+ боло, − поён",
            "Бандҳои меню (аз боло ба поён)", "Маълумот", "Шиддат", "Баромад", "Ҳатмӣ",
            "Ба боло кӯчондан", "Ба поён кӯчондан"}},
        {Language::Armenian, {
            "Սարքի ընտրացանկ", "Միայն ներառված DRAG S Score և Factory ապարատային տարբերակների համար։",
            "Միացնել լրացուցիչ ընտրացանկը", "Լարման սկզբնական ցուցադրում", "Մարտկոց", "Կծիկի վրա",
            "Միայն գլխավոր էկրանին՝ կարճ սեղմեք և բաց թողեք ամեն կոճակը՝ + − + − +։ Միաժամանակյա +/− և բնօրինակ համակցությունները չեն փոխվում։",
            "Ֆիզիկական +/− կոճակները գործում են ընտրված ուղղությամբ, Fire-ն ակտիվացնում է ընտրված կետը։ Բաց ընտրացանկում Fire-ը և ավտոմատ քաշումը չեն միացնում տաքացումը։",
            "INFO-ն միայն ընթերցման համար է՝ բնօրինակ մարտկոցի լարում (mV), կծիկի դիմադրություն (mΩ) և վերջին քաշման տևողություն (cs)։ Միավորների և քաշումների բնօրինակ հաշվիչները չեն փոխարինվում։",
            "Լարման ընտրությունը փոխվում է սարքում և կարող է վերակայվել սառը վերագործարկման կամ մարտկոցը հանելու ժամանակ։ Նախագիծը պահում է ընտրացանկի միացումն ու սկզբնական ընտրությունը։",
            "Δ Սարքի ընտրացանկ", "Դադարեցնել տաքացումը / արթնացնել էկրանը (առանց ընտրացանկի)", "Փոխել ընտրված ժամերը/րոպեները",
            "Պատչի համակցություններ", "Բացել սարքի ընտրացանկը", "Քաշման սենսոր",
            "Ֆիզիկական կոճակների ուղղություն", "+ ներքև, − վերև", "+ վերև, − ներքև",
            "Ընտրացանկի կետեր (վերևից ներքև)", "Տեղեկություն", "Լարում", "Ելք", "Պարտադիր",
            "Տեղափոխել վերև", "Տեղափոխել ներքև"}},
        {Language::Georgian, {
            "მოწყობილობის მენიუ", "მხოლოდ ჩაშენებული DRAG S Score და Factory აპარატურული ვერსიებისთვის.",
            "დამატებითი მენიუს ჩართვა", "ძაბვის საწყისი ჩვენება", "ბატარეა", "აორთქლებელზე",
            "მხოლოდ მთავარ ეკრანზე: მოკლედ დააჭირეთ და გაუშვით თითოეული ღილაკი: + − + − +. ერთდროული +/− და ორიგინალური კომბინაციები უცვლელია.",
            "ფიზიკური +/− ღილაკები არჩეულ მიმართულებას მიჰყვება; Fire ააქტიურებს არჩეულ პუნქტს. გახსნილ მენიუში Fire და ავტომატური ჩასუნთქვა გათბობას არ რთავს.",
            "INFO მხოლოდ წასაკითხია: მშობლიური ბატარეის ძაბვა (mV), სპირალის წინაღობა (mΩ) და ბოლო ჩასუნთქვის ხანგრძლივობა (cs). ქულებისა და ჩასუნთქვების ორიგინალური მრიცხველები არ იცვლება.",
            "ძაბვის არჩევა მოწყობილობაზე იცვლება და ცივი გადატვირთვის ან ბატარეის ამოღებისას შეიძლება განულდეს. პროექტი ინახავს მენიუს ჩართვას და საწყის არჩევანს.",
            "Δ მოწყობილობის მენიუ", "გათბობის გაჩერება / ეკრანის გაღვიძება (მენიუს გარეშე)", "არჩეული საათების/წუთების შეცვლა",
            "პატჩის კომბინაციები", "მოწყობილობის მენიუს გახსნა", "ჩასუნთქვის სენსორი",
            "ფიზიკური ღილაკების მიმართულება", "+ ქვემოთ, − ზემოთ", "+ ზემოთ, − ქვემოთ",
            "მენიუს პუნქტები (ზემოდან ქვემოთ)", "ინფორმაცია", "ძაბვა", "გასვლა", "სავალდებულო",
            "ზემოთ გადატანა", "ქვემოთ გადატანა"}},
        {Language::Romanian, {
            "Meniul dispozitivului", "Doar pentru reviziile DRAG S Score și Factory incluse.",
            "Activează meniul suplimentar", "Afișarea inițială a tensiunii", "Baterie", "La rezistență",
            "Doar pe ecranul principal: apasă scurt și eliberează fiecare buton: + − + − +. +/− simultan și combinațiile originale rămân neschimbate.",
            "Butoanele fizice +/− urmează direcția aleasă; Fire activează elementul selectat. Fire și tragerea automată nu pot încălzi cât timp meniul este deschis.",
            "INFO este doar pentru citire: tensiunea nativă a bateriei (mV), rezistența bobinei (mΩ) și durata ultimului puf (cs). Contoarele originale de puncte și pufuri nu sunt înlocuite.",
            "Alegerea tensiunii se schimbă pe dispozitiv și se poate reseta la repornirea la rece sau scoaterea bateriei. Proiectul salvează activarea meniului și alegerea inițială.",
            "Δ Meniul dispozitivului", "Oprește încălzirea / trezește ecranul (fără meniu)", "Reglează orele/minutele selectate",
            "Combinațiile patchului", "Deschide meniul dispozitivului", "Senzor de tragere",
            "Direcția butoanelor fizice", "+ jos, − sus", "+ sus, − jos",
            "Elementele meniului (de sus în jos)", "Informații", "Tensiune", "Ieșire", "Obligatoriu",
            "Mută în sus", "Mută în jos"}},
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
}

static void InitMenuWorkspaceTranslations() {
    constexpr Str keys[]{Str::PatchMenuPreview, Str::PatchMenuSample, Str::PatchMenuFire,
        Str::PatchMenuBack, Str::PatchMenuDrag, Str::PatchMenuInfoBattery,
        Str::PatchMenuInfoResistance, Str::PatchMenuInfoDuration, Str::PatchMenuCommon,
        Str::PatchMenuTotal, Str::PatchMenuCostNote, Str::PatchMenuPreviewOnly};
    struct Translation { Language language; const char* text[12]; };
    static constexpr Translation translations[]{
        {Language::English, {"LCD preview", "SAMPLE: 4000 mV / 250 mΩ / 125 cs — not live readings",
            "Fire", "Back", "Drag the numbered handles to reorder; arrows also work.",
            "Battery (mV)", "Resistance (mΩ)", "Last puff (cs)", "Common overhead", "Menu total",
            "Bytes, including shared code and alignment once. Free Flash includes all applied patches.",
            "Preview only: click a row to select, click again or press Fire to activate."}},
        {Language::Russian, {"Предпросмотр LCD", "ПРИМЕР: 4000 мВ / 250 мОм / 125 сс — не показания устройства",
            "Fire", "Назад", "Перетаскивайте номера для изменения порядка; стрелки также работают.",
            "Батарея (мВ)", "Сопротивление (мОм)", "Последняя тяга (сс)", "Общие расходы", "Всего меню",
            "Байты: общий код и выравнивание учтены один раз. Свободная Flash учитывает все применённые патчи.",
            "Только просмотр: щёлкните строку для выбора, ещё раз или Fire — для действия."}},
        {Language::Ukrainian, {"Перегляд LCD", "ПРИКЛАД: 4000 мВ / 250 мОм / 125 сс — не показники пристрою",
            "Fire", "Назад", "Перетягуйте номери для зміни порядку; стрілки теж працюють.",
            "Батарея (мВ)", "Опір (мОм)", "Остання затяжка (сс)", "Спільні витрати", "Усього меню",
            "Байти: спільний код і вирівнювання враховано один раз. Вільна Flash враховує всі застосовані патчі.",
            "Лише перегляд: натисніть рядок для вибору, ще раз або Fire — для дії."}},
        {Language::Polish, {"Podgląd LCD", "PRZYKŁAD: 4000 mV / 250 mΩ / 125 cs — nie odczyty na żywo",
            "Fire", "Wstecz", "Przeciągnij numery, aby zmienić kolejność; strzałki też działają.",
            "Bateria (mV)", "Rezystancja (mΩ)", "Ostatnie zaciągnięcie (cs)", "Wspólny narzut", "Całe menu",
            "Bajty: wspólny kod i wyrównanie liczone raz. Wolna Flash uwzględnia wszystkie zastosowane poprawki.",
            "Tylko podgląd: kliknij wiersz, aby wybrać; kliknij ponownie lub Fire, aby uruchomić."}},
        {Language::Kazakh, {"LCD алдын ала қарау", "ҮЛГІ: 4000 mV / 250 mΩ / 125 cs — нақты уақыт деректері емес",
            "Fire", "Артқа", "Ретті өзгерту үшін нөмірлерді сүйреңіз; көрсеткілер де жұмыс істейді.",
            "Батарея (mV)", "Кедергі (mΩ)", "Соңғы тарту (cs)", "Ортақ шығын", "Мәзір жиыны",
            "Байттар: ортақ код пен туралау бір рет есептеледі. Бос Flash барлық қолданылған патчтарды қамтиды.",
            "Тек қарау: жолды таңдаңыз; қайта басыңыз не Fire арқылы іске қосыңыз."}},
        {Language::German, {"LCD-Vorschau", "BEISPIEL: 4000 mV / 250 mΩ / 125 cs — keine Live-Messwerte",
            "Fire", "Zurück", "Nummern zum Sortieren ziehen; die Pfeile funktionieren weiterhin.",
            "Akku (mV)", "Widerstand (mΩ)", "Letzter Zug (cs)", "Gemeinsamer Aufwand", "Menü gesamt",
            "Bytes: gemeinsamer Code und Ausrichtung einmal gezählt. Freier Flash berücksichtigt alle angewendeten Patches.",
            "Nur Vorschau: Zeile auswählen, erneut klicken oder mit Fire aktivieren."}},
        {Language::Spanish, {"Vista previa LCD", "EJEMPLO: 4000 mV / 250 mΩ / 125 cs — no son lecturas en vivo",
            "Fire", "Atrás", "Arrastra los números para reordenar; también puedes usar las flechas.",
            "Batería (mV)", "Resistencia (mΩ)", "Última calada (cs)", "Coste compartido", "Total del menú",
            "Bytes: código compartido y alineación contados una vez. La Flash libre incluye todos los parches aplicados.",
            "Solo vista previa: selecciona una fila; pulsa de nuevo o Fire para activarla."}},
        {Language::French, {"Aperçu LCD", "EXEMPLE : 4000 mV / 250 mΩ / 125 cs — pas de mesures en direct",
            "Fire", "Retour", "Glissez les numéros pour réordonner ; les flèches restent disponibles.",
            "Batterie (mV)", "Résistance (mΩ)", "Dernière bouffée (cs)", "Coût commun", "Total du menu",
            "Octets : code commun et alignement comptés une fois. La Flash libre inclut tous les correctifs appliqués.",
            "Aperçu uniquement : sélectionnez une ligne, recliquez ou appuyez sur Fire pour l’activer."}},
        {Language::Belarusian, {"Перадпрагляд LCD", "ПРЫКЛАД: 4000 мВ / 250 мОм / 125 сс — не паказанні прылады",
            "Fire", "Назад", "Перацягвайце нумары для змены парадку; стрэлкі таксама працуюць.",
            "Батарэя (мВ)", "Супраціў (мОм)", "Апошняя зацяжка (сс)", "Агульныя выдаткі", "Усяго меню",
            "Байты: агульны код і выраўноўванне ўлічаны адзін раз. Вольная Flash улічвае ўсе ўжытыя патчы.",
            "Толькі прагляд: выберыце радок, націсніце яшчэ раз або Fire для дзеяння."}},
        {Language::Uzbek, {"LCD ko‘rinishi", "NAMUNA: 4000 mV / 250 mΩ / 125 cs — jonli o‘lchovlar emas",
            "Fire", "Orqaga", "Tartibni o‘zgartirish uchun raqamlarni torting; strelkalar ham ishlaydi.",
            "Batareya (mV)", "Qarshilik (mΩ)", "Oxirgi tortish (cs)", "Umumiy xarajat", "Menyu jami",
            "Baytlar: umumiy kod va tekislash bir marta hisoblanadi. Bo‘sh Flash barcha qo‘llangan patchlarni hisobga oladi.",
            "Faqat ko‘rish: qatorni tanlang, yana bosing yoki Fire bilan ishga tushiring."}},
        {Language::Azerbaijani, {"LCD önizləmə", "NÜMUNƏ: 4000 mV / 250 mΩ / 125 cs — canlı ölçülər deyil",
            "Fire", "Geri", "Sıranı dəyişmək üçün nömrələri sürüşdürün; oxlar da işləyir.",
            "Batareya (mV)", "Müqavimət (mΩ)", "Son çəkmə (cs)", "Ortaq xərc", "Menyu cəmi",
            "Baytlar: ortaq kod və hizalama bir dəfə sayılır. Boş Flash bütün tətbiq edilmiş patçları nəzərə alır.",
            "Yalnız önizləmə: sətri seçin, yenidən basın və ya Fire ilə aktivləşdirin."}},
        {Language::Kyrgyz, {"LCD алдын ала көрүү", "ҮЛГҮ: 4000 mV / 250 mΩ / 125 cs — түз өлчөөлөр эмес",
            "Fire", "Артка", "Тартипти өзгөртүү үчүн сандарды сүйрөңүз; жебелер да иштейт.",
            "Батарея (mV)", "Каршылык (mΩ)", "Акыркы тартуу (cs)", "Жалпы чыгым", "Меню жыйынтыгы",
            "Байттар: жалпы код жана тегиздөө бир жолу эсептелет. Бош Flash бардык колдонулган патчтарды камтыйт.",
            "Көрүү гана: сапты тандаңыз, кайра басыңыз же Fire менен иштетиңиз."}},
        {Language::Tajik, {"Пешнамоиши LCD", "НАМУНА: 4000 mV / 250 mΩ / 125 cs — ченакҳои зинда нестанд",
            "Fire", "Бозгашт", "Барои иваз кардани тартиб рақамҳоро кашед; тирчаҳо низ кор мекунанд.",
            "Батарея (mV)", "Муқовимат (mΩ)", "Кашиши охирин (cs)", "Хароҷоти умумӣ", "Ҷамъи меню",
            "Байтҳо: коди умумӣ ва ҳамворсозӣ як бор ҳисоб мешаванд. Flash-и озод ҳамаи патчҳои татбиқшударо дар бар мегирад.",
            "Танҳо пешнамоиш: сатрро интихоб кунед, дубора пахш кунед ё Fire-ро истифода баред."}},
        {Language::Armenian, {"LCD նախադիտում", "ՕՐԻՆԱԿ՝ 4000 mV / 250 mΩ / 125 cs — ոչ իրական չափումներ",
            "Fire", "Հետ", "Քաշեք համարները՝ հերթականությունը փոխելու համար․ սլաքները նույնպես գործում են։",
            "Մարտկոց (mV)", "Դիմադրություն (mΩ)", "Վերջին քաշում (cs)", "Ընդհանուր ծախս", "Մենյուի ծավալ",
            "Բայթեր․ ընդհանուր կոդն ու հավասարեցումը հաշվվում են մեկ անգամ։ Ազատ Flash-ը ներառում է բոլոր կիրառված փոփոխությունները։",
            "Միայն նախադիտում․ ընտրեք տողը, կրկին սեղմեք կամ գործարկեք Fire-ով։"}},
        {Language::Georgian, {"LCD წინასწარი ხედი", "ნიმუში: 4000 mV / 250 mΩ / 125 cs — არა ცოცხალი მაჩვენებლები",
            "Fire", "უკან", "რიგის შესაცვლელად გადაათრიეთ ნომრები; ისრებიც მუშაობს.",
            "ბატარეა (mV)", "წინაღობა (mΩ)", "ბოლო ჩასუნთქვა (cs)", "საერთო ხარჯი", "მენიუს ჯამი",
            "ბაიტები: საერთო კოდი და გასწორება ერთხელ ითვლება. თავისუფალი Flash ყველა გამოყენებულ პატჩს ითვალისწინებს.",
            "მხოლოდ ხედი: აირჩიეთ სტრიქონი, კვლავ დააჭირეთ ან გამოიყენეთ Fire."}},
        {Language::Romanian, {"Previzualizare LCD", "EXEMPLU: 4000 mV / 250 mΩ / 125 cs — nu sunt valori în timp real",
            "Fire", "Înapoi", "Trage numerele pentru reordonare; săgețile funcționează în continuare.",
            "Baterie (mV)", "Rezistență (mΩ)", "Ultimul puf (cs)", "Cost comun", "Total meniu",
            "Octeți: codul comun și alinierea sunt numărate o singură dată. Flash liber include toate modificările aplicate.",
            "Doar previzualizare: selectează un rând, apasă din nou sau Fire pentru activare."}},
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
    constexpr Str puffKeys[]{Str::PatchMenuPuffEditor, Str::PatchMenuPuffControls,
        Str::PatchMenuPuffBounds, Str::PatchMenuPuffChanged, Str::PatchMenuPuffUnchanged, Str::PatchMenuPuffSave};
    struct PuffTranslation { Language language; const char* text[6]; };
    static constexpr PuffTranslation puffTranslations[]{
        {Language::English, {"Puff count editor",
            "Opens the current count in five zero-padded slots. + / − changes the highlighted digit without carry; Fire advances, then confirms the fifth digit.",
            "Selected revision: 00000–%05u. Digits wrap only within the editor limit.",
            "Preview sample updated; nothing was written to the device.",
            "Preview sample unchanged; nothing was written.",
            "On the device, the last Fire saves through the native firmware only if the count changed, then returns to the menu."}},
        {Language::Russian, {"Редактор счётчика затяжек",
            "Текущее число открывается в пяти разрядах с ведущими нулями. + / − меняет выделенную цифру без переноса; Fire переходит дальше и подтверждает пятую.",
            "Выбранная ревизия: 00000–%05u. Цифры циклически меняются в пределах редактора.",
            "Число в примере изменено; на устройство ничего не записано.",
            "Число в примере не изменено; записи не было.",
            "На устройстве последний Fire сохраняет изменённый счётчик штатной прошивкой и возвращает в меню; неизменённый не записывается."}},
        {Language::Ukrainian, {"Редактор лічильника затяжок",
            "Поточне число відкривається в п’яти розрядах із початковими нулями. + / − змінює виділену цифру без перенесення; Fire переходить далі й підтверджує п’яту.",
            "Вибрана ревізія: 00000–%05u. Цифри циклічно змінюються в межах редактора.",
            "Число у прикладі змінено; на пристрій нічого не записано.",
            "Число у прикладі не змінено; запису не було.",
            "На пристрої останній Fire зберігає лічильник штатною прошивкою лише після зміни та повертає до меню."}},
        {Language::Polish, {"Edytor licznika zaciągnięć",
            "Otwiera bieżącą liczbę w pięciu pozycjach z zerami wiodącymi. + / − zmienia zaznaczoną cyfrę bez przeniesienia; Fire przechodzi dalej i zatwierdza piątą.",
            "Wybrana rewizja: 00000–%05u. Cyfry zawijają się w granicach edytora.",
            "Przykładowa liczba zmieniona; nic nie zapisano na urządzeniu.",
            "Przykładowa liczba bez zmian; nic nie zapisano.",
            "Na urządzeniu ostatni Fire zapisuje licznik przez natywne oprogramowanie tylko po zmianie i wraca do menu."}},
        {Language::Kazakh, {"Тарту санағын өңдеу",
            "Ағымдағы сан бастапқы нөлдері бар бес орында ашылады. + / − белгіленген цифрды тасымалсыз өзгертеді; Fire келесіге өтіп, бесіншіні растайды.",
            "Таңдалған нұсқа: 00000–%05u. Цифрлар өңдегіш шегінде айналады.",
            "Үлгі саны өзгерді; құрылғыға ештеңе жазылған жоқ.",
            "Үлгі саны өзгермеді; ештеңе жазылған жоқ.",
            "Құрылғыда соңғы Fire сан өзгерсе ғана түпнұсқа микробағдарламамен сақтап, мәзірге қайтарады."}},
        {Language::German, {"Zugzähler bearbeiten",
            "Öffnet den aktuellen Zähler fünfstellig mit führenden Nullen. + / − ändert die markierte Ziffer ohne Übertrag; Fire geht weiter und bestätigt die fünfte.",
            "Gewählte Revision: 00000–%05u. Ziffern laufen innerhalb der Editorgrenze um.",
            "Beispielzähler geändert; nichts auf das Gerät geschrieben.",
            "Beispielzähler unverändert; nichts geschrieben.",
            "Am Gerät speichert das letzte Fire nur einen geänderten Zähler über die native Firmware und kehrt ins Menü zurück."}},
        {Language::Spanish, {"Editor del contador de caladas",
            "Abre el contador actual con cinco dígitos y ceros iniciales. + / − cambia el dígito marcado sin acarreo; Fire avanza y confirma el quinto.",
            "Revisión elegida: 00000–%05u. Los dígitos recorren el límite del editor.",
            "Contador de ejemplo actualizado; no se escribió en el dispositivo.",
            "Contador de ejemplo sin cambios; no se escribió nada.",
            "En el dispositivo, el último Fire guarda mediante el firmware nativo solo si cambió el contador y vuelve al menú."}},
        {Language::French, {"Éditeur du compteur de bouffées",
            "Ouvre le compteur actuel sur cinq chiffres avec des zéros initiaux. + / − modifie le chiffre surligné sans retenue ; Fire avance puis confirme le cinquième.",
            "Révision choisie : 00000–%05u. Les chiffres bouclent dans la limite de l’éditeur.",
            "Compteur exemple modifié ; aucune écriture sur l’appareil.",
            "Compteur exemple inchangé ; aucune écriture.",
            "Sur l’appareil, le dernier Fire enregistre via le firmware natif uniquement si le compteur a changé, puis revient au menu."}},
        {Language::Belarusian, {"Рэдактар лічыльніка зацяжак",
            "Бягучы лік адкрываецца ў пяці разрадах з пачатковымі нулямі. + / − мяняе вылучаную лічбу без пераносу; Fire пераходзіць далей і пацвярджае пятую.",
            "Выбраная рэвізія: 00000–%05u. Лічбы змяняюцца цыклічна ў межах рэдактара.",
            "Лік у прыкладзе зменены; на прыладу нічога не запісана.",
            "Лік у прыкладзе не зменены; запісу не было.",
            "На прыладзе апошні Fire захоўвае лічыльнік штатнай прашыўкай толькі пасля змены і вяртае ў меню."}},
        {Language::Uzbek, {"Tortish hisoblagichi tahriri",
            "Joriy son old nollar bilan besh xonada ochiladi. + / − belgilangan raqamni ko‘chirishsiz o‘zgartiradi; Fire keyingisiga o‘tadi va beshinchisini tasdiqlaydi.",
            "Tanlangan versiya: 00000–%05u. Raqamlar muharrir chegarasi ichida aylanadi.",
            "Namuna soni o‘zgardi; qurilmaga hech narsa yozilmadi.",
            "Namuna soni o‘zgarmadi; hech narsa yozilmadi.",
            "Qurilmada oxirgi Fire faqat o‘zgargan hisoblagichni asl dastur orqali saqlab, menyuga qaytaradi."}},
        {Language::Azerbaijani, {"Çəkmə sayğacı redaktoru",
            "Cari say başlanğıc sıfırlarla beş xanada açılır. + / − seçilmiş rəqəmi daşımadan dəyişir; Fire növbəti xanaya keçir və beşincini təsdiqləyir.",
            "Seçilmiş versiya: 00000–%05u. Rəqəmlər redaktor həddi daxilində dövr edir.",
            "Nümunə sayı dəyişdi; cihaza heç nə yazılmadı.",
            "Nümunə sayı dəyişmədi; heç nə yazılmadı.",
            "Cihazda son Fire yalnız dəyişmiş sayğacı yerli proqramla saxlayır və menyuya qayıdır."}},
        {Language::Kyrgyz, {"Тартуу эсептегичин түзөтүү",
            "Учурдагы сан баштапкы нөлдөр менен беш орунда ачылат. + / − белгиленген цифраны ташуусуз өзгөртөт; Fire кийинкиге өтүп, бешинчини ырастайт.",
            "Тандалган нуска: 00000–%05u. Цифралар редактордун чегинде гана айланат.",
            "Үлгү саны өзгөрдү; түзмөккө эч нерсе жазылган жок.",
            "Үлгү саны өзгөргөн жок; эч нерсе жазылган жок.",
            "Түзмөктө акыркы Fire сан өзгөрсө гана түпкү камтылган программа аркылуу сактап, менюга кайтарат."}},
        {Language::Tajik, {"Таҳрири ҳисобкунаки кашиш",
            "Шумораи ҷорӣ дар панҷ хона бо сифрҳои аввал кушода мешавад. + / − рақами интихобшударо бе гузариш тағйир медиҳад; Fire ба навбатӣ мегузарад ва панҷумро тасдиқ мекунад.",
            "Нусхаи интихобшуда: 00000–%05u. Рақамҳо дар ҳудуди муҳаррир давр мезананд.",
            "Шумораи намуна тағйир ёфт; ба дастгоҳ чизе навишта нашуд.",
            "Шумораи намуна тағйир наёфт; чизе навишта нашуд.",
            "Дар дастгоҳ Fire-и охирин танҳо шумораи тағйирёфтаро бо нармафзори аслӣ нигоҳ дошта, ба меню бармегардад."}},
        {Language::Armenian, {"Քաշումների հաշվիչի խմբագրում",
            "Ընթացիկ թիվը բացվում է հինգ դիրքով՝ սկզբի զրոներով։ + / − փոխում է նշված թվանշանը առանց փոխանցման, Fire-ն անցնում է հաջորդին և հաստատում հինգերորդը։",
            "Ընտրված տարբերակ՝ 00000–%05u։ Թվանշանները պտտվում են խմբագրիչի սահմանի ներսում։",
            "Օրինակի թիվը փոխվել է․ սարքին ոչինչ չի գրվել։",
            "Օրինակի թիվը չի փոխվել․ ոչինչ չի գրվել։",
            "Սարքում վերջին Fire-ը բնօրինակ ծրագրով պահում է միայն փոխված հաշվիչը և վերադառնում ընտրացանկ։"}},
        {Language::Georgian, {"ჩასუნთქვების მრიცხველის რედაქტორი",
            "მიმდინარე რიცხვი იხსნება ხუთ პოზიციაზე საწყისი ნულებით. + / − ცვლის მონიშნულ ციფრს გადატანის გარეშე; Fire გადადის შემდეგზე და ადასტურებს მეხუთეს.",
            "არჩეული ვერსია: 00000–%05u. ციფრები ბრუნავს რედაქტორის ზღვარში.",
            "ნიმუშის რიცხვი შეიცვალა; მოწყობილობაში არაფერი ჩაწერილა.",
            "ნიმუშის რიცხვი უცვლელია; არაფერი ჩაწერილა.",
            "მოწყობილობაზე ბოლო Fire მხოლოდ შეცვლილ მრიცხველს ინახავს მშობლიური პროგრამით და მენიუში ბრუნდება."}},
        {Language::Romanian, {"Editorul contorului de pufuri",
            "Deschide numărul curent în cinci poziții cu zerouri inițiale. + / − schimbă cifra evidențiată fără transport; Fire avansează și confirmă a cincea.",
            "Revizia selectată: 00000–%05u. Cifrele se rotesc în limita editorului.",
            "Numărul exemplu a fost schimbat; nimic nu a fost scris pe dispozitiv.",
            "Numărul exemplu este neschimbat; nimic nu a fost scris.",
            "Pe dispozitiv, ultimul Fire salvează prin firmware-ul nativ numai dacă numărul s-a schimbat, apoi revine la meniu."}},
    };
    for (const auto& translation : puffTranslations)
        for (size_t index = 0; index < sizeof(puffKeys) / sizeof(puffKeys[0]); ++index)
            g_table[(int)translation.language][(int)puffKeys[index]] = translation.text[index];
    constexpr Str achievementKeys[]{Str::PatchMenuScoreEditor, Str::PatchMenuRankSelector,
        Str::PatchMenuScoreHelp, Str::PatchMenuRankHelp, Str::PatchMenuAchievementsUnsupported};
    struct AchievementTranslation { Language language; const char* text[5]; };
    static constexpr AchievementTranslation achievementTranslations[]{
        {Language::English, {"Achievement points editor", "Set achievement rank",
            "Edits 00000–99999 points. + / − changes a digit; Fire advances and confirms the fifth. Fractional points and the best single-puff score are preserved.",
            "Selecting a rank changes total points to its minimum, saves, and returns to the main screen. It can lower existing points.",
            "Achievements are available only in DRAG S Score firmware."}},
        {Language::Russian, {"Редактор очков достижений", "Выбор ранга достижения",
            "Редактор очков: 00000–99999. + / − меняет цифру; Fire переходит дальше и подтверждает пятую. Дробный остаток и рекорд одной тяги сохраняются.",
            "Выбор ранга устанавливает его минимальные очки, сохраняет их и возвращает главный экран. Текущее число очков может уменьшиться.",
            "Достижения доступны только в прошивке DRAG S Score."}},
        {Language::Ukrainian, {"Редактор очок досягнень", "Вибір рангу досягнення",
            "Редактор очок: 00000–99999. + / − змінює цифру; Fire переходить далі й підтверджує п’яту. Дробовий залишок і рекорд однієї затяжки зберігаються.",
            "Вибір рангу встановлює його мінімальні очки, зберігає їх і повертає головний екран. Поточні очки можуть зменшитися.",
            "Досягнення доступні лише у прошивці DRAG S Score."}},
        {Language::Polish, {"Edytor punktów osiągnięć", "Wybór rangi osiągnięcia",
            "Edytuje 00000–99999 punktów. + / − zmienia cyfrę; Fire przechodzi dalej i zatwierdza piątą. Część ułamkowa i rekord pojedynczego zaciągnięcia zostają zachowane.",
            "Wybór rangi ustawia jej minimalne punkty, zapisuje je i wraca do ekranu głównego. Może zmniejszyć bieżący wynik.",
            "Osiągnięcia są dostępne tylko w firmware DRAG S Score."}},
        {Language::Kazakh, {"Жетістік ұпайларын өңдеу", "Жетістік дәрежесін таңдау",
            "00000–99999 ұпай өңделеді. + / − цифрды өзгертеді; Fire келесіге өтіп, бесіншіні растайды. Бөлшек қалдық пен бір тарту рекорды сақталады.",
            "Дәрежені таңдау оның ең аз ұпайын орнатып, сақтайды және негізгі экранға қайтарады. Ағымдағы ұпай азаюы мүмкін.",
            "Жетістіктер тек DRAG S Score микробағдарламасында бар."}},
        {Language::German, {"Erfolgspunkte bearbeiten", "Erfolgsrang wählen",
            "Bearbeitet 00000–99999 Punkte. + / − ändert eine Ziffer; Fire geht weiter und bestätigt die fünfte. Bruchteile und der beste Einzelzug bleiben erhalten.",
            "Die Rangwahl setzt die Gesamtpunkte auf dessen Minimum, speichert und öffnet den Hauptbildschirm. Vorhandene Punkte können sinken.",
            "Erfolge sind nur in der DRAG-S-Score-Firmware verfügbar."}},
        {Language::Spanish, {"Editor de puntos de logros", "Elegir rango de logro",
            "Edita 00000–99999 puntos. + / − cambia un dígito; Fire avanza y confirma el quinto. Se conservan la fracción y el récord de una sola calada.",
            "Elegir un rango fija sus puntos mínimos, guarda y vuelve a la pantalla principal. Puede reducir los puntos actuales.",
            "Los logros solo están disponibles en el firmware DRAG S Score."}},
        {Language::French, {"Éditeur de points de succès", "Choisir le rang",
            "Modifie 00000–99999 points. + / − change un chiffre ; Fire avance puis confirme le cinquième. La fraction et le record d’une seule bouffée sont conservés.",
            "Choisir un rang fixe son minimum de points, enregistre et revient à l’écran principal. Le total actuel peut diminuer.",
            "Les succès sont disponibles uniquement dans le firmware DRAG S Score."}},
        {Language::Belarusian, {"Рэдактар ачкоў дасягненняў", "Выбар рангу дасягнення",
            "Рэдактар ачкоў: 00000–99999. + / − мяняе лічбу; Fire пераходзіць далей і пацвярджае пятую. Дробавы астатак і рэкорд адной зацяжкі захоўваюцца.",
            "Выбар рангу задае яго мінімальныя ачкі, захоўвае іх і вяртае галоўны экран. Бягучыя ачкі могуць зменшыцца.",
            "Дасягненні даступныя толькі ў прашыўцы DRAG S Score."}},
        {Language::Uzbek, {"Yutuq ballarini tahrirlash", "Yutuq darajasini tanlash",
            "00000–99999 ball tahrirlanadi. + / − raqamni o‘zgartiradi; Fire keyingisiga o‘tib, beshinchisini tasdiqlaydi. Kasr qoldig‘i va bir tortish rekordi saqlanadi.",
            "Darajani tanlash uning eng kam ballini o‘rnatadi, saqlaydi va asosiy ekranga qaytaradi. Joriy ball kamayishi mumkin.",
            "Yutuqlar faqat DRAG S Score dasturida mavjud."}},
        {Language::Azerbaijani, {"Nailiyyət xallarını redaktə et", "Nailiyyət dərəcəsini seç",
            "00000–99999 xal redaktə olunur. + / − rəqəmi dəyişir; Fire növbətiyə keçir və beşincini təsdiqləyir. Kəsr qalığı və bir çəkmə rekordu saxlanır.",
            "Dərəcə seçimi onun minimum xalını təyin edir, saxlayır və əsas ekrana qaytarır. Cari xal azala bilər.",
            "Nailiyyətlər yalnız DRAG S Score proqramında mövcuddur."}},
        {Language::Kyrgyz, {"Жетишкендик упайларын түзөтүү", "Жетишкендик даражасын тандоо",
            "00000–99999 упай түзөтүлөт. + / − цифраны өзгөртөт; Fire кийинкиге өтүп, бешинчини ырастайт. Бөлчөк калдык жана бир тартуунун рекорду сакталат.",
            "Даражаны тандоо анын эң аз упайын коюп, сактайт жана башкы экранга кайтарат. Учурдагы упай азайышы мүмкүн.",
            "Жетишкендиктер DRAG S Score камтылган программасында гана бар."}},
        {Language::Tajik, {"Таҳрири холҳои дастовард", "Интихоби дараҷаи дастовард",
            "00000–99999 хол таҳрир мешавад. + / − рақамро иваз мекунад; Fire ба навбатӣ гузашта, панҷумро тасдиқ мекунад. Бақияи касрӣ ва рекорди як кашиш нигоҳ дошта мешаванд.",
            "Интихоби дараҷа ҳадди ақали холҳои онро муқаррар карда, нигоҳ медорад ва ба экрани асосӣ бармегардад. Холҳои ҷорӣ метавонанд кам шаванд.",
            "Дастовардҳо танҳо дар нармафзори DRAG S Score дастрасанд."}},
        {Language::Armenian, {"Նվաճման միավորների խմբագրում", "Նվաճման աստիճանի ընտրություն",
            "Խմբագրում է 00000–99999 միավոր։ + / − փոխում է թվանշանը, Fire-ն անցնում է հաջորդին և հաստատում հինգերորդը։ Կոտորակային մնացորդն ու մեկ քաշման ռեկորդը պահպանվում են։",
            "Աստիճանի ընտրությունը սահմանում և պահում է դրա նվազագույն միավորները, ապա վերադառնում գլխավոր էկրան։ Ընթացիկ միավորները կարող են նվազել։",
            "Նվաճումները հասանելի են միայն DRAG S Score ծրագրում։"}},
        {Language::Georgian, {"მიღწევის ქულების რედაქტორი", "მიღწევის რანგის არჩევა",
            "ცვლის 00000–99999 ქულას. + / − ცვლის ციფრს; Fire გადადის შემდეგზე და ადასტურებს მეხუთეს. წილადი ნაშთი და ერთი ჩასუნთქვის რეკორდი ინახება.",
            "რანგის არჩევა ადგენს და ინახავს მის მინიმალურ ქულებს, შემდეგ აბრუნებს მთავარ ეკრანს. მიმდინარე ქულები შეიძლება შემცირდეს.",
            "მიღწევები ხელმისაწვდომია მხოლოდ DRAG S Score პროგრამაში."}},
        {Language::Romanian, {"Editorul punctelor de realizări", "Alegerea rangului",
            "Editează 00000–99999 puncte. + / − schimbă o cifră; Fire avansează și confirmă a cincea. Fracția și recordul unui singur puf se păstrează.",
            "Alegerea rangului setează punctajul său minim, salvează și revine la ecranul principal. Poate reduce punctajul curent.",
            "Realizările sunt disponibile doar în firmware-ul DRAG S Score."}},
    };
    for (const auto& translation : achievementTranslations)
        for (size_t index = 0; index < sizeof(achievementKeys) / sizeof(achievementKeys[0]); ++index)
            g_table[(int)translation.language][(int)achievementKeys[index]] = translation.text[index];
    struct HoldTranslation { Language language; const char* text; };
    static constexpr HoldTranslation holdTranslations[]{
        {Language::English, "Short Fire advances or confirms. Hold Fire for 0.7 s to cancel without saving; hold + / − to change faster."},
        {Language::Russian, "Короткий Fire — дальше или подтвердить. Fire на 0,7 с — отменить без сохранения; удержание + / − ускоряет изменение."},
        {Language::Ukrainian, "Короткий Fire — далі або підтвердити. Fire на 0,7 с — скасувати без збереження; утримання + / − прискорює зміну."},
        {Language::Polish, "Krótki Fire przechodzi dalej lub zatwierdza. Przytrzymaj Fire przez 0,7 s, aby anulować bez zapisu; przytrzymaj + / −, aby zmieniać szybciej."},
        {Language::Kazakh, "Қысқа Fire келесіге өтеді не растайды. Fire-ды 0,7 с ұстау сақтамай бас тартады; + / − ұстау өзгертуді жылдамдатады."},
        {Language::German, "Kurzes Fire geht weiter oder bestätigt. Fire 0,7 s halten: ohne Speichern abbrechen. + / − halten: schneller ändern."},
        {Language::Spanish, "Fire breve avanza o confirma. Mantén Fire 0,7 s para cancelar sin guardar; mantén + / − para cambiar más rápido."},
        {Language::French, "Un appui bref sur Fire avance ou confirme. Maintenir Fire 0,7 s annule sans enregistrer ; maintenir + / − accélère le changement."},
        {Language::Belarusian, "Кароткі Fire — далей або пацвердзіць. Fire на 0,7 с — скасаваць без захавання; утрыманне + / − паскарае змену."},
        {Language::Uzbek, "Qisqa Fire keyingisiga o‘tadi yoki tasdiqlaydi. Fire’ni 0,7 s ushlash saqlamasdan bekor qiladi; + / − ni ushlash o‘zgartirishni tezlashtiradi."},
        {Language::Azerbaijani, "Qısa Fire növbətiyə keçir və ya təsdiqləyir. Fire-ı 0,7 s saxlayın: saxlamadan ləğv edir; + / − saxlamaq dəyişikliyi sürətləndirir."},
        {Language::Kyrgyz, "Кыска Fire кийинкиге өтөт же ырастайт. Fire-ды 0,7 с кармоо сактабастан жокко чыгарат; + / − кармоо өзгөртүүнү тездетет."},
        {Language::Tajik, "Fire-и кӯтоҳ ба навбатӣ мегузарад ё тасдиқ мекунад. Fire-ро 0,7 с нигоҳ доред, то бе нигоҳдорӣ бекор кунед; нигоҳ доштани + / − тағйирро тез мекунад."},
        {Language::Armenian, "Կարճ Fire-ը անցնում է հաջորդին կամ հաստատում։ Fire-ը պահեք 0,7 վ՝ առանց պահելու չեղարկելու համար, իսկ + / − պահելը արագացնում է փոփոխությունը։"},
        {Language::Georgian, "Fire-ის მოკლე დაჭერა გადადის შემდეგზე ან ადასტურებს. Fire-ის 0,7 წმ დაჭერა აუქმებს შენახვის გარეშე; + / − დაჭერა აჩქარებს ცვლილებას."},
        {Language::Romanian, "Fire scurt avansează sau confirmă. Ține Fire 0,7 s pentru anulare fără salvare; ține + / − pentru schimbare mai rapidă."},
    };
    for (const auto& translation : holdTranslations)
        g_table[(int)translation.language][(int)Str::PatchMenuHoldControls] = translation.text;
}

static void InitExportReviewTranslations() {
    constexpr Str keys[]{Str::ExportReviewTitle, Str::ExportReviewIntro, Str::ExportReviewChanges,
        Str::ExportReviewNoChanges, Str::ExportReviewCapacity, Str::ExportReviewIssues,
        Str::ExportReviewConfirm, Str::ExportReviewStale, Str::ExportReviewUnavailable, Str::ExportReviewReady};
    struct Translation { Language language; const char* text[10]; };
    static constexpr Translation translations[]{
        {Language::English, {"Review firmware export",
            "Review all revisions before writing. No files are created until confirmation.",
            "Enabled and applied changes", "No custom changes", "Capacity", "Validation issues", "Confirm export",
            "Document changed. Cancel and review again.", "Unavailable", "All hardware revisions passed validation"}},
        {Language::Russian, {"Проверка экспорта прошивки",
            "Проверьте все ревизии перед записью. До подтверждения файлы не создаются.",
            "Включённые и применённые изменения", "Нет изменений", "Ёмкость", "Ошибки проверки", "Подтвердить экспорт",
            "Документ изменён. Отмените и повторите проверку.", "Недоступно", "Все аппаратные ревизии прошли проверку"}},
        {Language::Ukrainian, {"Перевірка експорту прошивки",
            "Перевірте всі ревізії перед записом. До підтвердження файли не створюються.",
            "Увімкнені та застосовані зміни", "Немає змін", "Місткість", "Помилки перевірки", "Підтвердити експорт",
            "Документ змінено. Скасуйте та повторіть перевірку.", "Недоступно", "Усі апаратні ревізії пройшли перевірку"}},
        {Language::Polish, {"Sprawdzenie eksportu firmware",
            "Sprawdź wszystkie rewizje przed zapisem. Pliki powstaną dopiero po potwierdzeniu.",
            "Włączone i zastosowane zmiany", "Brak zmian", "Pojemność", "Błędy sprawdzania", "Potwierdź eksport",
            "Dokument został zmieniony. Anuluj i sprawdź ponownie.", "Niedostępne", "Wszystkie rewizje sprzętu przeszły sprawdzenie"}},
        {Language::Kazakh, {"Микробағдарлама экспортын тексеру",
            "Жазу алдында барлық нұсқаны тексеріңіз. Расталғанша файлдар жасалмайды.",
            "Қосылған және қолданылған өзгерістер", "Өзгерістер жоқ", "Сыйымдылық", "Тексеру қателері", "Экспортты растау",
            "Құжат өзгерді. Бас тартып, қайта тексеріңіз.", "Қолжетімсіз", "Барлық аппараттық нұсқа тексеруден өтті"}},
        {Language::German, {"Firmware-Export prüfen",
            "Alle Revisionen vor dem Schreiben prüfen. Dateien werden erst nach der Bestätigung erstellt.",
            "Aktivierte und angewandte Änderungen", "Keine Änderungen", "Kapazität", "Prüffehler", "Export bestätigen",
            "Dokument geändert. Abbrechen und erneut prüfen.", "Nicht verfügbar", "Alle Hardware-Revisionen haben die Prüfung bestanden"}},
        {Language::Spanish, {"Revisar exportación del firmware",
            "Revisa todas las revisiones antes de escribir. No se crean archivos hasta confirmar.",
            "Cambios activados y aplicados", "Sin cambios", "Capacidad", "Errores de validación", "Confirmar exportación",
            "El documento cambió. Cancela y revisa de nuevo.", "No disponible", "Todas las revisiones de hardware superaron la validación"}},
        {Language::French, {"Vérifier l’export du firmware",
            "Vérifiez toutes les révisions avant l’écriture. Aucun fichier n’est créé avant confirmation.",
            "Modifications activées et appliquées", "Aucune modification", "Capacité", "Erreurs de vérification", "Confirmer l’export",
            "Le document a changé. Annulez et vérifiez à nouveau.", "Indisponible", "Toutes les révisions matérielles ont passé la vérification"}},
        {Language::Belarusian, {"Праверка экспарту прашыўкі",
            "Праверце ўсе рэвізіі перад запісам. Да пацвярджэння файлы не ствараюцца.",
            "Уключаныя і ўжытыя змены", "Няма змен", "Ёмістасць", "Памылкі праверкі", "Пацвердзіць экспарт",
            "Дакумент зменены. Скасуйце і паўтарыце праверку.", "Недаступна", "Усе апаратныя рэвізіі прайшлі праверку"}},
        {Language::Uzbek, {"Dastur eksportini tekshirish",
            "Yozishdan oldin barcha versiyalarni tekshiring. Tasdiqlanmaguncha fayllar yaratilmaydi.",
            "Yoqilgan va qo‘llangan o‘zgarishlar", "O‘zgarishlar yo‘q", "Sig‘im", "Tekshirish xatolari", "Eksportni tasdiqlash",
            "Hujjat o‘zgardi. Bekor qilib, qayta tekshiring.", "Mavjud emas", "Barcha apparat versiyalari tekshiruvdan o‘tdi"}},
        {Language::Azerbaijani, {"Proqram ixracını yoxla",
            "Yazmadan əvvəl bütün versiyaları yoxlayın. Təsdiqlənənədək fayllar yaradılmır.",
            "Aktiv və tətbiq edilmiş dəyişikliklər", "Dəyişiklik yoxdur", "Tutum", "Yoxlama xətaları", "İxracı təsdiqlə",
            "Sənəd dəyişib. Ləğv edib yenidən yoxlayın.", "Mövcud deyil", "Bütün aparat versiyaları yoxlamadan keçdi"}},
        {Language::Kyrgyz, {"Камтылган программанын экспортун текшерүү",
            "Жазуудан мурун бардык нускаларды текшериңиз. Ырасталмайынча файлдар түзүлбөйт.",
            "Кошулган жана колдонулган өзгөрүүлөр", "Өзгөрүүлөр жок", "Сыйымдуулук", "Текшерүү каталары", "Экспортту ырастоо",
            "Документ өзгөрдү. Жокко чыгарып, кайра текшериңиз.", "Жеткиликсиз", "Бардык аппараттык нускалар текшерүүдөн өттү"}},
        {Language::Tajik, {"Санҷиши содироти нармафзор",
            "Пеш аз навиштан ҳамаи нусхаҳоро санҷед. То тасдиқ файлҳо сохта намешаванд.",
            "Тағйироти фаъол ва татбиқшуда", "Тағйирот нест", "Ғунҷоиш", "Хатоҳои санҷиш", "Тасдиқи содирот",
            "Ҳуҷҷат тағйир ёфт. Бекор карда, боз санҷед.", "Дастнорас", "Ҳамаи нусхаҳои сахтафзор аз санҷиш гузаштанд"}},
        {Language::Armenian, {"Ծրագրի արտահանման ստուգում",
            "Գրելուց առաջ ստուգեք բոլոր տարբերակները։ Ֆայլերը չեն ստեղծվում մինչև հաստատումը։",
            "Միացված և կիրառված փոփոխություններ", "Փոփոխություններ չկան", "Տարողություն", "Ստուգման սխալներ", "Հաստատել արտահանումը",
            "Փաստաթուղթը փոխվել է։ Չեղարկեք և նորից ստուգեք։", "Հասանելի չէ", "Բոլոր սարքային տարբերակներն անցել են ստուգումը"}},
        {Language::Georgian, {"პროგრამის ექსპორტის შემოწმება",
            "ჩაწერამდე შეამოწმეთ ყველა რევიზია. ფაილები არ იქმნება დადასტურებამდე.",
            "ჩართული და გამოყენებული ცვლილებები", "ცვლილებები არ არის", "ტევადობა", "შემოწმების შეცდომები", "ექსპორტის დადასტურება",
            "დოკუმენტი შეიცვალა. გააუქმეთ და ხელახლა შეამოწმეთ.", "მიუწვდომელია", "ყველა აპარატურულმა რევიზიამ გაიარა შემოწმება"}},
        {Language::Romanian, {"Verificarea exportului firmware",
            "Verifică toate reviziile înainte de scriere. Fișierele sunt create doar după confirmare.",
            "Modificări activate și aplicate", "Fără modificări", "Capacitate", "Erori de verificare", "Confirmă exportul",
            "Documentul s-a schimbat. Anulează și verifică din nou.", "Indisponibil", "Toate reviziile hardware au trecut verificarea"}},
    };
    for (const auto& translation : translations)
        for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); ++index)
            g_table[(int)translation.language][(int)keys[index]] = translation.text[index];
    struct CapacityNoteTranslation { Language language; const char* text; };
    static constexpr CapacityNoteTranslation notes[]{
        {Language::English, "Capacity is shown only where the firmware has a verified safe Flash boundary. Unavailable does not mean zero free space; the normal firmware guards still apply."},
        {Language::Russian, "Ёмкость показана только для прошивок с проверенной безопасной границей Flash. «Недоступно» не означает нулевой остаток; обычные проверки прошивки сохраняются."},
        {Language::Ukrainian, "Місткість показана лише для прошивок із перевіреною безпечною межею Flash. «Недоступно» не означає нульовий залишок; звичайні перевірки прошивки зберігаються."},
        {Language::Polish, "Pojemność jest podawana tylko dla firmware ze sprawdzoną bezpieczną granicą Flash. Niedostępna wartość nie oznacza braku wolnego miejsca; standardowe zabezpieczenia nadal działają."},
        {Language::Kazakh, "Сыйымдылық қауіпсіз Flash шекарасы тексерілген микробағдарлама үшін ғана көрсетіледі. Қолжетімсіз мән бос орын жоқ дегенді білдірмейді; қалыпты тексерулер сақталады."},
        {Language::German, "Die Kapazität wird nur bei geprüfter sicherer Flash-Grenze angezeigt. Nicht verfügbar bedeutet nicht, dass kein Platz frei ist; die normalen Firmware-Prüfungen gelten weiterhin."},
        {Language::Spanish, "La capacidad solo se muestra si el firmware tiene un límite Flash seguro verificado. No disponible no significa que no quede espacio; siguen aplicándose las comprobaciones normales."},
        {Language::French, "La capacité est affichée uniquement si la limite sûre de la Flash est vérifiée. Indisponible ne signifie pas qu’il ne reste aucune place ; les vérifications habituelles restent actives."},
        {Language::Belarusian, "Ёмістасць паказваецца толькі для прашывак з праверанай бяспечнай мяжой Flash. «Недаступна» не азначае нулявы астатак; звычайныя праверкі прашыўкі захоўваюцца."},
        {Language::Uzbek, "Sig‘im faqat xavfsiz Flash chegarasi tekshirilgan dastur uchun ko‘rsatiladi. Mavjud emas degani bo‘sh joy yo‘q degani emas; odatiy tekshiruvlar saqlanadi."},
        {Language::Azerbaijani, "Tutum yalnız təhlükəsiz Flash sərhədi yoxlanmış proqram üçün göstərilir. Məlumatın olmaması boş yerin sıfır olması deyil; adi yoxlamalar qüvvədə qalır."},
        {Language::Kyrgyz, "Сыйымдуулук коопсуз Flash чеги текшерилген программа үчүн гана көрсөтүлөт. Жеткиликсиз маани бош орун жок дегенди билдирбейт; кадимки текшерүүлөр сакталат."},
        {Language::Tajik, "Ғунҷоиш танҳо барои нармафзори дорои ҳудуди бехатари санҷидашудаи Flash нишон дода мешавад. Дастнорас маънои набудани ҷойи холиро надорад; санҷишҳои муқаррарӣ амал мекунанд."},
        {Language::Armenian, "Տարողությունը ցուցադրվում է միայն ստուգված անվտանգ Flash սահմանով ծրագրերի համար։ Անհասանելի արժեքը չի նշանակում ազատ տեղի բացակայություն․ սովորական ստուգումները շարունակում են գործել։"},
        {Language::Georgian, "ტევადობა ჩანს მხოლოდ შემოწმებული უსაფრთხო Flash საზღვრის მქონე პროგრამებისთვის. მიუწვდომელი მნიშვნელობა არ ნიშნავს თავისუფალი ადგილის არქონას; ჩვეულებრივი შემოწმებები ძალაში რჩება."},
        {Language::Romanian, "Capacitatea este afișată doar când limita sigură a memoriei Flash este verificată. Indisponibil nu înseamnă lipsă de spațiu liber; verificările obișnuite rămân active."},
    };
    for (const auto& note : notes)
        g_table[(int)note.language][(int)Str::ExportReviewCapacityNote] = note.text;
}


void InitI18n() {
    InitEnglish();
    InitRussian();
    InitUkrainian();
    InitPolish();
    InitKazakh();
    InitGerman();
    InitSpanish();
    InitFrench();
    InitBelarusian();
    InitUzbek();
    InitAzerbaijani();
    InitKyrgyz();
    InitTajik();
    InitArmenian();
    InitGeorgian();
    InitRomanian();
    InitSettingsTranslations();
    InitPatchPasteTranslations();
    InitEffectDirectionTranslations();
    InitDeviceShortcutTranslations();
    InitDeviceMenuTranslations();
    InitMenuWorkspaceTranslations();
    InitExportReviewTranslations();

    std::string configPath = GetConfigPath();
    char langCode[32] = {0};
    GetPrivateProfileStringA("Settings", "Language", "", langCode, sizeof(langCode), configPath.c_str());

    if (langCode[0] != '\0') {
        SetLanguageByCode(langCode);
    } else {
        SetLanguage(DetectSystemLanguage());
    }
}

const std::vector<LanguageInfo>& GetSupportedLanguages() {
    return g_supportedLanguages;
}

Language GetCurrentLanguage() {
    return g_currentLanguage;
}

const LanguageInfo& GetCurrentLanguageInfo() {
    for (const auto& info : g_supportedLanguages) {
        if (info.lang == g_currentLanguage) return info;
    }
    return g_supportedLanguages[0];
}

void SetLanguage(Language lang) {
    if (lang >= Language::English && lang < Language::Count) {
        g_currentLanguage = lang;
    }
}

bool SetLanguageByCode(const std::string& code) {
    for (const auto& info : g_supportedLanguages) {
        if (code == info.code) {
            g_currentLanguage = info.lang;
            return true;
        }
    }
    return false;
}

Language DetectSystemLanguage() {
    LANGID langID = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langID);
    switch (primaryLang) {
        case LANG_RUSSIAN: return Language::Russian;
        case LANG_UKRAINIAN: return Language::Ukrainian;
        case LANG_POLISH: return Language::Polish;
        case LANG_KAZAK: return Language::Kazakh;
        case LANG_GERMAN: return Language::German;
        case LANG_SPANISH: return Language::Spanish;
        case LANG_FRENCH: return Language::French;
        case LANG_BELARUSIAN: return Language::Belarusian;
        case LANG_UZBEK: return Language::Uzbek;
        case LANG_AZERI: return Language::Azerbaijani;
        case LANG_KYRGYZ: return Language::Kyrgyz;
        case LANG_TAJIK: return Language::Tajik;
        case LANG_ARMENIAN: return Language::Armenian;
        case LANG_GEORGIAN: return Language::Georgian;
        case LANG_ROMANIAN: return Language::Romanian;
        default: return Language::English;
    }
}

bool SaveSettings(const std::string& path) {
    const auto configPath = path.empty() ? GetConfigPath() : path;
    return WritePrivateProfileStringA("Settings", "Language", GetCurrentLanguageInfo().code, configPath.c_str()) != FALSE;
}

const char* Get(Str id) {
    int langIdx = (int)g_currentLanguage;
    int strIdx = (int)id;
    if (langIdx < 0 || langIdx >= (int)Language::Count) return "";
    if (strIdx < 0 || strIdx >= (int)Str::Count) return "";
    const char* str = g_table[langIdx][strIdx];
    if (!str || str[0] == '\0') {
        str = g_table[(int)Language::English][strIdx];
    }
    if (!str) return "";
    return str;
}

} // namespace i18n

#pragma once

#include <QtQml/qqmlregistration.h>

#include "components/settingseditor/editorsession.h"
#include "components/settingseditor/scripteditormodel.h"
#include "components/settingseditor/settingseditormodel.h"
#include "components/settingseditor/settingsscales.h"
#include "components/settingseditor/settingsvalues.h"
#include "components/settingseditor/shortcuteditormodel.h"

// QML registrations of the settings editor types, which live in the
// UI-independent view components (both UIs use them).

struct EditorSessionForeign {
    Q_GADGET
    QML_FOREIGN(EditorSession)
    QML_ANONYMOUS
};

struct SettingsEditorModelForeign {
    Q_GADGET
    QML_FOREIGN(SettingsEditorModel)
    QML_NAMED_ELEMENT(SettingsEditorModel)
    QML_UNCREATABLE("Provided by SettingsDialogController")
};

struct ShortcutEditorModelForeign {
    Q_GADGET
    QML_FOREIGN(ShortcutEditorModel)
    QML_NAMED_ELEMENT(ShortcutEditorModel)
    QML_UNCREATABLE("Provided by SettingsEditorModel")
};

struct ScriptEditorModelForeign {
    Q_GADGET
    QML_FOREIGN(ScriptEditorModel)
    QML_NAMED_ELEMENT(ScriptEditorModel)
    QML_UNCREATABLE("Provided by SettingsEditorModel")
};

struct GeneralSettingsForeign {
    Q_GADGET
    QML_FOREIGN(GeneralSettings)
    QML_VALUE_TYPE(generalSettings)
};

struct ViewSettingsForeign {
    Q_GADGET
    QML_FOREIGN(ViewSettings)
    QML_VALUE_TYPE(viewSettings)
};

struct ThemeSettingsForeign {
    Q_GADGET
    QML_FOREIGN(ThemeSettings)
    QML_VALUE_TYPE(themeSettings)
};

struct ControlsSettingsForeign {
    Q_GADGET
    QML_FOREIGN(ControlsSettings)
    QML_VALUE_TYPE(controlsSettings)
};

struct AdvancedSettingsForeign {
    Q_GADGET
    QML_FOREIGN(AdvancedSettings)
    QML_VALUE_TYPE(advancedSettings)
};

struct UpscaleSettingsForeign {
    Q_GADGET
    QML_FOREIGN(UpscaleSettings)
    QML_VALUE_TYPE(upscaleSettings)
};

struct SettingsRangeForeign {
    Q_GADGET
    QML_FOREIGN(SettingsRange)
    QML_VALUE_TYPE(settingsRange)
};

struct SettingsRangesForeign {
    Q_GADGET
    QML_FOREIGN(SettingsRanges)
    QML_VALUE_TYPE(settingsRanges)
};

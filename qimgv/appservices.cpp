#include "appservices.h"

#include "components/actionmanager/actionmanager.h"
#include "components/scriptmanager/scriptmanager.h"
#include "settings.h"
#include "sharedresources.h"
#include "utils/actions.h"
#include "utils/iconfontmanager.h"
#include "utils/inputmap.h"

//------------------------------------------------------------------------------
AppServices::AppServices() {
    // Must run before any IconWidget/StyledComboBox is constructed, since
    // both resolve glyphs through IconFontManager::pixmap() during their
    // first paint. Safe to call unconditionally: init() is idempotent and
    // a failed font load just means glyph rendering will log a warning and
    // return null pixmaps instead of crashing.
    IconFontManager::init();
    // getInstance() publishes each instance through its global before
    // returning, so later services can already use the earlier ones while
    // they initialize (ScriptManager reads its scripts from settings).
    mInputMap.reset(InputMap::getInstance());
    mActions.reset(Actions::getInstance());
    mSettings.reset(Settings::getInstance());
    mScriptManager.reset(ScriptManager::getInstance());
    mActionManager.reset(ActionManager::getInstance());
    mSharedResources.reset(SharedResources::getInstance());
}
//------------------------------------------------------------------------------
AppServices::~AppServices() {
    // ScriptManager saves its scripts through settings in its destructor, so
    // it goes before Settings. Each global is cleared right after its
    // instance is gone so that no stale pointer stays reachable during the
    // remaining teardown.
    mActionManager.reset();
    actionManager = nullptr;
    mScriptManager.reset();
    scriptManager = nullptr;
    mSettings.reset();
    settings = nullptr;
    mInputMap.reset();
    inputMap = nullptr;
    mActions.reset();
    appActions = nullptr;
    mSharedResources.reset();
    shrRes = nullptr;
}

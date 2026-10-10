## QML MODULE qimgv.style
# Qt Quick Controls style of the Qt Quick UI, selected at compile time: QML
# files import qimgv.style instead of QtQuick.Controls. Controls without an
# implementation here fall back to the Basic style (imported by the module's
# qmldir). The controls are built on QtQuick.Templates and read only the
# Theme singleton (qimgv.bridges) and StyleConstants, so a theme switch
# restyles every control through its bindings.

qt_add_library(qimgv_style STATIC)

set_source_files_properties(StyleConstants.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(qimgv_style
    URI qimgv.style
    IMPORTS QtQuick.Controls.Basic
    DEPENDENCIES QtQuick qimgv.bridges
    SOURCES
        shortcuttext.cpp
        shortcuttext.h
    QML_FILES
        Button.qml
        CheckBox.qml
        ComboBox.qml
        DialogButtonBox.qml
        DoubleSpinBox.qml
        IconGlyph.qml
        ItemDelegate.qml
        Label.qml
        Menu.qml
        MenuItem.qml
        MenuSeparator.qml
        Popup.qml
        PopupBackground.qml
        ProgressBar.qml
        RadioButton.qml
        ScrollBar.qml
        ScrollView.qml
        SearchField.qml
        Slider.qml
        SpinBox.qml
        SpinStepIndicator.qml
        StyleConstants.qml
        TextField.qml
        ToolButton.qml
        ToolTip.qml
)

target_compile_features(qimgv_style PUBLIC cxx_std_23)
if(MSVC)
    # Same C++26 working-draft switch as qimgv-plus.
    target_compile_options(qimgv_style PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>")
endif()

# qmlcachegen compiles the bindings on the Theme singleton to C++ that calls
# the bridge directly, so the module links the bridges library.
target_link_libraries(qimgv_style
    PUBLIC
        Qt6::Quick
        qimgv_bridges
    PRIVATE
        Qt6::QuickControls2
)

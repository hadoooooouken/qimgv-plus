import QtQuick
import QtQuick.Templates as T
import qimgv.bridges

// Check box with the Fluent glyph indicators ProxyStyle draws for QCheckBox.
T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: StyleConstants.indicatorPadding
    spacing: StyleConstants.indicatorSpacing

    indicator: IconGlyph {
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        icon: control.checkState === Qt.Checked ? FluentIcons.CheckboxChecked16
            : control.checkState === Qt.PartiallyChecked ? FluentIcons.CheckboxIndeterminate16
            : FluentIcons.CheckboxUnchecked16
        size: StyleConstants.indicatorSize
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font: control.font
        color: Theme.colors.textHc
        opacity: control.enabled ? 1.0 : StyleConstants.disabledOpacity
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}

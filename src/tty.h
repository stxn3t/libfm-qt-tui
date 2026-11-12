/*
 * tty.h - shared state for the tty (terminal-style) look of pcmanfm-tty.
 *
 * pcmanfm-tty sets these at startup from its tty.conf. Widgets in
 * libfm-qt read them to draw ls-style colours, the ">" cursor and the
 * floating labels that sit on a box's border line.
 *
 * When tty mode is off (the default), every helper here is a no-op, so
 * other programs linking libfm-qt are unaffected.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef FM_TTY_H
#define FM_TTY_H

#include "libfmqtglobals.h"
#include <QColor>
#include <QPointer>
#include <QString>
#include <QWidget>
#include <memory>

class QWidget;
class QDialog;

namespace Fm {

class FileInfo;

namespace Tty {

// The 12-colour palette, same names as heroic-tty / jellyfin-tty.
struct Colors {
    QColor bg{0x00, 0x00, 0x00};
    QColor bgAlt{0x11, 0x11, 0x13};
    QColor fg{0xd4, 0xd4, 0xd8};
    QColor fgDim{0x8a, 0x8a, 0x94};
    QColor border{0x3f, 0x3f, 0x46};
    QColor borderHover{0xa1, 0xa1, 0xaa};
    QColor accent{0xa6, 0xe3, 0xa1};
    QColor accentAlt{0xc6, 0xf6, 0xc1};
    QColor red{0xf3, 0x8b, 0xa8};
    QColor yellow{0xf9, 0xe2, 0xaf};
    QColor blue{0x89, 0xb4, 0xfa};
    QColor green{0xa6, 0xe3, 0xa1};
};

LIBFM_QT_API bool enabled();
LIBFM_QT_API void setEnabled(bool enabled);

LIBFM_QT_API const Colors& colors();
LIBFM_QT_API void setColors(const Colors& colors);

// Box line and label metrics, from tty.conf.
struct Metrics {
    int borderWidth = 1;
    Qt::PenStyle borderStyle = Qt::SolidLine;
    int radius = 3;
    int labelPixelSize = 12;
    int labelWeight = 500;
};
LIBFM_QT_API const Metrics& metrics();
LIBFM_QT_API void setMetrics(const Metrics& metrics);

// Window background opacity, 0.0 - 1.0 (1.0 = solid). Below 1.0 the
// background is see-through (blurred by the compositor if it blurs);
// text, lines and icons stay solid.
LIBFM_QT_API qreal opacity();
LIBFM_QT_API void setOpacity(qreal opacity);
// `color` with the window opacity applied as its alpha.
LIBFM_QT_API QColor translucent(const QColor& color);

// Show file icons in list-style views (detailed list, side pane).
LIBFM_QT_API bool listIcons();
LIBFM_QT_API void setListIcons(bool show);

// Colour a file name like `ls --color`: folders blue, archives red,
// images / video / audio yellow, executables green. Invalid colour = default.
LIBFM_QT_API QColor colorForFile(const std::shared_ptr<const Fm::FileInfo>& file);

// The prefix drawn before list items: "> " for the current one, "  " otherwise.
LIBFM_QT_API QString cursorPrefix(bool current);

// A box around `framed` with a label sitting on its top line, like a
// fieldset legend. It is drawn by a transparent overlay on top of the
// widget: the line runs `ttyLineY` px (default 8) below the widget's top,
// with a gap where the label is, so nothing has to paint over the line
// (which also works on a see-through background). Line and label light up
// while the mouse is over the box.
class LIBFM_QT_API FloatingLabel : public QWidget {
    Q_OBJECT
public:
    // `inset`: distance of the box's sides and bottom from the widget's edges.
    static FloatingLabel* attach(QWidget* framed, const QString& text, int inset = 0);
    void setLabel(const QString& text);
    QString label() const { return text_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    FloatingLabel(QWidget* framed, int inset);
    void sync();
    void keepOnTop();
    QPointer<QWidget> framed_;
    QString text_;
    int inset_;
    bool hot_ = false;
};

// Wrap a dialog's existing layout in a box carrying `label` on its border.
LIBFM_QT_API void boxDialog(QDialog* dialog, const QString& label);

} // namespace Tty
} // namespace Fm

#endif // FM_TTY_H

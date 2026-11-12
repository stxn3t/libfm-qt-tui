/*
 * tty.cpp - shared state for the tty (terminal-style) look of pcmanfm-tty.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "tty.h"
#include "core/fileinfo.h"
#include "core/mimetype.h"
#include "core/archiver.h"

#include <QDialog>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>

namespace Fm {
namespace Tty {

namespace {
bool g_enabled = false;
bool g_listIcons = false;
Colors g_colors;

// Where a box's top border line sits, measured from the top of the widget.
// Boxes styled by the tty stylesheet get `margin-top: 8px`, and boxDialog()
// paints its border at the same height, so one number serves both.
constexpr int kLineY = 8;

class BorderPainter : public QObject {
public:
    explicit BorderPainter(QWidget* w) : QObject(w) {}
    bool eventFilter(QObject* watched, QEvent* event) override {
        if(event->type() == QEvent::Paint) {
            QWidget* w = static_cast<QWidget*>(watched);
            QPainter p(w);
            p.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(colors().borderHover);
            pen.setWidthF(1.0);
            p.setPen(pen);
            QRectF r = QRectF(w->rect()).adjusted(4.5, kLineY + 0.5, -4.5, -4.5);
            p.drawRoundedRect(r, 3, 3);
        }
        return false;
    }
};
} // namespace

bool enabled() { return g_enabled; }
void setEnabled(bool enabled) { g_enabled = enabled; }

const Colors& colors() { return g_colors; }
void setColors(const Colors& colors) { g_colors = colors; }

bool listIcons() { return !g_enabled || g_listIcons; }
void setListIcons(bool show) { g_listIcons = show; }

QColor colorForFile(const std::shared_ptr<const Fm::FileInfo>& file) {
    if(!g_enabled || !file) {
        return QColor();
    }
    if(file->isDir()) {
        return g_colors.blue;
    }
    const auto& mime = file->mimeType();
    if(mime) {
        const char* name = mime->name();
        if(auto archiver = Archiver::defaultArchiver()) {
            if(archiver->isMimeTypeSupported(name)) {
                return g_colors.red;
            }
        }
        if(mime->isImage() || qstrncmp(name, "video/", 6) == 0 || qstrncmp(name, "audio/", 6) == 0) {
            return g_colors.yellow;
        }
        if(qstrcmp(name, "application/x-cd-image") == 0
           || qstrcmp(name, "application/vnd.efi.iso") == 0
           || qstrcmp(name, "application/zstd") == 0) {
            return g_colors.red;
        }
    }
    if(file->isExecutableType()) {
        return g_colors.green;
    }
    return QColor();
}

QString cursorPrefix(bool current) {
    return current ? QStringLiteral("> ") : QStringLiteral("  ");
}

// ---------------------------------------------------------------------------

FloatingLabel::FloatingLabel(QWidget* framed, QWidget* host)
    : QLabel(host), framed_(framed) {
    setObjectName(QStringLiteral("ttyLabel"));
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAutoFillBackground(true);
    setContentsMargins(6, 0, 6, 0);
    setHot(false);
    framed->setAttribute(Qt::WA_Hover, true);
    framed->installEventFilter(this);
    if(host != framed) {
        host->installEventFilter(this);
    }
}

FloatingLabel* FloatingLabel::attach(QWidget* framed, const QString& text, QWidget* host) {
    if(!g_enabled || !framed) {
        return nullptr;
    }
    if(!host) {
        host = framed->parentWidget();
    }
    if(!host || qobject_cast<QSplitter*>(host)) {
        host = framed;
    }
    auto label = new FloatingLabel(framed, host);
    label->setLabel(text);
    label->reposition();
    return label;
}

void FloatingLabel::setLabel(const QString& text) {
    setText(text.toLower());
    adjustSize();
    reposition();
}

void FloatingLabel::setHot(bool hot) {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, g_colors.bg);
    pal.setColor(QPalette::WindowText, hot ? g_colors.accent : g_colors.fgDim);
    setPalette(pal);
    // the stylesheet may also style #ttyLabel; keep the hover colour authoritative
    setStyleSheet(QStringLiteral("color: %1; background: %2;")
                  .arg(hot ? g_colors.accent.name() : g_colors.fgDim.name(), g_colors.bg.name()));
}

void FloatingLabel::reposition() {
    if(!framed_) {
        return;
    }
    QWidget* host = parentWidget();
    QPoint origin = (host == framed_) ? QPoint(0, 0) : framed_->mapTo(host, QPoint(0, 0));
    // a box may move its border line down (property "ttyLineY", in px)
    const QVariant lineY = framed_->property("ttyLineY");
    int y = origin.y() + (lineY.isValid() ? lineY.toInt() : kLineY) - height() / 2;
    move(origin.x() + 10, y);
    setVisible(framed_->isVisible() && !text().isEmpty());
    raise();
}

bool FloatingLabel::eventFilter(QObject* watched, QEvent* event) {
    switch(event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::LayoutRequest:
    case QEvent::DynamicPropertyChange:
        reposition();
        break;
    case QEvent::Enter:
    case QEvent::HoverEnter:
        if(watched == framed_) {
            setHot(true);
        }
        break;
    case QEvent::Leave:
    case QEvent::HoverLeave:
        if(watched == framed_) {
            setHot(false);
        }
        break;
    default:
        break;
    }
    return QLabel::eventFilter(watched, event);
}

void boxDialog(QDialog* dialog, const QString& label) {
    if(!g_enabled || !dialog) {
        return;
    }
    // keep the dialog's own layout and inset its contents inside the border
    dialog->setContentsMargins(10, kLineY + 10, 10, 10);
    dialog->installEventFilter(new BorderPainter(dialog));
    FloatingLabel::attach(dialog, label, dialog);
}

} // namespace Tty
} // namespace Fm

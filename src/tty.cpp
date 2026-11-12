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
#include <QStackedWidget>
#include <QFontMetrics>

namespace Fm {
namespace Tty {

namespace {
bool g_enabled = false;
bool g_listIcons = false;
Colors g_colors;
Metrics g_metrics;
qreal g_opacity = 1.0;

// Where a box's top border line sits, measured from the top of the widget.
// Boxes styled by the tty stylesheet get `margin-top: 8px`; a box may move
// its line down with the "ttyLineY" property.
constexpr int kLineY = 8;
} // namespace

bool enabled() { return g_enabled; }
void setEnabled(bool enabled) { g_enabled = enabled; }

const Colors& colors() { return g_colors; }
void setColors(const Colors& colors) { g_colors = colors; }

const Metrics& metrics() { return g_metrics; }
void setMetrics(const Metrics& metrics) { g_metrics = metrics; }

qreal opacity() { return g_opacity; }
void setOpacity(qreal opacity) { g_opacity = qBound(0.0, opacity, 1.0); }

QColor translucent(const QColor& color) {
    QColor c(color);
    c.setAlphaF(color.alphaF() * g_opacity);
    return c;
}

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

FloatingLabel::FloatingLabel(QWidget* framed, int inset)
    : QWidget(framed), framed_(framed), inset_(inset) {
    setObjectName(QStringLiteral("ttyLabel"));
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
    framed->setAttribute(Qt::WA_Hover, true);
    framed->installEventFilter(this);
    // a stacked widget raises the page it switches to: stay above it
    if(auto stack = qobject_cast<QStackedWidget*>(framed)) {
        connect(stack, &QStackedWidget::currentChanged, this, &FloatingLabel::keepOnTop);
    }
    sync();
}

FloatingLabel* FloatingLabel::attach(QWidget* framed, const QString& text, int inset) {
    if(!g_enabled || !framed) {
        return nullptr;
    }
    auto label = new FloatingLabel(framed, inset);
    label->setLabel(text);
    label->show();
    return label;
}

void FloatingLabel::setLabel(const QString& text) {
    text_ = text.toLower();
    update();
}

void FloatingLabel::sync() {
    if(framed_) {
        setGeometry(framed_->rect());
        keepOnTop();
        update();
    }
}

void FloatingLabel::keepOnTop() {
    if(parentWidget() && parentWidget()->children().constLast() != this) {
        raise();
    }
}

void FloatingLabel::paintEvent(QPaintEvent* /*event*/) {
    if(!framed_) {
        return;
    }
    const QVariant lineProp = framed_->property("ttyLineY");
    const int lineY = lineProp.isValid() ? lineProp.toInt() : kLineY;
    const qreal bw = g_metrics.borderWidth;
    const qreal half = bw / 2.0;
    const QRectF box = QRectF(rect()).adjusted(inset_ + half, lineY + half, -inset_ - half, -inset_ - half);
    if(box.width() <= 0 || box.height() <= 0) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // the label text and the gap it leaves in the line
    QFont font = framed_->font();
    font.setPixelSize(g_metrics.labelPixelSize);
    font.setWeight(QFont::Weight(g_metrics.labelWeight));
    p.setFont(font);
    const QFontMetrics fm(font);
    const qreal textX = box.left() + 10 + 6;
    const qreal textW = text_.isEmpty() ? 0 : fm.horizontalAdvance(text_);
    const qreal gapL = textX - 6;
    const qreal gapR = textX + textW + 6;

    QPen pen(hot_ ? g_colors.borderHover : g_colors.border);
    pen.setWidthF(bw);
    pen.setStyle(g_metrics.borderStyle);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    QPainterPath path;
    const qreal r = qMin<qreal>(g_metrics.radius, qMin(box.width(), box.height()) / 2);
    if(textW > 0) {
        // start right of the gap, go clockwise round the box, stop left of it
        path.moveTo(gapR, box.top());
        path.lineTo(box.right() - r, box.top());
        path.arcTo(QRectF(box.right() - 2 * r, box.top(), 2 * r, 2 * r), 90, -90);
        path.lineTo(box.right(), box.bottom() - r);
        path.arcTo(QRectF(box.right() - 2 * r, box.bottom() - 2 * r, 2 * r, 2 * r), 0, -90);
        path.lineTo(box.left() + r, box.bottom());
        path.arcTo(QRectF(box.left(), box.bottom() - 2 * r, 2 * r, 2 * r), 270, -90);
        path.lineTo(box.left(), box.top() + r);
        path.arcTo(QRectF(box.left(), box.top(), 2 * r, 2 * r), 180, -90);
        path.lineTo(gapL, box.top());
    }
    else {
        path.addRoundedRect(box, r, r);
    }
    p.drawPath(path);

    if(textW > 0) {
        p.setPen(hot_ ? g_colors.accent : g_colors.fgDim);
        const qreal baseline = box.top() + (fm.ascent() - fm.descent()) / 2.0;
        p.drawText(QPointF(textX, baseline), text_);
    }
}

bool FloatingLabel::eventFilter(QObject* watched, QEvent* event) {
    if(watched == framed_) {
        switch(event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::LayoutRequest:
        case QEvent::DynamicPropertyChange:
        case QEvent::FontChange:
            sync();
            break;
        case QEvent::ChildAdded:
            // keep the line above children added later (pages, popups)
            QMetaObject::invokeMethod(this, &FloatingLabel::keepOnTop, Qt::QueuedConnection);
            break;
        case QEvent::Enter:
        case QEvent::HoverEnter:
            if(!hot_) {
                hot_ = true;
                update();
            }
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            if(hot_) {
                hot_ = false;
                update();
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void boxDialog(QDialog* dialog, const QString& label) {
    if(!g_enabled || !dialog) {
        return;
    }
    // keep the dialog's own layout and inset its contents inside the box
    dialog->setContentsMargins(10, kLineY + 10, 10, 10);
    FloatingLabel::attach(dialog, label, 4);
}

} // namespace Tty
} // namespace Fm

#include "autotrybutton.h"

#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>

#include "commongraphics/commongraphics.h"
#include "dpiscalemanager.h"
#include "graphicresources/fontmanager.h"
#include "graphicresources/imageresourcessvg.h"
#include "graphicresources/independentpixmap.h"
#include "tooltips/tooltipcontroller.h"
#include "tooltips/tooltiptypes.h"

namespace ConnectWindow {

namespace {
constexpr int kHeight = 26;
constexpr int kHorizontalPadding = 12;
constexpr int kIconSize = 14;
constexpr int kIconTextGap = 6;
constexpr int kRotationDurationMs = 1200;
constexpr int kOpacityAnimationMs = 150;
constexpr int kCornerRadius = kHeight / 2;
const char *kIconPath = "REFRESH_ICON";

QColor runningColor()
{
    return QColor(0x8F, 0xD9, 0xF5);
}

QColor connectedColor()
{
    return FontManager::instance().getSeaGreenColor();
}
} // namespace

AutoTryButton::AutoTryButton(ScalableGraphicsObject *parent)
    : ClickableGraphicsObject(parent),
      state_(kIdle),
      index_(0),
      total_(0),
      width_(0),
      height_(kHeight),
      iconRotation_(0),
      textOpacity_(OPACITY_UNHOVER_TEXT)
{
    rotationAnimation_.setStartValue(0.0);
    rotationAnimation_.setEndValue(360.0);
    rotationAnimation_.setDuration(kRotationDurationMs);
    rotationAnimation_.setLoopCount(-1);
    connect(&rotationAnimation_, &QVariantAnimation::valueChanged, this, &AutoTryButton::onRotationValueChanged);

    connect(&textOpacityAnimation_, &QVariantAnimation::valueChanged, this, &AutoTryButton::onOpacityValueChanged);

    connect(this, &ClickableGraphicsObject::hoverEnter, this, &AutoTryButton::onHoverEnter);
    connect(this, &ClickableGraphicsObject::hoverLeave, this, &AutoTryButton::onHoverLeave);

    setClickableHoverable(true, true);
    recalcSize();
}

QRectF AutoTryButton::boundingRect() const
{
    return QRectF(0, 0, width_*G_SCALE, height_*G_SCALE);
}

void AutoTryButton::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QColor color = textColor();

    // Pill background.
    painter->setPen(Qt::NoPen);
    QColor background = color;
    background.setAlphaF(0.12);
    painter->setBrush(background);
    painter->drawRoundedRect(boundingRect(), kCornerRadius*G_SCALE, kCornerRadius*G_SCALE);

    // Rotating icon.
    QSharedPointer<IndependentPixmap> icon = ImageResourcesSvg::instance().getIndependentPixmap(kIconPath);
    if (icon && !icon->isNull()) {
        const int iconW = kIconSize*G_SCALE;
        const int iconH = kIconSize*G_SCALE;
        painter->save();
        painter->translate(kHorizontalPadding*G_SCALE + iconW/2.0, boundingRect().height()/2.0);
        painter->rotate(iconRotation_);
        icon->draw(-iconW/2, -iconH/2, iconW, iconH, painter, color);
        painter->restore();
    }

    // Label.
    painter->setOpacity(painter->opacity() * textOpacity_);
    painter->setPen(QPen(color, 1*G_SCALE));
    painter->setFont(FontManager::instance().getFont(FontDescr(12, QFont::DemiBold)));
    const QRectF textRect(kHorizontalPadding*G_SCALE + kIconSize*G_SCALE + kIconTextGap*G_SCALE, 0,
                          width_*G_SCALE - (kHorizontalPadding + kIconSize + kIconTextGap)*G_SCALE, boundingRect().height());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, label());
}

void AutoTryButton::updateScaling()
{
    ClickableGraphicsObject::updateScaling();
    recalcSize();
}

void AutoTryButton::setIdle()
{
    state_ = kIdle;
    index_ = 0;
    total_ = 0;
    rotationAnimation_.stop();
    iconRotation_ = 0;
    startAnAnimation<double>(textOpacityAnimation_, textOpacity_, OPACITY_UNHOVER_TEXT, kOpacityAnimationMs);
    recalcSize();
}

void AutoTryButton::setProgress(int index, int total)
{
    state_ = kRunning;
    index_ = index;
    total_ = total;
    if (rotationAnimation_.state() != QAbstractAnimation::Running) {
        rotationAnimation_.start();
    }
    startAnAnimation<double>(textOpacityAnimation_, textOpacity_, OPACITY_FULL, kOpacityAnimationMs);
    recalcSize();
}

void AutoTryButton::setConnected()
{
    state_ = kConnected;
    rotationAnimation_.stop();
    iconRotation_ = 0;
    startAnAnimation<double>(textOpacityAnimation_, textOpacity_, OPACITY_FULL, kOpacityAnimationMs);
    recalcSize();
}

void AutoTryButton::onRotationValueChanged(const QVariant &value)
{
    iconRotation_ = value.toDouble();
    update();
}

void AutoTryButton::onOpacityValueChanged(const QVariant &value)
{
    textOpacity_ = value.toDouble();
    update();
}

void AutoTryButton::onHoverEnter()
{
    startAnAnimation<double>(textOpacityAnimation_, textOpacity_, OPACITY_FULL, ANIMATION_SPEED_FAST);

    if (scene() && !scene()->views().isEmpty()) {
        QGraphicsView *view = scene()->views().first();
        const QPoint buttonGlobalPoint = view->mapToGlobal(view->mapFromScene(scenePos()));
        TooltipInfo ti(TOOLTIP_TYPE_BASIC, TOOLTIP_ID_AUTO_TRY);
        ti.x = buttonGlobalPoint.x() + static_cast<int>(boundingRect().width())/2;
        ti.y = buttonGlobalPoint.y() - 5*G_SCALE;
        ti.title = tooltip();
        ti.tailtype = TOOLTIP_TAIL_BOTTOM;
        ti.tailPosPercent = 0.5;
        ti.parent = view->viewport();
        TooltipController::instance().showTooltipBasic(ti);
    }
}

void AutoTryButton::onHoverLeave()
{
    TooltipController::instance().hideTooltip(TOOLTIP_ID_AUTO_TRY);
    if (state_ == kIdle) {
        startAnAnimation<double>(textOpacityAnimation_, textOpacity_, OPACITY_UNHOVER_TEXT, ANIMATION_SPEED_FAST);
    }
}

void AutoTryButton::recalcSize()
{
    prepareGeometryChange();

    const QFont font = FontManager::instance().getFont(FontDescr(12, QFont::DemiBold));
    const QFontMetrics fm(font);
    const int textWidth = fm.horizontalAdvance(label());

    width_ = kHorizontalPadding + kIconSize + kIconTextGap + qCeil(textWidth / G_SCALE) + kHorizontalPadding;
    height_ = kHeight;
    update();
}

QString AutoTryButton::label() const
{
    if (state_ == kRunning && total_ > 0) {
        return QString("%1/%2").arg(index_).arg(total_);
    }
    return tr("Auto-Try");
}

QString AutoTryButton::tooltip() const
{
    return state_ == kRunning ? tr("Stop Auto-Try") : tr("Try every location and protocol");
}

QColor AutoTryButton::textColor() const
{
    switch (state_) {
        case kRunning:
            return runningColor();
        case kConnected:
            return connectedColor();
        case kIdle:
        default:
            return QColor(Qt::white);
    }
}

} //namespace ConnectWindow

#pragma once

#include <QGraphicsObject>
#include <QVariantAnimation>
#include <QString>

#include "commongraphics/clickablegraphicsobject.h"
#include "graphicresources/fontdescr.h"

namespace ConnectWindow {

// Secondary pill button under the connect button. Tapping it walks every location on the current
// protocol, then every location on the next, until one connects; tapping it again stops the walk. A
// sweep in progress shows its position in the grid and spins its icon.
class AutoTryButton : public ClickableGraphicsObject
{
    Q_OBJECT
public:
    explicit AutoTryButton(ScalableGraphicsObject *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    void updateScaling() override;

    // Idle, before a sweep and once one ends without connecting.
    void setIdle();
    // `index` is one-based; `total` is the size of the grid being walked.
    void setProgress(int index, int total);
    // The sweep found a location and connected.
    void setConnected();

private slots:
    void onRotationValueChanged(const QVariant &value);
    void onOpacityValueChanged(const QVariant &value);
    void onHoverEnter();
    void onHoverLeave();

private:
    enum State { kIdle, kRunning, kConnected };

    State state_;
    int index_;
    int total_;
    int width_;
    int height_;
    double iconRotation_;
    double textOpacity_;

    QVariantAnimation rotationAnimation_;
    QVariantAnimation textOpacityAnimation_;

    void recalcSize();
    QString label() const;
    QString tooltip() const;
    QColor textColor() const;
};

} //namespace ConnectWindow

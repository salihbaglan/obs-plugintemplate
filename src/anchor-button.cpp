#include "anchor-button.hpp"
#include <QPainter>
#include <QStyleOption>

AnchorButton::AnchorButton(AnchorH h, AnchorV v, QWidget *parent)
    : QPushButton(parent), hAlign(h), vAlign(v)
{
    setFixedSize(32, 32);
    setCheckable(true);
    setAutoExclusive(true);
}

void AnchorButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    if (isChecked()) {
        p.fillRect(rect(), QColor(60, 60, 60)); // Highlight
    } else if (underMouse()) {
        p.fillRect(rect(), QColor(50, 50, 50));
    } else {
        p.fillRect(rect(), QColor(40, 40, 40));
    }

    // Margins for the "Canvas" representation
    int m = 4;
    QRect canvas = rect().adjusted(m, m, -m, -m);
    
    // Draw Canvas Border
    p.setPen(QPen(QColor(100, 100, 100), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(canvas);

    // Setup for Anchor Lines (Red)
    p.setPen(QPen(QColor(255, 100, 100), 1)); // Reddish

    // Horizontal Anchor Lines
    int midX = canvas.center().x();
    int midY = canvas.center().y();

    switch (hAlign) {
        case AnchorH::Left:
            p.drawLine(canvas.left(), canvas.top() - 2, canvas.left(), canvas.bottom() + 2);
            break;
        case AnchorH::Center:
            p.drawLine(midX, canvas.top() + 2, midX, canvas.bottom() - 2);
            break;
        case AnchorH::Right:
            p.drawLine(canvas.right(), canvas.top() - 2, canvas.right(), canvas.bottom() + 2);
            break;
        case AnchorH::Stretch:
            // Arrows or just indicators at corners? Unity uses blue arrows for stretch usually
            // but the RED anchors are usually at corners for stretch.
            // Let's draw slight corners
            p.drawLine(canvas.left(), canvas.center().y(), canvas.left() + 2, canvas.center().y());
            p.drawLine(canvas.right(), canvas.center().y(), canvas.right() - 2, canvas.center().y());
            break;
    }

    // Vertical Anchor Lines
    switch (vAlign) {
        case AnchorV::Top:
            p.drawLine(canvas.left() - 2, canvas.top(), canvas.right() + 2, canvas.top());
            break;
        case AnchorV::Middle:
            p.drawLine(canvas.left() + 2, midY, canvas.right() - 2, midY);
            break;
        case AnchorV::Bottom:
            p.drawLine(canvas.left() - 2, canvas.bottom(), canvas.right() + 2, canvas.bottom());
            break;
        case AnchorV::Stretch:
            p.drawLine(canvas.center().x(), canvas.top(), canvas.center().x(), canvas.top() + 2);
            p.drawLine(canvas.center().x(), canvas.bottom(), canvas.center().x(), canvas.bottom() - 2);
            break;
    }

    // Draw the "Item" representation (Box or Arrows)
    // Blue/Cyan color
    p.setPen(QPen(QColor(0, 200, 255), 1.5));
    
    int itemSize = 8;
    QRect itemRect(0, 0, itemSize, itemSize);

    // Calculate Item Position
    int x = midX;
    int y = midY;

    if (hAlign == AnchorH::Left) x = canvas.left() + 6;
    if (hAlign == AnchorH::Right) x = canvas.right() - 6;
    if (vAlign == AnchorV::Top) y = canvas.top() + 6;
    if (vAlign == AnchorV::Bottom) y = canvas.bottom() - 6;

    itemRect.moveCenter(QPoint(x, y));

    // Handle Stretch Visualization
    if (hAlign == AnchorH::Stretch && vAlign == AnchorV::Stretch) {
        // Full stretch arrows
        p.drawLine(canvas.center(), QPoint(canvas.left() + 4, canvas.center().y()));
        p.drawLine(canvas.center(), QPoint(canvas.right() - 4, canvas.center().y()));
        p.drawLine(canvas.center(), QPoint(canvas.center().x(), canvas.top() + 4));
        p.drawLine(canvas.center(), QPoint(canvas.center().x(), canvas.bottom() - 4));
        // Arrow heads
        // ... simple lines for now
    } else if (hAlign == AnchorH::Stretch) {
         p.drawLine(x - 6, y, x + 6, y);
         // Arrow heads
         p.drawLine(x - 6, y, x - 4, y - 2);
         p.drawLine(x - 6, y, x - 4, y + 2);
         p.drawLine(x + 6, y, x + 4, y - 2);
         p.drawLine(x + 6, y, x + 4, y + 2);
    } else if (vAlign == AnchorV::Stretch) {
         p.drawLine(x, y - 6, x, y + 6);
          // Arrow heads
         p.drawLine(x, y - 6, x - 2, y - 4);
         p.drawLine(x, y - 6, x + 2, y - 4);
         p.drawLine(x, y + 6, x - 2, y + 4);
         p.drawLine(x, y + 6, x + 2, y + 4);
    } else {
        // Normal Box
        p.drawRect(itemRect);
    }
    
    // Draw Center Dot (Pivot) if not stretch?
    if (hAlign != AnchorH::Stretch && vAlign != AnchorV::Stretch) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 200, 0)); // Yellow dot
        p.drawEllipse(itemRect.center(), 1, 1);
    }
}

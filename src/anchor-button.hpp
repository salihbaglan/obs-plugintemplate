#pragma once

#include <QPushButton>
#include <QPaintEvent>

enum class AnchorH { Left, Center, Right, Stretch };
enum class AnchorV { Top, Middle, Bottom, Stretch };

class AnchorButton : public QPushButton {
    Q_OBJECT

public:
    AnchorButton(AnchorH h, AnchorV v, QWidget *parent = nullptr);

    AnchorH horizontal() const { return hAlign; }
    AnchorV vertical() const { return vAlign; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    AnchorH hAlign;
    AnchorV vAlign;
};

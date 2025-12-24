#pragma once

#include <QWidget>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include "anchor-button.hpp"

class QSpinBox;
class QPushButton;
class QLabel;
class QStackedLayout;

class SourceResizerDock : public QWidget {
    Q_OBJECT

public:
    explicit SourceResizerDock(QWidget *parent = nullptr);
    ~SourceResizerDock() override;

    // Called by global event callback or self-registered
    void HandleFrontendEvent(enum obs_frontend_event event);

public slots:
    void RefreshFromSelection();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void handleResize();
    void handlePositionChange();
    void onAnchorClicked();
    void updateModifierLabels();

private:
    void SubscribeToScene(obs_scene_t *scene);
    void UnsubscribeFromScene();
    void ApplyAnchorPreset(AnchorH h, AnchorV v);
    
    // Static callbacks for OBS signals
    static void OBSSceneItemSignal(void *data, calldata_t *cd);

    QStackedLayout *mainStack;
    QWidget *controlsWidget;
    QLabel *noSelectionLabel;
    
    QSpinBox *widthSpin;
    QSpinBox *heightSpin;
    QSpinBox *xSpin;
    QSpinBox *ySpin;
    
    QLabel *shiftLabel;
    QLabel *altLabel;

    obs_source_t *trackedSource = nullptr;
    signal_handler_t *sceneSignalHandler = nullptr;
};

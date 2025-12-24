#include "source-resizer-dock.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QMetaObject>
#include <QTimer>
#include <QKeyEvent>
#include <QApplication>
#include <QGridLayout>
#include <QStackedLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <functional>
#include <utility>
#include "anchor-button.hpp"

// Global callback wrapper
static void frontend_event_callback(enum obs_frontend_event event, void *param)
{
    SourceResizerDock *dock = reinterpret_cast<SourceResizerDock*>(param);
    dock->HandleFrontendEvent(event);
}

// Helper for recursive enumeration of selected items
static void EnumSelectedItemsRecursive(obs_scene_t *scene, std::function<void(obs_sceneitem_t*)> callback) {
    obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
        auto *cb = reinterpret_cast<std::function<void(obs_sceneitem_t*)>*>(param);
        
        if (obs_sceneitem_selected(item)) {
            (*cb)(item);
        }
        
        if (obs_sceneitem_is_group(item)) {
            obs_source_t *groupSource = obs_sceneitem_get_source(item);
            obs_scene_t *groupScene = obs_scene_from_source(groupSource);
            if (groupScene) {
                EnumSelectedItemsRecursive(groupScene, *cb);
            }
        }
        return true;
    }, &callback);
}

SourceResizerDock::SourceResizerDock(QWidget *parent) : QWidget(parent)
{
    // Main Stack Layout
    mainStack = new QStackedLayout(this);

    // 1. No Selection Widget
    noSelectionLabel = new QLabel("Select a source to edit", this);
    noSelectionLabel->setAlignment(Qt::AlignCenter);
    noSelectionLabel->setStyleSheet("color: gray; font-style: italic;");
    mainStack->addWidget(noSelectionLabel);

    // 2. Controls Widget
    controlsWidget = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(controlsWidget);
    rootLayout->setContentsMargins(5, 5, 5, 5);
    rootLayout->setSpacing(5);

    // TOP: Name and Visibility
    QHBoxLayout *topLayout = new QHBoxLayout();
    visCheck = new QCheckBox(this);
    visCheck->setToolTip("Toggle Visibility");
    connect(visCheck, &QCheckBox::checkStateChanged, this, &SourceResizerDock::handleVisibility);
    
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText("Source Name");
    connect(nameEdit, &QLineEdit::editingFinished, this, &SourceResizerDock::handleRenaming);

    topLayout->addWidget(visCheck);
    topLayout->addWidget(nameEdit);
    rootLayout->addLayout(topLayout);

    // MIDDLE: Unity RectTransform Area
    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(10);
    rootLayout->addLayout(mainLayout);

    // LEFT: Main Anchor Button
    mainAnchorBtn = new AnchorButton(AnchorH::Center, AnchorV::Middle, this);
    mainAnchorBtn->setFixedSize(60, 60);
    mainAnchorBtn->setCursor(Qt::PointingHandCursor);
    mainAnchorBtn->setToolTip("Anchor Presets");
    connect(mainAnchorBtn, &QPushButton::clicked, this, &SourceResizerDock::toggleAnchorPopup);
    
    // Container for Left side to align top
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(mainAnchorBtn);
    leftLayout->addStretch();
    mainLayout->addLayout(leftLayout);

    // RIGHT: Fields Grid (Pos X, Pos Y, W, H)
    QGridLayout *fieldGrid = new QGridLayout();
    fieldGrid->setSpacing(5);
    
    // Row 0: Labels
    fieldGrid->addWidget(new QLabel("Pos X"), 0, 0);
    fieldGrid->addWidget(new QLabel("Pos Y"), 0, 1);
    
    // Row 1: SpinBoxes
    xSpin = new QSpinBox(this);
    xSpin->setRange(-10000, 10000);
    xSpin->setButtonSymbols(QAbstractSpinBox::NoButtons); 
    
    ySpin = new QSpinBox(this);
    ySpin->setRange(-10000, 10000);
    ySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    fieldGrid->addWidget(xSpin, 1, 0);
    fieldGrid->addWidget(ySpin, 1, 1);

    // Row 2: Labels
    fieldGrid->addWidget(new QLabel("Width"), 2, 0);
    fieldGrid->addWidget(new QLabel("Height"), 2, 1);

    // Row 3: SpinBoxes
    widthSpin = new QSpinBox(this);
    widthSpin->setRange(1, 10000);
    widthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    
    heightSpin = new QSpinBox(this);
    heightSpin->setRange(1, 10000);
    heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);

    fieldGrid->addWidget(widthSpin, 3, 0);
    fieldGrid->addWidget(heightSpin, 3, 1);
    
    // Add Row 4
    fieldGrid->setRowStretch(4, 1);

    mainLayout->addLayout(fieldGrid);
    
    mainStack->addWidget(controlsWidget);
    mainStack->setCurrentWidget(noSelectionLabel);

    // Create the Popup (Hidden by default)
    CreateAnchorPopup();

    // Connect input signals
    connect(widthSpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handleResize);
    connect(heightSpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handleResize);
    connect(xSpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handlePositionChange);
    connect(ySpin, &QSpinBox::valueChanged, this, &SourceResizerDock::handlePositionChange);

    // Modifier Timer
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SourceResizerDock::updateModifierLabels);
    timer->start(100);

    // Init OBS
    obs_frontend_add_event_callback(frontend_event_callback, this);
    
    // Initial subscription
    obs_source_t *source = obs_frontend_get_current_scene();
    if (source) {
        obs_scene_t *scene = obs_scene_from_source(source);
        SubscribeToScene(scene);
        obs_source_release(source);
    }
    RefreshFromSelection();
}

void SourceResizerDock::CreateAnchorPopup()
{
    anchorPopup = new QWidget(this, Qt::Popup);
    anchorPopup->setStyleSheet("background-color: #333; border: 1px solid #555;");
    QVBoxLayout *layout = new QVBoxLayout(anchorPopup);
    layout->setContentsMargins(5, 5, 5, 5);
    
    QHBoxLayout *modLayout = new QHBoxLayout();
    shiftLabel = new QLabel("Shift: Pivot", anchorPopup);
    altLabel = new QLabel("Alt: Position", anchorPopup);
    shiftLabel->setStyleSheet("color: gray;");
    altLabel->setStyleSheet("color: gray;");
    modLayout->addWidget(shiftLabel);
    modLayout->addWidget(altLabel);
    layout->addLayout(modLayout);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(2);
    
    AnchorV vRows[] = { AnchorV::Top, AnchorV::Middle, AnchorV::Bottom, AnchorV::Stretch };
    AnchorH hCols[] = { AnchorH::Left, AnchorH::Center, AnchorH::Right, AnchorH::Stretch };

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            AnchorButton *btn = new AnchorButton(hCols[c], vRows[r], anchorPopup);
            connect(btn, &QPushButton::clicked, this, &SourceResizerDock::onAnchorClicked);
            grid->addWidget(btn, r, c);
        }
    }
    layout->addLayout(grid);
}

void SourceResizerDock::toggleAnchorPopup()
{
    if (anchorPopup->isVisible()) {
        anchorPopup->hide();
    } else {
        QPoint p = mainAnchorBtn->mapToGlobal(QPoint(0, mainAnchorBtn->height()));
        anchorPopup->move(p);
        anchorPopup->show();
    }
}

SourceResizerDock::~SourceResizerDock()
{
    UnsubscribeFromScene();
    obs_frontend_remove_event_callback(frontend_event_callback, this);
}

void SourceResizerDock::updateModifierLabels()
{
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    shiftLabel->setStyleSheet(mods & Qt::ShiftModifier ? "color: #00AAFF; font-weight: bold;" : "color: gray;");
    altLabel->setStyleSheet(mods & Qt::AltModifier ? "color: #00AAFF; font-weight: bold;" : "color: gray;");
}

void SourceResizerDock::keyPressEvent(QKeyEvent *event) { updateModifierLabels(); QWidget::keyPressEvent(event); }
void SourceResizerDock::keyReleaseEvent(QKeyEvent *event) { updateModifierLabels(); QWidget::keyReleaseEvent(event); }

void SourceResizerDock::SubscribeToScene(obs_scene_t *scene)
{
    UnsubscribeAll();
    SubscribeRecursive(scene);
}

void SourceResizerDock::SubscribeRecursive(obs_scene_t *scene)
{
    obs_source_t *source = obs_scene_get_source(scene);
    if (!source) return;

    // Check if already tracked
    for (auto *s : trackedSources) {
        if (s == source) return;
    }

    obs_source_get_ref(source);
    trackedSources.push_back(source);

    signal_handler_t *sh = obs_source_get_signal_handler(source);
    if (sh) {
        signal_handler_connect(sh, "item_select", OBSSceneItemSignal, this);
        signal_handler_connect(sh, "item_deselect", OBSSceneItemSignal, this);
        signal_handler_connect(sh, "item_transform", OBSSceneItemSignal, this);
    }
    
    // Recurse into groups
    obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *param) {
        if (obs_sceneitem_is_group(item)) {
            obs_source_t *gSource = obs_sceneitem_get_source(item);
            obs_scene_t *gScene = obs_scene_from_source(gSource);
            if (gScene) {
                SourceResizerDock *dock = reinterpret_cast<SourceResizerDock*>(param);
                dock->SubscribeRecursive(gScene);
            }
        }
        return true;
    }, this);
}

void SourceResizerDock::UnsubscribeAll()
{
    for (obs_source_t *source : trackedSources) {
        signal_handler_t *sh = obs_source_get_signal_handler(source);
        if (sh) {
            signal_handler_disconnect(sh, "item_select", OBSSceneItemSignal, this);
            signal_handler_disconnect(sh, "item_deselect", OBSSceneItemSignal, this);
            signal_handler_disconnect(sh, "item_transform", OBSSceneItemSignal, this);
        }
        obs_source_release(source);
    }
    trackedSources.clear();
}

void SourceResizerDock::UnsubscribeFromScene()
{
    UnsubscribeAll();
}

void SourceResizerDock::OBSSceneItemSignal(void *data, calldata_t *cd)
{
    SourceResizerDock *dock = reinterpret_cast<SourceResizerDock*>(data);
    QMetaObject::invokeMethod(dock, "RefreshFromSelection", Qt::QueuedConnection);
}

void SourceResizerDock::HandleFrontendEvent(enum obs_frontend_event event)
{
    if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        obs_source_t *source = obs_frontend_get_current_scene();
        if (source) {
            obs_scene_t *scene = obs_scene_from_source(source);
            SubscribeToScene(scene);
            obs_source_release(source);
            RefreshFromSelection();
        }
    }
}

void SourceResizerDock::handleRenaming()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
         obs_source_t *itemSource = obs_sceneitem_get_source(item);
         if (itemSource) {
             obs_source_set_name(itemSource, nameEdit->text().toUtf8().constData());
         }
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

void SourceResizerDock::handleVisibility(int state)
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    bool visible = (state == Qt::Checked);

    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
         obs_sceneitem_set_visible(item, visible);
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

void SourceResizerDock::RefreshFromSelection()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) {
        obs_source_release(source);
        return;
    }

    // Ensure we are tracking this scene
    SubscribeToScene(scene);

    obs_sceneitem_t *selectedItem = nullptr;
    
    // Context for recursion
    struct FindData {
        obs_sceneitem_t **target; 
        std::function<bool(obs_scene_t*)> recurse;
    };

    // Recursive finder
    std::function<bool(obs_scene_t*)> findRecursive = [&](obs_scene_t *currentScene) -> bool {
        FindData data;
        data.target = &selectedItem;
        data.recurse = findRecursive;
        
        obs_scene_enum_items(currentScene, [](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
            FindData *pData = (FindData*)param;

            if (obs_sceneitem_selected(item)) {
                *(pData->target) = item;
                return false; // Stop enumeration, found it
            }
            
            if (obs_sceneitem_is_group(item)) {
                obs_source_t *groupSource = obs_sceneitem_get_source(item);
                obs_scene_t *groupScene = obs_scene_from_source(groupSource);
                if (groupScene) {
                    if (pData->recurse(groupScene)) {
                        return false; 
                    }
                }
            }
            return true;
        }, (void*)&data);
        
        return (selectedItem != nullptr);
    };

    findRecursive(scene);

    // Update UI
    if (selectedItem) {
        mainStack->setCurrentWidget(controlsWidget);

        struct vec2 pos;
        obs_sceneitem_get_pos(selectedItem, &pos);
        
        struct vec2 scale;
        obs_sceneitem_get_scale(selectedItem, &scale);
        
        obs_source_t *itemSource = obs_sceneitem_get_source(selectedItem);
        float itemW = 0.0f;
        float itemH = 0.0f;
        const char* name = "";
        bool visible = true;
        
        if (itemSource) {
            itemW = (float)obs_source_get_width(itemSource) * scale.x;
            itemH = (float)obs_source_get_height(itemSource) * scale.y;
            name = obs_source_get_name(itemSource);
            
             // Handle bounds if present
            if (obs_sceneitem_get_bounds_type(selectedItem) != OBS_BOUNDS_NONE) {
                struct vec2 bounds;
                obs_sceneitem_get_bounds(selectedItem, &bounds);
                itemW = bounds.x;
                itemH = bounds.y;
            }
        }
        visible = obs_sceneitem_visible(selectedItem);

        // Block signals to prevent feedback loop
        widthSpin->blockSignals(true);
        heightSpin->blockSignals(true);
        xSpin->blockSignals(true);
        ySpin->blockSignals(true);
        nameEdit->blockSignals(true);
        visCheck->blockSignals(true);

        widthSpin->setValue((int)itemW);
        heightSpin->setValue((int)itemH);
        xSpin->setValue((int)pos.x);
        ySpin->setValue((int)pos.y);
        nameEdit->setText(QString::fromUtf8(name));
        visCheck->setChecked(visible);

        widthSpin->blockSignals(false);
        heightSpin->blockSignals(false);
        xSpin->blockSignals(false);
        ySpin->blockSignals(false);
        nameEdit->blockSignals(false);
        visCheck->blockSignals(false);
        
        this->setEnabled(true);
    } else {
        mainStack->setCurrentWidget(noSelectionLabel);
    }

    obs_source_release(source);
}

void SourceResizerDock::handleResize()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) {
        obs_source_release(source);
        return;
    }

    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
        obs_source_t *itemSource = obs_sceneitem_get_source(item);
        if (!itemSource) return;

        float targetW = (float)widthSpin->value();
        float targetH = (float)heightSpin->value();
        
        if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
             struct vec2 bounds;
             bounds.x = targetW;
             bounds.y = targetH;
             obs_sceneitem_set_bounds(item, &bounds);
        } else {
            uint32_t sourceW = obs_source_get_width(itemSource);
            uint32_t sourceH = obs_source_get_height(itemSource);
            
            if (sourceW == 0 || sourceH == 0) return;

            struct vec2 newScale;
            newScale.x = targetW / sourceW;
            newScale.y = targetH / sourceH;

            obs_sceneitem_set_scale(item, &newScale);
        }
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

void SourceResizerDock::handlePositionChange()
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) {
        obs_source_release(source);
        return;
    }

    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
        struct vec2 newPos;
        newPos.x = (float)xSpin->value();
        newPos.y = (float)ySpin->value();

        obs_sceneitem_set_pos(item, &newPos);
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

void SourceResizerDock::onAnchorClicked()
{
    AnchorButton *btn = qobject_cast<AnchorButton*>(sender());
    if (!btn) return;
    
    ApplyAnchorPreset(btn->horizontal(), btn->vertical());
}

void SourceResizerDock::ApplyAnchorPreset(AnchorH h, AnchorV v)
{
    obs_source_t *source = obs_frontend_get_current_scene();
    if (!source) return;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) { obs_source_release(source); return; }

    uint32_t canvasW = obs_source_get_width(source);
    uint32_t canvasH = obs_source_get_height(source);
    
    Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
    bool setPivot = (mods & Qt::ShiftModifier);
    bool setPosition = (mods & Qt::AltModifier);
    
    std::function<void(obs_sceneitem_t*)> action = [&](obs_sceneitem_t *item) {
        // 1. alignment logic (Shift)
        uint32_t newAlign = 0;
        if (h == AnchorH::Left) newAlign |= OBS_ALIGN_LEFT;
        else if (h == AnchorH::Right) newAlign |= OBS_ALIGN_RIGHT;
        else if (h == AnchorH::Center) newAlign |= 0; // Center is 0
        
        if (v == AnchorV::Top) newAlign |= OBS_ALIGN_TOP;
        else if (v == AnchorV::Bottom) newAlign |= OBS_ALIGN_BOTTOM;

        if (setPivot) {
             obs_sceneitem_set_alignment(item, newAlign);
        }

        // 2. Position/Size Logic (Alt)
        if (setPosition) {
             bool stretchX = (h == AnchorH::Stretch);
             bool stretchY = (v == AnchorV::Stretch);

             // Current dimensions
             struct vec2 scale;
             obs_sceneitem_get_scale(item, &scale);
             obs_source_t *itemSource = obs_sceneitem_get_source(item);
             float currentW = obs_source_get_width(itemSource) * scale.x;
             float currentH = obs_source_get_height(itemSource) * scale.y;
             
             // If bounds exist, use them
             if (obs_sceneitem_get_bounds_type(item) != OBS_BOUNDS_NONE) {
                  struct vec2 b;
                  obs_sceneitem_get_bounds(item, &b);
                  currentW = b.x;
                  currentH = b.y;
             }

             // Calculate Target Size
             float targetW = currentW;
             float targetH = currentH;
             
             if (stretchX || stretchY) {
                 // Ensure bounds enabled
                 if (obs_sceneitem_get_bounds_type(item) == OBS_BOUNDS_NONE) {
                      obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_STRETCH);
                 }
                 if (stretchX) targetW = (float)canvasW;
                 if (stretchY) targetH = (float)canvasH;
                 
                 struct vec2 bounds = { targetW, targetH };
                 obs_sceneitem_set_bounds(item, &bounds);
             }

             // Calculate Target Top-Left Position
             float boxX = 0;
             float boxY = 0;

             // X Axis
             if (h == AnchorH::Left) boxX = 0;
             else if (h == AnchorH::Right) boxX = canvasW - targetW;
             else if (h == AnchorH::Center) boxX = (canvasW - targetW) / 2.0f;
             else if (h == AnchorH::Stretch) boxX = 0; // Full width starts at 0

             // Y Axis
             if (v == AnchorV::Top) boxY = 0;
             else if (v == AnchorV::Bottom) boxY = canvasH - targetH;
             else if (v == AnchorV::Middle) boxY = (canvasH - targetH) / 2.0f;
             else if (v == AnchorV::Stretch) boxY = 0; // Full height starts at 0

             // Convert Top-Left boxX/boxY to Alignment Point Position
             uint32_t align = obs_sceneitem_get_alignment(item);
             struct vec2 finalPos = { boxX, boxY };

             // Add offset based on alignment
             if (align & OBS_ALIGN_RIGHT) finalPos.x += targetW;
             else if (!(align & OBS_ALIGN_LEFT)) finalPos.x += targetW / 2.0f; // Center

             if (align & OBS_ALIGN_BOTTOM) finalPos.y += targetH;
             else if (!(align & OBS_ALIGN_TOP)) finalPos.y += targetH / 2.0f; // Center

             obs_sceneitem_set_pos(item, &finalPos);
        }
    };

    EnumSelectedItemsRecursive(scene, action);

    obs_source_release(source);
}

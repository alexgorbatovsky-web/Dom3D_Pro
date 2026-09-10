#pragma once
#include "../KitchenLayout.h"
class QWidget;
bool EditKitchenLayoutDialog(QWidget* parent, KitchenLayout& value, bool creating, QWidget* sceneHost = nullptr);
bool EditKitchenModuleDialog(QWidget* parent, KitchenLayout& value, int uid);

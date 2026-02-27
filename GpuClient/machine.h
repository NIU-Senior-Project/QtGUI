#pragma once
#include <QString>

struct Machine {
    QString name;
    QString gpuModel;
    QString vram;     // 例如 "24GB"
    QString status;   // 可先留空
};
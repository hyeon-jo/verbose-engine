#pragma once

#include <QWidget>

class SensorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SensorWindow(QWidget* parent = nullptr);
    void setupUI();
}; 
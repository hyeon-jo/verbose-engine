#include "sensor_window.hpp"
#include <QVBoxLayout>
#include <QLabel>

SensorWindow::SensorWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Sensor Window");
    setupUI();
    show();
}

void SensorWindow::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* label = new QLabel("Sensor Data", this);
    layout->addWidget(label);
} 
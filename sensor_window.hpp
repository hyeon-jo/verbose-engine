#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QImage>
#include <vector>
#include <array>

class SensorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SensorWindow(QWidget* parent = nullptr);
    void setupUI();
    void updateImageFrame(int streamIndex, const std::vector<uint8_t>& yuv422Data, int width, int height);

private slots:
    void updateDisplay();

private:
    QImage convertYUV422ToRGB(const std::vector<uint8_t>& yuv422Data, int width, int height);
    
    QGridLayout* layout;
    std::array<QLabel*, 4> imageLabels;
    QTimer* updateTimer;
    int frameWidth;
    int frameHeight;
    std::array<std::vector<uint8_t>, 4> frameData;  // 각 스트림의 현재 프레임 데이터
}; 
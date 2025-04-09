#include "sensor_window.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTimer>

SensorWindow::SensorWindow(QWidget* parent)
    : QWidget(parent)
    , frameWidth(320)  // 각 스트림의 기본 크기
    , frameHeight(240)
{
    setWindowTitle("Multi-Camera Stream");
    setupUI();

    // Initialize update timer
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &SensorWindow::updateDisplay);
    updateTimer->start(33);  // ~30fps

    // 각 스트림의 프레임 데이터 초기화
    for (auto& data : frameData) {
        data.resize(frameWidth * frameHeight * 2);  // YUV422 포맷
    }
}

void SensorWindow::setupUI()
{
    layout = new QGridLayout(this);
    layout->setSpacing(5);  // 스트림 간 간격 설정
    setLayout(layout);

    // 2x2 그리드로 4개의 이미지 레이블 생성
    for (int i = 0; i < 4; ++i) {
        imageLabels[i] = new QLabel(this);
        imageLabels[i]->setMinimumSize(frameWidth, frameHeight);
        imageLabels[i]->setAlignment(Qt::AlignCenter);
        imageLabels[i]->setStyleSheet("QLabel { background-color: black; }");
        
        // 2x2 그리드에 배치
        layout->addWidget(imageLabels[i], i / 2, i % 2);
    }

    // Set window properties
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    resize(frameWidth * 2 + 10, frameHeight * 2 + 10);  // 간격을 고려한 전체 크기
}

QImage SensorWindow::convertYUV422ToRGB(const std::vector<uint8_t>& yuv422Data, int width, int height)
{
    QImage rgbImage(width, height, QImage::Format_RGB888);
    
    // YUV422 포맷: [Y0 U0 Y1 V0] [Y2 U1 Y3 V1] ...
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int index = (y * width + x) * 2;
            if (index + 3 >= yuv422Data.size()) break;

            // YUV 값 추출
            int y0 = yuv422Data[index];
            int u = yuv422Data[index + 1];
            int y1 = yuv422Data[index + 2];
            int v = yuv422Data[index + 3];

            // YUV를 RGB로 변환 (BT.601 표준)
            auto convertToRGB = [](int y, int u, int v) {
                y = std::max(0, std::min(255, y));
                u = u - 128;
                v = v - 128;

                int r = y + (1.370705 * v);
                int g = y - (0.337633 * u) - (0.698001 * v);
                int b = y + (1.732446 * u);

                r = std::max(0, std::min(255, r));
                g = std::max(0, std::min(255, g));
                b = std::max(0, std::min(255, b));

                return qRgb(r, g, b);
            };

            // 두 픽셀에 대해 RGB 값 설정
            rgbImage.setPixel(x, y, convertToRGB(y0, u, v));
            if (x + 1 < width) {
                rgbImage.setPixel(x + 1, y, convertToRGB(y1, u, v));
            }
        }
    }
    
    return rgbImage;
}

void SensorWindow::updateImageFrame(int streamIndex, const std::vector<uint8_t>& yuv422Data, int width, int height)
{
    if (streamIndex < 0 || streamIndex >= 4 || yuv422Data.empty() || width <= 0 || height <= 0) return;

    // 프레임 데이터 업데이트
    frameData[streamIndex] = yuv422Data;
    
    // 이미지 변환 및 표시
    QImage rgbImage = convertYUV422ToRGB(yuv422Data, width, height);
    imageLabels[streamIndex]->setPixmap(QPixmap::fromImage(rgbImage).scaled(
        imageLabels[streamIndex]->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void SensorWindow::updateDisplay()
{
    // 테스트용 더미 YUV422 데이터 생성
    // 실제로는 서버로부터 받은 데이터를 사용해야 합니다
    for (int i = 0; i < 4; ++i) {
        std::vector<uint8_t> dummyData(frameWidth * frameHeight * 2);
        for (size_t j = 0; j < dummyData.size(); j += 4) {
            // 각 스트림마다 다른 패턴 생성
            uint8_t y = (i * 64 + j / 4) % 255;
            dummyData[j] = y;       // Y0
            dummyData[j + 1] = 128; // U
            dummyData[j + 2] = y;   // Y1
            dummyData[j + 3] = 128; // V
        }
        updateImageFrame(i, dummyData, frameWidth, frameHeight);
    }
} 
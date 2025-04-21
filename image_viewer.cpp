#include "image_viewer.hpp"
#include <QImage>
#include <QPixmap>

ImageViewer::ImageViewer(QWidget* parent) : QWidget(parent) {
    setupUI();
}

ImageViewer::~ImageViewer() {
    for (auto label : imageLabels) {
        delete label;
    }
}

void ImageViewer::setupUI() {
    layout = new QGridLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);

    for (int i = 0; i < 4; ++i) {
        imageLabels[i] = new QLabel(this);
        imageLabels[i]->setMinimumSize(320, 240);
        imageLabels[i]->setAlignment(Qt::AlignCenter);
        imageLabels[i]->setStyleSheet("QLabel { background-color: black; }");
        layout->addWidget(imageLabels[i], i / 2, i % 2);
    }

    setLayout(layout);
    setWindowTitle("Image Viewer");
    resize(800, 600);
}

void ImageViewer::updateImage(int index, const cv::Mat& image) {
    if (index >= 0 && index < 4) {
        convertAndDisplay(index, image);
    }
}

void ImageViewer::convertAndDisplay(int index, const cv::Mat& image) {
    if (image.empty()) return;

    cv::Mat rgbImage;
    if (image.channels() == 1) {
        cv::cvtColor(image, rgbImage, cv::COLOR_GRAY2RGB);
    } else if (image.channels() == 3) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    } else {
        return;
    }

    QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(qImage);
    imageLabels[index]->setPixmap(pixmap.scaled(imageLabels[index]->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
} 
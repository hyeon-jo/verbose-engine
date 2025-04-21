#pragma once

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <opencv2/opencv.hpp>

class ImageViewer : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewer(QWidget* parent = nullptr);
    ~ImageViewer();

public slots:
    void updateImage(int index, const cv::Mat& image);

private:
    void setupUI();
    void convertAndDisplay(int index, const cv::Mat& image);

    QGridLayout* layout;
    std::array<QLabel*, 4> imageLabels;
}; 
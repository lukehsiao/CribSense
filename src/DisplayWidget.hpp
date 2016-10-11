#ifndef DISPLAY_WIDGET_H_INCLUDED
#define DISPLAY_WIDGET_H_INCLUDED

#include <QtGui>
#include <QtWidgets>
#include <opencv2/core/core.hpp>
#include <memory>


// See the following link
//
// http://qtandopencv.blogspot.com/2013/08/how-to-convert-between-cvmat-and-qimage.html
//
// for the image conversion in DisplayWidget::show().


// A rigid video display of OpenCV Mat frames wired into the Qt UI.
//
class DisplayWidget : public QWidget {

    Q_OBJECT

    std::unique_ptr<uchar[]> buffer;
    QImage itsQimage;

public:

    void paintEvent(QPaintEvent *e) {
        e->accept();
        QPainter(this).drawImage(0, 0, itsQimage);
    }

    // Show the OpenCV image in m.
    //
    void show(const cv::Mat &m)
    {
        static const QImage::Format rgb888 = QImage::Format_RGB888;
        if (m.type() == CV_8UC3) {
            // make a copy of the data
            buffer = std::unique_ptr<uchar[]>(new uchar[m.rows * m.step]);
            if (buffer == nullptr)
                throw std::bad_alloc();
            memcpy(buffer.get(), m.data, m.rows * m.step);
            itsQimage
                = QImage(buffer.get(), m.cols, m.rows, m.step, rgb888).rgbSwapped();
        } else {
            qWarning() << "Image type" << m.type() << "is not" << CV_8UC3
                       << "(CV_8UC3) so it cannot be displayed.";
        }
        update();
    }

    DisplayWidget(const cv::Size &frameSize, QWidget *parent = 0)
        : QWidget(parent)
        , itsQimage()
    {
        setMinimumSize(frameSize.width, frameSize.height);
        setMaximumSize(frameSize.width, frameSize.height);
    }
};

#endif // #ifndef DISPLAY_WIDGET_H_INCLUDED

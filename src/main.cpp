#include "MainDialog.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"
#include "WorkerThread.hpp"
#include <future>
#include "INIReader.h"
#include "MotionDetection.hpp"

// Return true iff can write cl.outFile to work around VideoWriter.
//
#include <fstream>
static bool canWriteOutFileHackRandomHack(const CommandLine &cl)
{
    const char *const file = cl.outFile.c_str();
    const int random = rand();
    { std::ofstream ofs(file); ofs << cl.program << " " << random; }
    std::ifstream ifs(file); std::string hack; int r; ifs >> hack >> r;
    if (cl.program == hack && random == r) return true;
    std::cerr << cl.program << ": Cannot write file: " << cl.outFile
              << std::endl;
    return false;
}


// Return the frame rate.  Use the source frame rate unless it is a camera
// reporting less than minimumFps.  Some cameras report 0.0 FPS, so if the
// source is a camera, estimate the rate over the first 100 frames assuming
// minimumFps, and then discard them.
//
// Write minimumFps to the output file initially so the output looks silly
// while measuring the frame rate.
//
static double measureFpsHack(const CommandLine &cl, VideoSource &source)
{
    MeasureFps mfps(source.fps());
    if (source.isCamera()) {
        static const double minimumFps = MeasureFps::minimumFps();
        const int codec = source.fourCcCodec();
        const cv::Size size = source.frameSize();
        RieszTransform rt; cl.apply(rt); rt.fps(minimumFps);
        cv::VideoWriter sink(cl.outFile, codec, minimumFps, size);
        while (true) {
            if (mfps.frame()) break;
            cv::Mat frame; const bool more = source.read(frame);
            if (frame.empty()) {
                if (!more) break;
            } else {
                sink.write(rt.transform(frame));
            }
        }
    }
    return mfps.fps();
}


// Return true iff source.isOpened().
//
static bool canReadInFile(const CommandLine &cl, const VideoSource &source)
{
    if (source.isOpened()) return true;
    if (source.isFile()) {
        std::cerr << cl.program << ": Cannot read file: " << cl.inFile;
    } else {
        std::cerr << cl.program << ": Cannot use camera: " << cl.cameraId;
    }
    std::cerr << std::endl;
    return false;
}

/**
 * Launch rt.transform for the given RieszTransform and the given frame.
 */
cv::Mat do_transforms(RieszTransform* rt, cv::Mat frame)
{
    return rt->transform(frame);
}

cv::Rect find_crop(cv::Mat frame)
{
    cv::Mat hsvFrame;
    cv::Mat maskFrame;
    cv::cvtColor(frame, hsvFrame, CV_BGR2HSV);
    //cv::inRange(hsvFrame, cv::Scalar(17, 0, 0), cv::Scalar(21, 245, 245), maskFrame);    //color filter for baby blanket
    cv::inRange(hsvFrame, cv::Scalar(33, 100, 0), cv::Scalar(35, 245, 255), maskFrame);

    double largest_area = 0;
    int largest_contour = 0;
    std::vector<std::vector<cv::Point>> contours;
    findContours(maskFrame, contours, cv::noArray(), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    for(unsigned int i = 0; i < contours.size(); i++ ) {
        double area = cv::contourArea(contours[i], false);
        if(area > largest_area){
            largest_area = area;
            largest_contour = i;
        }
    }

    if (contours.empty()) {
        return cv::Rect(0, 0, frame.cols, frame.rows);
    } else {
        return cv::boundingRect(contours[largest_contour]);
    }
}

// Transform video in command-line or "batch" mode according to cl.
// Return 0 on success or 1 on failure.
//
static int batch(const CommandLine &cl)
{
    // Buffer of 3 frames for use with the DifferentialCollins algorithm.
    MotionDetection detector(cl);

    time_t start, end;
    time(&start);
    VideoSource source(cl.cameraId, cl.inFile);
    if (canReadInFile(cl, source) && canWriteOutFileHackRandomHack(cl)) {
        const int codec = source.fourCcCodec();
        cv::Rect cropWindow;
        double fps;
        if (cl.fps < 0) { // if user-sepcifies fps, use that
            fps = measureFpsHack(cl, source);
        } else {
            fps = cl.fps;
        }

        cv::Size size;

        // if cl.crop flag is true, reads the first frame to compute the crop window
        // the first frame is discarded after use
        if (cl.crop) {
            cv::Mat firstFrame;
            source.read(firstFrame);
            cropWindow = find_crop(firstFrame);
            size = cropWindow.size();
        } else {
            size = source.frameSize();
        }

        cv::VideoWriter sink(cl.outFile, codec, fps, size);

        // Create 3 RieszTransforms. One for each section.
        static const int SPLIT = 3;
        RieszTransform rt[SPLIT];
        WorkerThread<cv::Mat, RieszTransform*, cv::Mat> thread[SPLIT];

        for (int i = 0; i < SPLIT; i++) {
            cl.apply(rt[i]);
            rt[i].fps(fps);
        }

        for (;;) {
            // for each frame
            cv::Mat frame; const bool more = source.read(frame);
            if (frame.empty()) {
                if (!more) {
                    time(&end);
                    double diff_t = difftime(end, start);
                    printf("[info] time: %f\n", diff_t);
                    return 0;
                }
            } else {
                if (cl.crop) {
                    frame = frame(cropWindow);
                }

                // Split a single 640 x 480 frame into equal sections, 1 section
                // for each thread to process
                // Run each transform independently.
                std::future<cv::Mat> futures[SPLIT];
                cv::Mat in_sections[SPLIT];
                cv::Mat out_sections[SPLIT];

                for (int i = 0; i < SPLIT; i++) {
                    auto rowRange = cv::Range(frame.rows * i / SPLIT, (frame.rows * (i+1) / SPLIT));
                    auto colRange = cv::Range(0, frame.cols);
                    in_sections[i] = frame(rowRange, colRange);
                    futures[i] = thread[i].push(do_transforms, &rt[i], in_sections[i]);
                }

                // recombine results and output
                for (int i = 0; i < SPLIT; i++) {
                    out_sections[i] = futures[i].get();
                }
                cv::Mat result;
                cv::vconcat(out_sections, 3, result);
                sink.write(result);

                // Just ignore this return value for now.
                detector.isValidMotion(result);
            }
        }
    }
    return 1; // error condition
}


int main(int argc, char *argv[])
{
    QApplication qApplication(argc, argv);

    const CommandLine cl(argc, argv);
    if (cl.ok) {
        if (cl.gui) {
            MainDialog window(cl);
            if (window.ok()) {
                window.show();
                return qApplication.exec();
            }
        } else {
            printf("[info] starting batch processing.\n");
            if (cl.sourceCount && cl.sinkCount) return batch(cl);
            if (cl.help) return 0;
        }
    }
    return 1;
}

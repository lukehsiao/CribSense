#include "MainDialog.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"
#include <future>

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

// Transform video in command-line or "batch" mode according to cl.
// Return 0 on success or 1 on failure.
//
static int batch(const CommandLine &cl)
{
    // Buffer of 3 frames for use with the DifferentialCollins algorithm.
    cv::Mat frameBuffer[3];
    // frameBuffer[0] = Mat::zeros(640, 480, CV_8UC1);
    // frameBuffer[1] = Mat::zeros(640, 480, CV_8UC1);
    // frameBuffer[2] = Mat::zeros(640, 480, CV_8UC1);
    int frameCount = 0;

    // Parameters for the DifferentialCollins algorithm
    int erode = 2;
    cv::Mat erode_kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(erode,erode));
    int diff_threshold = 5;


    time_t start, end;
    time(&start);
    VideoSource source(cl.cameraId, cl.inFile);
    if (canReadInFile(cl, source) && canWriteOutFileHackRandomHack(cl)) {
        const int codec = source.fourCcCodec();
        double fps;
        if (cl.fps < 0) { // if user-sepcifies fps, use that
            fps = measureFpsHack(cl, source);
        } else {
            fps = cl.fps;
        }
        const cv::Size size = source.frameSize();
        cv::VideoWriter sink(cl.outFile, codec, fps, size);

        // Create 3 RieszTransforms. One for each section.
        RieszTransform rt_left, rt_mid, rt_right;
        cl.apply(rt_left); cl.apply(rt_mid); cl.apply(rt_right);
        rt_left.fps(fps); rt_mid.fps(fps); rt_right.fps(fps);
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
                if (frameCount < 3) {frameCount++;} // track when buffer is full
                // Split a single 640 x 480 frame into equal sections, 1 section
                // for each thread to process
                const cv::Mat left = frame(cv::Range(0, frame.rows), cv::Range(0, frame.cols / 3));
                const cv::Mat mid = frame(cv::Range(0, frame.rows), cv::Range(frame.cols / 3, frame.cols * 2 / 3));
                const cv::Mat right = frame(cv::Range(0, frame.rows), cv::Range(frame.cols * 2 / 3, frame.cols));

                // Run each transform independently.
                auto t_left = std::async(std::launch::async, do_transforms, &rt_left, left);
                auto t_mid = std::async(std::launch::async, do_transforms, &rt_mid, mid);
                auto t_right = std::async(std::launch::async, do_transforms, &rt_right, right);

                // recombine results and output
                cv::Mat result;
                cv::Mat sections[] = {
                  t_left.get(), t_mid.get(), t_right.get(),
                };
                cv::hconcat(sections, 3, result);
                // Update all the images in the ImageVector
                frameBuffer[0] = frameBuffer[1].clone();
                frameBuffer[1] = frameBuffer[2].clone();
                frameBuffer[2] = result.clone();

                sink.write(result);

                // Once all of the 3 frames are filled, check for motion;
                if (frameCount >= 3) {
                    // Perform DifferentialCollins Algorithm
                    cv::Mat h_d1;
                    cv::Mat h_d2;
                    cv::Mat evaluation;

                    // Convert to GrayScale
                    try {
                        // TODO: Is this the correct color space transformation?
                        // cv::cvtColor(frameBuffer[0], frameBuffer[0], CV_RGB2GRAY);
                        // cv::cvtColor(frameBuffer[1], frameBuffer[1], CV_RGB2GRAY);
                        // cv::cvtColor(frameBuffer[2], frameBuffer[2], CV_RGB2GRAY);

                        // Check differences
                        cv::absdiff(frameBuffer[0], frameBuffer[2], h_d1);
                        cv::absdiff(frameBuffer[1], frameBuffer[2], h_d2);
                        cv::bitwise_and(h_d1, h_d2, evaluation);

                        // threshold
                        cv::threshold(evaluation, evaluation, diff_threshold, 255, CV_THRESH_BINARY);

                        // erode
                        cv::erode(evaluation, evaluation, erode_kernel);
                    }
                    catch(cv::Exception &ex) {
                        printf("[error] OpenCV Exception %s", ex.what());
                    }
                    try {
                        cv::imshow( "Evaluation", evaluation );                  // Show our image inside it.
                        cv::waitKey(0);
                        cv::destroyAllWindows();
                    }
                    catch(cv::Exception &ex) {
                        printf("[error] OpenCV Exception in display %s", ex.what());
                        return 1;
                    }
                }
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

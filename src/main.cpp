#include "MainDialog.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"
#include <future>


// Return true iff can write cl.outFile to work around VideoWriter.
//
#include <fstream>
bool canWriteOutFileHackRandomHack(const CommandLine &cl)
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
double measureFpsHack(const CommandLine &cl, VideoSource &source)
{
    MeasureFps mfps(source.fps());
    if (source.isCamera()) {
        const double minimumFps = MeasureFps::minimumFps();
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
bool canReadInFile(const CommandLine &cl, const VideoSource &source)
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


cv::Mat do_transforms(RieszTransform* rt, cv::Mat frame)
{
    return rt->transform(frame);
}

// Transform video in command-line or "batch" mode according to cl.
// Return 0 on success or 1 on failure.
//
int batch(const CommandLine &cl)
{
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
        RieszTransform rt_l;
        cl.apply(rt_l);
        rt_l.fps(fps);
        RieszTransform rt_r;
        cl.apply(rt_r);
        rt_r.fps(fps);
        // TODO: Add rt_right
        for (;;) {
            cv::Mat frame; const bool more = source.read(frame);
            if (frame.empty()) {
                if (!more) return 0;
            } else {
                // Split a single 640 x 480 frame into equal sections, 1 section
                // for each thread to process
                const cv::Mat left = frame(cv::Range(0, frame.rows), cv::Range(0, frame.cols / 2));
                const cv::Mat right = frame(cv::Range(0, frame.rows), cv::Range(frame.cols / 2, frame.cols));

                // // Run both transforms on different threads.
                auto r_left = std::async(do_transforms, &rt_l, left);
                auto r_right = std::async(do_transforms, &rt_r, right);
                // // t_left.join();
                // // t_right.join();
                //
                // // recombine results and output
                cv::Mat result;
                cv::Mat new_left = r_left.get();
                cv::Mat new_right = r_right.get();
                cv::hconcat(new_left, new_right, result);
                sink.write(result);
            }
        }
    }
    return 1;
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
            if (cl.sourceCount && cl.sinkCount) return batch(cl);
            if (cl.help) return 0;
        }
    }
    return 1;
}

#include "VideoSource.hpp"
#include "MotionDetection.hpp"
#include <fstream>

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

// Transform video in command-line or "batch" mode according to cl.
// Return 0 on success or 1 on failure.
//
static int batch(const CommandLine &cl)
{
    // Buffer of 3 frames for use with the DifferentialCollins algorithm.
    MotionDetection detector(cl);

    time_t start, end;
    time(&start);
    VideoSource source(cl.cameraId, cl.inFile, cl.fps, cl.frameWidth, cl.frameHeight);
    if (canReadInFile(cl, source)) {
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
                detector.update(frame);
            }
        }
    }
    return 1; // error condition
}

int main(int argc, char *argv[])
{
    const CommandLine cl(argc, argv);
    if (cl.help || cl.about) return 0;
    if (cl.ok) {
        printf("[info] starting batch processing.\n");
        if (cl.sourceCount) return batch(cl);
    }
    return 1;
}

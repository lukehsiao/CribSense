#include "VideoSource.hpp"
#include "MotionDetection.hpp"
#include <fstream>
#include <inttypes.h>

#include <time.h>

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

static inline void print_time(const CommandLine& cl, uint64_t& time, char c) {
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    uint64_t new_time = (uint64_t)((uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000);

    if (time == 0) {
        fprintf(stderr, "%c Start time: %" PRIu64 "\n", c, new_time);
        time = new_time;
    } else {
	fprintf(stderr, "%c Delta: %" PRIu64 "\n", c, new_time - time);
	time = new_time;
    }
}

// Transform video in command-line or "batch" mode according to cl.
// Return 0 on success or 1 on failure.
//
static int batch(const CommandLine &cl)
{
    // Buffer of 3 frames for use with the DifferentialCollins algorithm.
    MotionDetection detector(cl);

    uint64_t frame_time = 0;
    VideoSource source(cl.cameraId, cl.inFile, cl.fps, cl.frameWidth, cl.frameHeight);
    if (canReadInFile(cl, source)) {
        print_time(cl, frame_time, 'A');
        for (;;) {
            // for each frame
            cv::Mat frame; const bool more = source.read(frame);
            print_time(cl, frame_time, 'A');
            if (frame.empty()) {
                print_time(cl, frame_time, 'B');
                if (!more) {
                    //time(&end);
                    //double diff_t = difftime(end, start);
                    //printf("[info] time: %f\n", diff_t);
                    return 0;
                }
            } else {
                detector.update(frame);
                print_time(cl, frame_time, 'B');
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

#ifndef COMMAND_LINE_H_INCLUDED
#define COMMAND_LINE_H_INCLUDED

#include <string>
#include <sys/time.h>

class MainDialog;
class RieszTransform;


// Determine a useful frame rate for the application.  Isolate the frame
// rate policy and FPS-measuring code here.  That policy is:
//
// Never report a frame rate less than minimumFps().
//
// Always process frames as fast as possible unless there is a GUI.  Write
// the source's reported frame rate to the output file if it is greater
// than minimumFps().  Otherwise use the measured frame rate for output.
//
// If running with the GUI:
//    And input is a camera (reported source FPS is less than minimumFps()):
//        Process frames as fast as possible.
//        Report the measured frame rate in the GUI and in output file.
//    And input is a file (reported source FPS > minimumFps()):
//        Measure the frame rate as if from a camera.
//        If the measured rate exceeds the rate reported by the input file,
//            slow processing down to the reported rate using a timer.
//        If the measured rate is slower than what the input file reports,
//            keep processing frames as fast as possible and write the
//            measured frame rate to the output.
//
// Measure the frame rate over the first 100 frames, then update the values
// returned by fps() and timerIntervalMsPerFrame() at most once.
//
struct MeasureFps {

    const double itsSourceFps;
    struct timeval itsStart;
    unsigned itsCount;
    double itsFps;
    int itsTimerInterval;

    // A minimum frame rate because filters need a sampling rate > 0.
    //
    static double minimumFps() { return 2.0; }

    // Return minimumFps() or the measured frame rate.
    //
    double measureFps()
    {
        struct timeval now;
        const int fail = gettimeofday(&now, 0);
        double result = minimumFps();
        if (!fail) {
            static const int million = 1000 * 1000;
            struct timeval elapsed;
            elapsed.tv_sec  = now.tv_sec  - itsStart.tv_sec;
            elapsed.tv_usec = now.tv_usec - itsStart.tv_usec;
            const double microseconds
                = million * elapsed.tv_sec + elapsed.tv_usec;
            const double measured = million * (itsCount / microseconds);
            if (measured > result) result = measured;
        }
        return result;
    }

    // The frame rate available now.  Update when frame() returns true.
    //
    double fps() const { return itsFps; }

    // Return 0 or the time interval per frame in milliseconds for QTimer.
    //
    int timerIntervalMsPerFrame() const { return itsTimerInterval; }

    // Count a frame.  Return true and update timerIntervalMsPerFrame() and
    // fps() on the 100th frame.  Otherwise return false and count one
    // frame.  Call this for at least 100 frames.
    //
    // Avoid expense when this returns false because frame() is likely
    // called for every video frame.
    //
    bool frame()
    {
        static unsigned lastFrame = 100;
        const bool result = itsCount == lastFrame;
        if (result) {
            itsFps = measureFps();
            if (itsFps > itsSourceFps && itsSourceFps > minimumFps()) {
                itsFps = itsSourceFps;
                itsTimerInterval = 1000.0 / itsFps;
            }
        } else if (itsCount == 0) {
            const int fail = gettimeofday(&itsStart, 0);
            if (fail) itsCount = lastFrame;
        }
        ++itsCount;
        return result;
    }

    // Initialize this with the reported source frame rate fps.
    //
    MeasureFps(double fps)
        : itsSourceFps(fps)
        , itsStart()
        , itsCount(0)
        , itsFps(minimumFps() > itsSourceFps ? minimumFps() : itsSourceFps)
        , itsTimerInterval(0)
    {}
};


// A command line for this program.
//
struct CommandLine {

    const std::string av0;           // The zeroth command line argument.
    std::string program;             // The name of this program in messages.
    std::string inFile;              // The video input file or "".
    std::string outFile;             // The output file or "".
    int cameraId;                    // The camera if not negative.
    int sourceCount;                 // Count of video sources specified.
    int sinkCount;                   // Count of video sinks specified.
    double amplify;                  // The current amplification.
    double lowCutoff;                // The low frequency of the bandpass.
    double highCutoff;               // The high frequency of the bandpass.
    double threshold;                // The phase threshold as % of pi.
    bool repeat;                     // True for looping input file.
    bool gui;                        // True for the GUI controls.
    bool about;                      // True iff --about.
    bool help;                       // True iff --help.
    bool ok;                         // True if (ac, av) parse is valid.

    // Return a static string about this program.
    //
    static const char *acknowlegements();

    // Show a usage message for this program named av0 on stream os.
    //
    static void showUsage(const std::string &av0, std::ostream &os);

    // Write cl to os such that it can be invoked from a shell.
    //
    friend std::ostream &operator<<(std::ostream &os, const CommandLine &cl);

    // Apply the settings here to rt or md.
    //
    void apply(RieszTransform &rt) const;
    void apply(MainDialog &md) const;

    // Parse command line options from ac and av as passed to main().
    //
    CommandLine(int ac, char *av[]);

    // Reconstruct command line options from current settings at md.
    //
    CommandLine(const MainDialog *md);
};

#endif // #ifndef COMMAND_LINE_H_INCLUDED

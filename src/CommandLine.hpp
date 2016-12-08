/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

#ifndef COMMAND_LINE_H_INCLUDED
#define COMMAND_LINE_H_INCLUDED

#include <string>
#include <sys/time.h>
#include "INIReader.h"

class RieszTransform;

// A command line for this program.
//
struct CommandLine {

    const std::string av0;           // The zeroth command line argument.
    std::string program;             // The name of this program in messages.
    std::string inFile;              // The video input file or "".
    int cameraId;                    // The camera if not negative.
    int sourceCount;                 // Count of video sources specified.
    int erodeDimension;              // Dimention of the erode kernel.
    int dilateDimension;             // Dimention of the dilate kernel.
    int diffThreshold;               // Difference threshold before marking
                                     //     pixel as changed.
    int motionDuration;              // # of frames motion must be detected.
    int pixelThreshold;              // # of pixels that must be different
                                     //   to be flagged as motion.
    unsigned timeToAlarm;            // # of seconds to wait before sounding alarm.
    unsigned framesToSettle;         // # frames to ignore on startup and reset
    unsigned roiUpdateInterval;      // # frames between roi updates
    unsigned roiWindow;              // # frames to consider when calculating roi
    double amplify;                  // The current amplification.
    double input_fps;                // fps to read from the input
    double full_fps;                 // fps at which full frames can be processed
    double crop_fps;                 // fps at which cropped frames can be processed
    double lowCutoff;                // The low frequency of the bandpass.
    double highCutoff;               // The high frequency of the bandpass.
    double threshold;                // The phase threshold as % of pi.
    bool showDiff;                   // optionally show the diff between frames
    bool showMagnification;          // optionally show the magnification output
    bool showTimes;                  // optionally print the times between frames
    bool about;                      // True iff --about.
    bool help;                       // True iff --help.
    bool ok;                         // True if (ac, av) parse is valid.
    bool crop;                       // True if adaptive crop is enabled.
    int frameWidth;
    int frameHeight;

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

    // Parse command line options from ac and av as passed to main().
    //
    CommandLine(int ac, char *av[]);
};

#endif // #ifndef COMMAND_LINE_H_INCLUDED

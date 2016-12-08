/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

#include "CommandLine.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"

#include <sys/time.h>

const char *CommandLine::acknowlegements()
{
    static const char result[] =
        (" is a real-time implementation of the methods"
         " described in the paper"
         " \"Eulerian Video Magnification for Revealing Subtle Changes"
         " in the World\""
         " written by Hao-yu Wu, Michael Rubinstein, Eugene Shih,"
         " John Guttag, Fredo Durand, and William T. Freeman "
         " and published in the"
         " <i>ACM Transactions for Graphics</i>,"
         " Volume 31, Number 4, July 2012."
         "<br/>"
         "<br/>"
         "Copyright 2012 Massachusetts Institute of Technology"
         " and Quanta Research Cambridge. All Rights Reserved."
         "<br/>"
         "See: http://people.csail.mit.edu/mrub/evm/code/LICENSE.pdf"
         "<br/>"
         "Read: http://people.csail.mit.edu/nwadhwa/riesz-pyramid/RieszPyr.pdf"
         "Contact Neal Wadhwa at MIT dot EDU for the latest improvements."
         "http://people.csail.mit.edu/nwadhwa/");
    return result;
}

void CommandLine::showUsage(const std::string &program, std::ostream &os)
{
    os << std::endl << program << ": Amplify motion in a video." << std::endl
       << std::endl
       << "Usage: " << program << " [--config] <path>" << std::endl
       << std::endl
       << "Where: " << "--config specifies the path to the config INI." << std::endl
       << "Example: " << program << " --config config.ini"
       << std::endl << std::endl;
}


std::ostream &operator<<(std::ostream &os, const CommandLine &cl)
{
    os << cl.av0;
    if (cl.inFile.empty()) {
        if (cl.cameraId >= 0) os << " --camera " << cl.cameraId;
    } else {
        os << " --input " << cl.inFile;
    }
    if (cl.amplify    >= 0)  os << " --amplify "     << cl.amplify;
    if (cl.lowCutoff  >  0)  os << " --low-cutoff "  << cl.lowCutoff;
    if (cl.highCutoff >  0)  os << " --high-cutoff " << cl.highCutoff;
    if (cl.threshold  >= 0)  os << " --threshold "   << cl.threshold;
    return os;
}


// Always set highCutoff before lowCutoff.
//
void CommandLine::apply(RieszTransform &rt) const
{
    if (amplify    >= 0) rt.alpha(amplify);
    if (highCutoff >  0) rt.highCutoff(highCutoff);
    if (lowCutoff  >  0) rt.lowCutoff(lowCutoff);
    if (threshold  >= 0) rt.threshold(threshold);
}

// Defaults for transform settings should be OK for the minimum frame rate.
//
CommandLine::CommandLine(int ac, char *av[])
    : av0(av[0])
    , program()
    , inFile()
    , cameraId(-1)
    , sourceCount(0)
    , erodeDimension(2)
    , dilateDimension(60)
    , diffThreshold(5)
    , motionDuration(1)
    , pixelThreshold(10)
    , amplify(30.0)
    , lowCutoff(0.5)
    , highCutoff(1.0)
    , threshold(25.0)
    , showDiff(false)
    , showMagnification(false)
    , showTimes(false)
    , about(false)
    , help(false)
    , ok(true)           // Check this before use.
    , crop(false)
    , frameWidth(640)
    , frameHeight(480)
{
    const std::string::size_type n = av0.rfind("/");
    program
        = (n == std::string::npos || n == av0.size() - 1)
        ? av0
        : av0.substr(n + 1);

    bool use_config_file = false;
    std::string config_path = "";

    for (int i = 1; ok && i < ac; ++i) {
        std::stringstream raw(av[i]); std::string arg; raw >> arg;
        if ("--about" == arg) {
            about = true;
        } else if ("--help" == arg) {
            help = true;
            showUsage(program, std::cerr);
            break;
        } else if ("--config" == arg && (ok = ++i < ac)) {
            config_path = av[i];
            use_config_file = true;
            ok = true;
            break;
        } else {
            std::cerr << std::endl << program << ": Bad option '" << arg << "'"
                      << std::endl;
            ok = false;
        }
    }

    // Of user specifies to use INI file on commandline, load these arguments
    // from the file rather than the commandline.
    if (use_config_file) {
        sourceCount = 0;
        static INIReader reader(config_path);

        if (reader.ParseError() < 0) {
            printf("[error] Cannot load %s\n", config_path.c_str());
            ok = false;
            return;
        }

        // check sources
        std::string input_path = reader.Get("io", "input", "");
        if (input_path != "") {
            inFile = input_path;
            ok = ok && sourceCount == 0;
            ++sourceCount;
        }

        int input_cameraID = reader.GetInteger("io", "camera", -1);
        if (input_cameraID != -1) {
            cameraId = input_cameraID;
            ok = ok && cameraId >= 0 && sourceCount == 0;
            ++sourceCount;
        }

        // Validate that input is EITHER camera or file.
        if (sourceCount > 1) {
            std::cerr << program << ": Specify only one of --camera or --input."
                      << std::endl << std::endl;
            ok = false;
            return;
        }
        if (!(sourceCount)) if (!(about || help)) {
            std::cerr << program << ": Specify at least 1 input."
                      << std::endl << std::endl;
        }

        amplify = reader.GetReal("magnification", "amplify", 20);
        ok = ok && amplify && amplify >= 0 && amplify <= 100;

        input_fps = reader.GetReal("io", "input_fps", 15);
        if (input_cameraID != -1) {
            // NoIR Camera can only handle 40fps max while retaining good
            // performance in low-light.
            ok = ok && input_fps && input_fps >= 0 && input_fps <= 40;
        }
        else {
            ok = ok && input_fps && input_fps >= 0;
        }

        full_fps = reader.GetReal("io", "full_fps", 4.5);
        ok = ok && full_fps && full_fps >= 0;

        crop_fps = reader.GetReal("io", "crop_fps", 15);
        ok = ok && crop_fps && crop_fps >= 0;

        lowCutoff = reader.GetReal("magnification", "low-cutoff", 0.7);
        ok = ok && lowCutoff && lowCutoff >= 0;

        highCutoff = reader.GetReal("magnification", "high-cutoff", 1);
        ok = ok && highCutoff && highCutoff >= 0;

        threshold = reader.GetReal("magnification", "threshold", 50);
        ok = ok && threshold && threshold >= 0 && threshold <= 100;

        frameWidth = reader.GetInteger("io", "width", 640);
        ok = ok && frameWidth >= 320 && frameWidth <= 1920;
        frameHeight = reader.GetInteger("io", "height", 480);
        ok = ok && frameHeight >= 240 && frameHeight <= 1080;


        erodeDimension = reader.GetInteger("motion", "erode_dim", 3);
        ok = ok && erodeDimension && erodeDimension > 0;

        dilateDimension = reader.GetInteger("motion", "dilate_dim", 60);
        ok = ok && dilateDimension && dilateDimension > 0;

        diffThreshold = reader.GetInteger("motion", "diff_threshold", 10);
        ok = ok && diffThreshold && diffThreshold >= 0;

        motionDuration = reader.GetInteger("motion", "duration", 1);
        ok = ok && motionDuration && motionDuration >= 1;

        pixelThreshold = reader.GetInteger("motion", "pixel_threshold", 5);
        ok = ok && pixelThreshold && pixelThreshold >= 1;

        showDiff = reader.GetBoolean("motion", "show_diff", false);

        showMagnification = reader.GetBoolean("magnification", "show_magnification", false);

        showTimes = reader.GetBoolean("debug", "print_times", false);

        crop = reader.GetBoolean("cropping", "crop", false);

        framesToSettle = reader.GetInteger("cropping", "frames_to_settle", 10);
        ok = ok && framesToSettle && framesToSettle >= 1;

        timeToAlarm = reader.GetInteger("io", "time_to_alarm", 10);
        ok = ok && timeToAlarm && timeToAlarm > 1;

        roiWindow = reader.GetInteger("cropping", "roi_window", 10);
        ok = ok && roiWindow && roiWindow >= 1;

        roiUpdateInterval = reader.GetInteger("cropping", "roi_update_interval", 100);
        ok = ok && roiUpdateInterval && roiUpdateInterval >= roiWindow;


        about = reader.GetBoolean("io", "about", false);
        help = reader.GetBoolean("io", "help", false);
    }

    if (about) std::cout << program << acknowlegements() << std::endl;
    if (!ok) showUsage(program, std::cerr);
}

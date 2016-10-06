#include "CommandLine.hpp"
#include "MainDialog.hpp"
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

static std::string percentageOption(const char *o, const std::string &program)
{
    std::stringstream oss;
    if (program != "") oss << std::endl << program << ": ";
    oss << o << " takes a percentage 0 to 100." << std::endl;
    return oss.str();
}
static std::string amplifyOption(const std::string &program = "")
{
    return percentageOption("--amplify", program);
}
static std::string thresholdOption(const std::string &program = "")
{
    return percentageOption("--threshold", program);
}


void CommandLine::showUsage(const std::string &program, std::ostream &os)
{
    os << std::endl << program << ": Amplify motion in a video." << std::endl
       << std::endl
       << "Usage: " << program << " [--about] [--gui] [--repeat] [<source>] "
       << "[--output <file>] [--amplify %] [--threshold %]"
       << "[--low-cutoff <Hz>] [--high-cutoff <Hz>]" << std::endl
       << std::endl
       << "Where: " << "--about shows acknowledgements." << std::endl
       << "       " << "--gui opens the GUI controls window." << std::endl
       << "       " << "--repeat loops an input video file." << std::endl
       << "       " << "--output sends processed video to <file>." << std::endl
       << "       " << amplifyOption()
       << "       " << thresholdOption()
       << "       " << "--low-cutoff is frequency in <Hz>." << std::endl
       << "       " << "--high-cutoff is frequency in <Hz>." << std::endl
       << "  and  " << std::endl
       << "       " << "<source> is either '--camera <id>' or '--input <file>'"
       << std::endl
       << "       " << "         where <id> is an integer camera ID"
       << std::endl
       << "       " << "         and <file> is a video file to process."
       << std::endl << std::endl
       << "Example: " << program << " --gui --input in.avi --output out.avi "
       << "         " << "--amplify 18 --low-cutoff 1.3 --high-cutoff 2.7"
       << std::endl << std::endl;
}


std::ostream &operator<<(std::ostream &os, const CommandLine &cl)
{
    os << cl.av0;
    if (cl.gui) os << " --gui";
    if (cl.repeat) os << " --repeat";
    if (cl.inFile.empty()) {
        if (cl.cameraId >= 0) os << " --camera " << cl.cameraId;
    } else {
        os << " --input " << cl.inFile;
    }
    if (!cl.outFile.empty()) os << " --output "      << cl.outFile;
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


// Check repeat only if there is already an output file specified.
//
void CommandLine::apply(MainDialog &md) const
{
    static const double tick = md.itsLowCutoff.slider.tickInterval();
    assert(ok);
    assert(md.itsTransform);
    apply(*md.itsTransform);
    md.changeAmplification(amplify);
    md.changeHighCutoff(highCutoff * tick);
    md.changeLowCutoff(lowCutoff * tick);
    md.changeThreshold(threshold);
    if (!outFile.empty()) {
        if (md.itsRepeat) md.itsRepeat->setChecked(repeat);
        const int codec = md.itsSource->fourCcCodec();
        const cv::Size size = md.itsSource->frameSize();
        MeasureFps mFps(md.itsSource->fps());
        md.itsOutput.saveTo(codec, mFps.fps(), size, outFile);
        md.itsOutput.itsSave.setAutoDefault(false);
    }
}


// Defaults for transform settings should be OK for the minimum frame rate.
//
CommandLine::CommandLine(int ac, char *av[])
    : av0(av[0])
    , program()
    , inFile()
    , outFile()
    , cameraId(-1)
    , sourceCount(0)
    , sinkCount(0)
    , amplify(30.0)
    , lowCutoff( MeasureFps::minimumFps() / 4.0)
    , highCutoff(MeasureFps::minimumFps() / 2.0)
    , threshold(25.0)
    , repeat(false)
    , gui(false)
    , about(false)
    , help(false)
    , ok(true)                          // Check this before use.
{
    const std::string::size_type n = av0.rfind("/");
    program
        = (n == std::string::npos || n == av0.size() - 1)
        ? av0
        : av0.substr(n + 1);
    for (int i = 1; ok && i < ac; ++i) {
        std::stringstream raw(av[i]); std::string arg; raw >> arg;
        if ("--about" == arg) {
            about = true;
        } else if ("--camera" == arg && (ok = ++i < ac)) {
            std::stringstream ss(av[i]);
            ok = (ss >> cameraId) && cameraId >= 0 && sourceCount == 0;
            ++sourceCount;
        } else if ("--input" == arg && (ok = ++i < ac)) {
            inFile = av[i];
            ok = sourceCount == 0;
            ++sourceCount;
        } else if ("--output" == arg && (ok = ++i < ac)) {
            outFile = av[i];
            ok = sinkCount == 0;
            ++sinkCount;
        } else if ("--amplify" == arg && (ok = ++i < ac)) {
            std::stringstream ss(av[i]);
            ok = (ss >> amplify) && amplify >= 0 && amplify <= 100;
            if (!ok) std::cerr << amplifyOption(program);
        } else if ("--low-cutoff" == arg && (ok = ++i < ac)) {
            std::stringstream ss(av[i]);
            ok = (ss >> lowCutoff) && lowCutoff >= 0;
        } else if ("--high-cutoff" == arg && (ok = ++i < ac)) {
            std::stringstream ss(av[i]);
            ok = (ss >> highCutoff) && highCutoff >= 0;
        } else if ("--threshold" == arg && (ok = ++i < ac)) {
            std::stringstream ss(av[i]);
            ok = (ss >> threshold) && threshold >= 0 && threshold <= 100;
            if (!ok) std::cerr << thresholdOption(program);
        } else if ("--repeat" == arg) {
            repeat = true;
        } else if ("--gui" == arg) {
            gui = true;
        } else if ("--help" == arg) {
            help = true;
            showUsage(program, std::cerr);
            break;
        } else {
            std::cerr << std::endl << program << ": Bad option '" << arg << "'"
                      << std::endl;
            ok = false;
        }
    }
    if (sourceCount > 1) {
        std::cerr << program << ": Specify only one of --camera or --input."
                  << std::endl << std::endl;
    }
    if (sinkCount > 1) {
        std::cerr << program << ": Specify only one --output file."
                  << std::endl << std::endl;
    }
    if (!(sourceCount && sinkCount)) if (!(about || help)) gui = true;
    if (about && !gui) std::cout << program << acknowlegements() << std::endl;
    if (!ok) showUsage(program, std::cerr);
}


CommandLine::CommandLine(const MainDialog *md)
    : av0(md->itsCl.av0)
    , program(md->itsCl.program)
    , inFile()
    , outFile()
    , cameraId(-1)
    , sourceCount(0)
    , sinkCount(0)
    , amplify(-1.0)
    , lowCutoff(-1.0)
    , highCutoff(-1.0)
    , threshold(-1.0)
    , repeat(false)
    , gui(true)
    , about(false)
    , help(false)
    , ok(false)                         // Never check this.
{
    repeat = md->itsRepeat && md->itsRepeat->isChecked();
    if (md->itsSource) {
        if (md->itsSource->isFile()) {
            inFile = md->itsSource->fileName();
        } else {
            cameraId = md->itsSource->cameraId();
        }
    }
    if (!md->itsOutput.itsFile.empty()) outFile = md->itsOutput.itsFile;
    amplify    = md->itsAmplify.number.value();
    lowCutoff  = md->itsLowCutoff.number.value();
    highCutoff = md->itsHighCutoff.number.value();
    threshold  = md->itsThreshold.number.value();
}

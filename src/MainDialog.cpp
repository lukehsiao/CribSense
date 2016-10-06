#include "MainDialog.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"


// If md selects a camera, write its ID into cameraId and return true.
// Otherwise return false.
//
static bool selectCamera(MainDialog *md, QString &cameraId, const char *file)
{
    QStringList items;
    for (int i = 0; true; ++i) {
        cv::VideoCapture vc(i);
        if (!vc.isOpened()) break;
        QString c("Camera ");
        c.append(QString::number(i));
        items.append(c);
    }
    items.append(file);
    bool ok = false;
    const QString text = QInputDialog::getItem(md, "Select Video", "Source",
                                               items, 0, false, &ok);
    if (ok) cameraId = text.isEmpty() ? "" : text;
    return ok;
}


// Return "" or the name of a video file to use as input.
//
static std::string chooseInputFile(MainDialog *md)
{
    static const char select[] = "Select Video File";
    static const char videos[] = "Video Files (*.avi *.mp4)";
    const QString qFile = QFileDialog::getOpenFileName(md, select, "", videos);
    const std::string result = qFile.isNull() ? "" : qFile.toStdString();
    return result;
}


// Return 0 or pointer to a new VideoSource for md.
// Caller must eventually delete the result.
//
static VideoSource *newVideoSource(MainDialog *md, const CommandLine &cl)
{
    if (cl.sourceCount == 1) {
        return new VideoSource(cl.cameraId, cl.inFile);
    } else {
        static const char chooseFile[] = "Choose a video file";
        QString returnedText;
        if (selectCamera(md, returnedText, chooseFile)) {
            int cameraId = -1;
            std::string filename = "";
            if (returnedText == chooseFile) {
                filename = chooseInputFile(md);
                if (filename.empty()) return 0;
            } else {
                bool okIgnored;
                cameraId = returnedText.toInt(&okIgnored);
            }
            return new VideoSource(cameraId, filename);
        }
    }
    return 0;
}


// Choose a file in which to save output.
//
void MainDialog::chooseOutFile()
{
    static const char c[] = "Save Output to File";
    static const char v[] = "Video Files (*.avi)";
    const QString qFile = QFileDialog::getSaveFileName(this, c, "", v);
    if (!(qFile.isNull() || qFile.isEmpty())) {
        const std::string file = qFile.toStdString();
        const int codec = itsSource->fourCcCodec();
        const cv::Size size = itsSource->frameSize();
        itsOutput.saveTo(codec, itsFps.fps(), size, file);
        itsOutput.itsSave.setDefault(false);
    }
}


// Return frame size from cd or zero.
//
static cv::Size cameraSize(VideoSource *vs)
{
    static const cv::Size zero;
    return vs ? vs->frameSize() : zero;
}


// Return a label that reads pi/100 but pi is in G(r)eek.
//
static QString piPercent()
{
    const QChar pi(0xc0, 0x03);
    QString result(pi);
    result.append("/100");
    return result;
}


// Dump the final command line to stdout on exit.
//
MainDialog::~MainDialog()
{
    const CommandLine cl(this);
    std::cout << std::endl << cl << std::endl << std::endl;
    delete itsBar;
    delete itsRepeat;
    delete itsTransform;
    delete itsSource;
}


MainDialog::MainDialog(const CommandLine &cl)
    : QDialog()
    , itsCl(cl)
    , itsTimer()
    , itsSource(newVideoSource(this, this->itsCl))
    , itsTransform(new RieszTransform())
    , itsInputWidget(cameraSize(itsSource), this)
    , itsOutputWidget(cameraSize(itsSource), this)
    , itsInput( "Original",  itsInputWidget, "frames/second")
    , itsOutput("Processed", itsOutputWidget)
    , itsAmplify(   "Amplification",   "")
    , itsLowCutoff( "Low cutoff",      "Hz")
    , itsHighCutoff("High cutoff",     "Hz")
    , itsThreshold( "Phase Threshold", piPercent())
    , itsButtons(cl.program)
    , itsRepeat(0)
    , itsBar(0)
    , itsGrid()
    , itsFps(itsSource ? itsSource->fps() : 0.0)
{
    setWindowTitle(itsCl.program.c_str());
    if (itsSource && itsTransform) {
        itsTransform->fps(itsFps.fps());
        if (itsSource->isFile()) {
            itsRepeat = new QCheckBox("Repeat", this);
            itsRepeat->setChecked(true);
            itsBar = new QProgressBar(this);
            itsBar->setMaximum(itsSource->frameCount());
        }
        layoutGrid();
        itsCl.apply(*this);
        connectStuff();
        itsTimer.start(itsFps.timerIntervalMsPerFrame());
    }
}


// Reset the transform, input, and output video streams.
//
void MainDialog::resetIo()
{
    const int codec = itsSource->fourCcCodec();
    const cv::Size size = itsSource->frameSize();
    const std::string file = itsOutput.itsFile;
    if (itsOutput.itsSink) itsOutput.saveTo(codec, itsFps.fps(), size, file);
    VideoSource *const vs = new VideoSource(*itsSource);
    delete itsSource; itsSource = vs;
    RieszTransform *const rt = new RieszTransform(*itsTransform);
    delete itsTransform; itsTransform = rt;
}


// Handle the empty frame marking the end of a video stream.
//
bool MainDialog::doEmptyFrame(bool more)
{
    const bool repeat = itsRepeat && itsRepeat->isChecked();
    if (more || repeat) {
        resetIo();
        if (itsBar) itsBar->setValue(0);
    } else {
        itsTimer.stop();
        itsButtons.quit.setDefault(true);
        return false;
    }
    return true;
}


// Process one frame of video.
//
bool MainDialog::doOneFrame()
{
    if (itsFps.frame()) {
        itsTimer.setInterval(itsFps.timerIntervalMsPerFrame());
        itsTransform->fps(itsFps.fps());
        itsInput.number.setSegmentStyle(QLCDNumber::Flat);
        rangeSlidersForFps(itsFps.fps());
    }
    cv::Mat inFrame;
    bool more = itsSource->read(inFrame);
    if (inFrame.empty()) {
        more = doEmptyFrame(more);
    } else {
        const cv::Mat outFrame = itsTransform->transform(inFrame);
        itsInputWidget.show(inFrame);
        itsOutputWidget.show(outFrame);
        if (itsOutput.itsSink) itsOutput.itsSink->write(outFrame);
    }
    if (itsBar) itsBar->setValue(1 + itsBar->value());
    return more;
}


void MainDialog::changeAmplification(int value)
{
    itsTransform->alpha(value);
}
void MainDialog::changeThreshold(int value)
{
    itsTransform->threshold(value);
}

// Clamp the low cutoff control at the high-cutoff value and vice versa.
//
void MainDialog::changeLowCutoff(int value)
{
    static const double tick = itsLowCutoff.slider.tickInterval();
    const int highCutoff = itsHighCutoff.slider.value();
    if (value > highCutoff) {
        itsLowCutoff.slider.setValue(highCutoff);
    } else {
        const double newValue = value / tick;
        itsTransform->lowCutoff(newValue);
        itsLowCutoff.number.display(newValue);
        itsLowCutoff.slider.setValue(value);
    }
}
void MainDialog::changeHighCutoff(int value)
{
    static const double tick = itsHighCutoff.slider.tickInterval();
    const int lowCutoff = itsLowCutoff.slider.value();
    if (value < lowCutoff) {
        itsHighCutoff.slider.setValue(lowCutoff);
    } else {
        const double newValue = value / tick;
        itsTransform->highCutoff(newValue);
        itsHighCutoff.number.display(newValue);
        itsHighCutoff.slider.setValue(value);
    }
}


// Set frequency ranges for the frame sampling rate (FPS).
//
void MainDialog::rangeSlidersForFps(double fps)
{
    static const int tick = itsLowCutoff.slider.tickInterval();
    const double rangeTop = fps * tick / 2.0;
    const int iRangeTop = rangeTop;
    itsLowCutoff .slider.setRange(1, iRangeTop);
    itsHighCutoff.slider.setRange(1, iRangeTop);
    itsInput.number.display(QString::number(fps, 'f', 2));
}


// Connect and synchronize the UI and transform settings.
//
void MainDialog::connectStuff()
{
    rangeSlidersForFps(itsFps.fps());
    itsAmplify.slider.setRange(0, 100);
    itsAmplify.slider.setValue(itsCl.amplify);
    itsAmplify.number.display(itsCl.amplify);
    itsThreshold.slider.setRange(0, 100);
    itsThreshold.slider.setValue(itsCl.threshold);
    itsThreshold.number.display(itsCl.threshold);
    connect(&itsAmplify.slider, SIGNAL(valueChanged(int)),
            &itsAmplify.number, SLOT(display(int)));
    connect(&itsThreshold.slider, SIGNAL(valueChanged(int)),
            &itsThreshold.number, SLOT(display(int)));
    connect(&itsAmplify.slider, SIGNAL(valueChanged(int)),
            this, SLOT(changeAmplification(int)));
    connect(&itsLowCutoff.slider, SIGNAL(valueChanged(int)),
            this, SLOT(changeLowCutoff(int)));
    connect(&itsHighCutoff.slider, SIGNAL(valueChanged(int)),
            this, SLOT(changeHighCutoff(int)));
    connect(&itsThreshold.slider, SIGNAL(valueChanged(int)),
            this, SLOT(changeThreshold(int)));
    connect(&itsOutput.itsSave, SIGNAL(clicked()),
            this, SLOT(chooseOutFile()));
    connect(&itsTimer, SIGNAL(timeout()), this, SLOT(doOneFrame()));
    itsButtons.about.setAutoDefault(false);
    itsButtons.quit.setAutoDefault(false);
}


void MainDialogButtons::doAbout()
{
    static const char *name = program.c_str();
    QString about("About ");
    QString acknowlegements(name);
    about.append(name);
    acknowlegements.append(CommandLine::acknowlegements());
    QMessageBox::information(parentWidget(), about, acknowlegements);
}
void MainDialogButtons::doQuit()
{
    QApplication::instance()->quit();
}

// Show acknowledgements box if requested.
//
void MainDialog::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (itsCl.about) itsButtons.doAbout();
}


// Layout widgets in a grid optionally including a repeat checkbox and
// progress bar.
//
void MainDialog::layoutGrid()
{
    //                                          row  col rspan cspan
    itsGrid.setSpacing(10);                //   ---  ---  --   ---
    itsGrid.addWidget(&itsInput.itsTitle       ,  0 ,  0          );
    itsGrid.addLayout(itsInput.box             ,  0 ,  0 , 1 ,  6 );
    itsGrid.addWidget(&itsInput.itsDisplay     ,  1 ,  0 , 4 ,  6 );
    itsGrid.addWidget(&itsOutput.itsTitle      ,  0 ,  8          );
    itsGrid.addWidget(&itsOutput.itsSave       ,  0 , 13          );
    itsGrid.addWidget(&itsOutput.itsDisplay    ,  1 ,  8 , 4 ,  6 );
    itsGrid.addWidget(&itsAmplify.title        ,  5 ,  0          );
    itsGrid.addWidget(&itsAmplify.slider       ,  5 ,  1 , 1 , 11 );
    itsGrid.addWidget(&itsAmplify.number       ,  5 , 12          );
    itsGrid.addWidget(&itsLowCutoff.title      ,  7 ,  0          );
    itsGrid.addWidget(&itsLowCutoff.slider     ,  7 ,  1 , 1 , 11 );
    itsGrid.addWidget(&itsLowCutoff.number     ,  7 , 12          );
    itsGrid.addWidget(&itsLowCutoff.units      ,  7 , 13          );
    itsGrid.addWidget(&itsHighCutoff.title     ,  8 ,  0          );
    itsGrid.addWidget(&itsHighCutoff.slider    ,  8 ,  1 , 1 , 11 );
    itsGrid.addWidget(&itsHighCutoff.number    ,  8 , 12          );
    itsGrid.addWidget(&itsHighCutoff.units     ,  8 , 13          );
    itsGrid.addWidget(&itsThreshold.title      ,  9 ,  0          );
    itsGrid.addWidget(&itsThreshold.slider     ,  9 ,  1 , 1 , 11 );
    itsGrid.addWidget(&itsThreshold.number     ,  9 , 12          );
    itsGrid.addWidget(&itsThreshold.units      ,  9 , 13          );
    if (itsBar)    itsGrid.addWidget(itsBar    , 10 ,  1 , 1 , 11 );
    if (itsRepeat) itsGrid.addWidget(itsRepeat , 10 ,  0          );
    itsGrid.addWidget(&itsButtons.about        , 10 , 12          );
    itsGrid.addWidget(&itsButtons.quit         , 10 , 13          );
    setLayout(&itsGrid);
}

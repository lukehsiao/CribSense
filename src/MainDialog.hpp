#ifndef MAIN_DIALOG_H_INCLUDED
#define MAIN_DIALOG_H_INCLUDED

#include <QtGui>
#include <QtWidgets>
#include <opencv2/highgui/highgui.hpp>
#include "CommandLine.hpp"
#include "DisplayWidget.hpp"

class RieszTransform;
class VideoSource;


// A DisplayWidget labeled with title t.
//
struct MainDialogVideo {
    QLabel itsTitle;
    DisplayWidget &itsDisplay;
    MainDialogVideo(const char *t, DisplayWidget &dw)
        : itsTitle(t)
        , itsDisplay(dw)
    {}
};


// A DisplayWidget labeled with title t and an FPS display.
//
// HACK: MainDialogVideoFps::box must point to a heap object because Qt
// deletes it when the parent MainDialog::itsGrid is destructed.
//
// And don't add a destructor to delete box because that deletes it twice.
//
struct MainDialogVideoFps: MainDialogVideo {
    QLCDNumber number;
    QLabel units;
    QHBoxLayout *box;
    // ~MainDialogVideoFps() { delete box; } // Don't do this either.
    MainDialogVideoFps(const char *t, DisplayWidget &dw, const char *u)
        : MainDialogVideo(t, dw)
        , number()
        , units(u)
        , box(new QHBoxLayout())
    {
        box->addStretch();
        box->addWidget(&number);
        box->addWidget(&units);
    }
};


// A DisplayWidget labeled with title t with a Save button to choose a
// video sink file.
//
struct MainDialogVideoSave: MainDialogVideo {
    QPushButton itsSave;
    std::string itsFile;
    cv::VideoWriter *itsSink;
    ~MainDialogVideoSave() { delete itsSink; }
    void saveTo(int codec, double fps, const cv::Size &size,
                const std::string &file)
    {
        itsFile = file;
        delete itsSink;
        itsSink = new cv::VideoWriter(itsFile, codec, fps, size);
        assert(itsSink->isOpened());
    }
    MainDialogVideoSave(const char *t, DisplayWidget &dw)
        : MainDialogVideo(t, dw)
        , itsSave("Save")
        , itsFile()
        , itsSink(0)
    {}
};


// A styled number slider labeled with title t and units u.
//
struct MainDialogSlider {
    QLabel title;
    QSlider slider;
    QLCDNumber number;
    QLabel units;
    MainDialogSlider(const char *t, const QString &u)
        : title(t)
        , slider(Qt::Horizontal)
        , number()
        , units(u)
    {
        slider.setFocusPolicy(Qt::StrongFocus);
        slider.setTickInterval(10);
        number.setSegmentStyle(QLCDNumber::Flat);
    }
};


// About and Exit buttons wired up to do what they say.
//
struct MainDialogButtons: QWidget {

    Q_OBJECT

    const std::string &program;

public slots:

    void doAbout();
    void doQuit();

public:

    QPushButton about;
    QPushButton quit;

    MainDialogButtons(const std::string &name, QWidget *parent = 0)
        : QWidget(parent)
        , program(name)
        , about("About", parent)
        , quit("Exit", parent)
    {
        connect(&quit,  SIGNAL(clicked()), this, SLOT(doQuit()));
        connect(&about, SIGNAL(clicked()), this, SLOT(doAbout()));
    }
};


// The main window of the Eulerizer application.
//
class MainDialog : public QDialog {

    Q_OBJECT

    void showEvent(QShowEvent *e);

    friend struct CommandLine;

    const CommandLine &itsCl;
    QTimer itsTimer;
    VideoSource *itsSource;
    RieszTransform *itsTransform;
    DisplayWidget itsInputWidget;
    DisplayWidget itsOutputWidget;
    MainDialogVideoFps itsInput;
    MainDialogVideoSave itsOutput;
    MainDialogSlider itsAmplify;
    MainDialogSlider itsLowCutoff;
    MainDialogSlider itsHighCutoff;
    MainDialogSlider itsThreshold;
    MainDialogButtons itsButtons;
    QCheckBox *itsRepeat;
    QProgressBar *itsBar;
    QGridLayout itsGrid;

    MeasureFps itsFps;

    void resetIo();
    void rangeSlidersForFps(double fps);
    void connectStuff();
    void layoutGrid();
    bool doEmptyFrame(bool more);

private slots:

    void chooseOutFile();
    bool doOneFrame();
    void changeAmplification(int value);
    void changeLowCutoff(int value);
    void changeHighCutoff(int value);
    void changeThreshold(int value);

public:

    ~MainDialog();

    // True iff a video source was selected.
    //
    bool ok () const { return itsSource && itsTransform; }

    // Initialize the GUI according to cl.
    //
    MainDialog(const CommandLine &cl);
};

#endif // #ifndef MAIN_DIALOG_H_INCLUDED

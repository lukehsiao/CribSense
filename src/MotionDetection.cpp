#include <opencv2/opencv.hpp>
#include "MotionDetection.hpp"
#include <time.h>

enum motionDetection_st {
    init_st,            // enter this state while waiting for frames to settle
    reset_st,           // re-enlarge the video and recalculate ROI
    idle_st,            // evaluation is valid to compute from
    monitor_motion_st,  // observe motions in several frams
    compute_roi_st,     // occasionally recompute roi
    valid_roi_st        // Flag a valid ROI
} currentState = init_st;

/**
 * Launch rt.transform for the given RieszTransform and the given frame.
 */
static cv::Mat
do_transforms(RieszTransform* rt, cv::Mat frame)
{
    return rt->transform(frame);
}

static void debugStatePrint(void) __attribute__((unused));

static void debugStatePrint(void) {
    static motionDetection_st previousState;
    static bool firstPass = true;
    if (previousState != currentState || firstPass) {
        firstPass = false;
        previousState = currentState;
        switch(currentState) {
            case init_st:
                printf("init_st\n");
                break;
            case reset_st:
                printf("reset_st\n");
                break;
            case idle_st:
                printf("idle_st\n");
                break;
            case compute_roi_st:
                printf("compute_roi_st\n");
                break;
            case monitor_motion_st:
                printf("monitor_motion_st\n");
                break;
            case valid_roi_st:
                printf("valid_roi_st\n");
                break;
            default:
                printf("[error] Invalid state reached.\n");
                break;
        }
    }
}

static inline double get_time_ms() {
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((double)(ts.tv_sec * 1000) + ts.tv_nsec / 1000000);
}

static inline unsigned get_time_sec() {
	struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (unsigned) ts.tv_sec;
}

void MotionDetection::DifferentialCollins() {
    cv::Mat h_d1;
    cv::Mat h_d2;
    cv::Mat eval;

    // Check differences
    // NOTE: If further optimization is necessary, we can probably hand-code this
    // part to expose the loops to the compiler.
    cv::absdiff(frameBuffer[0], frameBuffer[2], h_d1);
    cv::absdiff(frameBuffer[1], frameBuffer[2], h_d2);
    cv::bitwise_and(h_d1, h_d2, eval);

    // threshold
    cv::threshold(eval, eval, diffThreshold, 255, CV_THRESH_BINARY);

    // erode
    // cv::erode(eval, eval, erodeKernel);

    evaluation =  eval;
}

void MotionDetection::calculatePeriod() {
    // weight of the exponentially-weighted moving average. Higher ratio gives
    // more weight to more recent samples.
    const double ALPHA = 0.4;
    static double lastTimestamp = get_time_ms();

    // get time in milliseconds
    double timestamp = get_time_ms();

    // amount of time that has passed between peaks
    double period = timestamp - lastTimestamp;
    // TODO: This is what is trying to deal with the bimodal nature of the Movement
    // of breathing (movement while inhale, still, movement while exhale quickly after)
    // As a side effect, this limits the maximum frequency we detect.
    if (period > 400) { // low-pass filter of peaks occuring faster than 400ms apart
        double newRate = 1.0 / (period / 1000);
        breathingRate = ALPHA * newRate + (1 - ALPHA) * breathingRate;
        lastTimestamp = timestamp;
    }
}

void MotionDetection::soundAlarm() {
    int playing;

    ca_context_playing(snd_context, 0, &playing);
    if (playing == 0)
        ca_context_play(snd_context, 0, CA_PROP_EVENT_ID, "alarm-clock-elapsed", nullptr);

    printf("[ERROR] >>>>>  NO MOVEMENT DETECTED!!!!!!\n\n");
}

unsigned MotionDetection::countNumChanges() {
    /**
     * NOTE: We are using an exponentially-weighted moving average to smooth
     * out the data here. Otherwise, the data has a high-frequency component
     * that makes peak detection more difficult.
     */
    static unsigned ewma = 0;

    // weight of the exponentially-weighted moving average. Higher ratio gives
    // more weight to more recent samples.
    const double ALPHA = 0.3;

    static unsigned lastEWMA = 0;
    static bool wasRising = true;

    static unsigned lastZeroStartTime;
    static bool noMovementDetected = false;

    cv::Mat frameErode = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    // Erode the remaining noise
    cv::erode(evaluation, evaluation, frameErode);

    if (currentState == idle_st) {
        int numberOfChanges = 0;

        // -----------------------------------
        // loop over image and detect changes

        for(int i = 0; i < evaluation.cols; i++)
        {
            for(int j = 0; j <  evaluation.rows; j++)
            {
                if(static_cast<int>(evaluation.at<uchar>(j,i)) == 255)
                {
                    numberOfChanges++;
                }
            }
        }

        static int duration = 0;
        if (numberOfChanges >= pixelThreshold) {
            duration++;
            if (duration >= motionDuration) {
                // Update the moving average
                ewma = ALPHA * (double)numberOfChanges + (1 - ALPHA) * (double)ewma;

                // The first time a fall is detected (signifying a peak) log the
                // time and calculate a breathing rate. Then, wait until we
                // are rising again before looking for another fall.
                if ((ewma < lastEWMA) && wasRising) {
                    calculatePeriod();
                    wasRising = false;
                }
                else if ((ewma > lastEWMA) && !wasRising) {
                    wasRising = true;
                }
                lastEWMA = ewma;
                noMovementDetected = false;
                return ewma;
            }
        }
        else {
            if (duration > 0) {
                duration--;
            }
        }
    }
    if (noMovementDetected) {
        unsigned timestamp = get_time_sec();
        unsigned elapsedTime = timestamp - lastZeroStartTime;
        // printf("[info]    lastZeroStartTime: %u\n", lastZeroStartTime);
        // printf("[info]    timestamp:         %u\n", timestamp);
        // printf("[info]    elapsedTime:       %u\n", elapsedTime);
        if (elapsedTime >= timeToAlarm) {
            soundAlarm();
        }
    }
    else {
        noMovementDetected = true;
        lastZeroStartTime = get_time_sec();
        // printf("[info]    lastZeroStartTime: %u\n", lastZeroStartTime);
    }
    return 0;
}

double MotionDetection::getBreathingRate() {
    return breathingRate;
}

cv::Mat MotionDetection::magnifyVideo(cv::Mat frame) {
    static cv::Mat result;

    // Split a single 640 x 480 frame into equal sections, 1 section
    // for each thread to process
    // Run each transform independently.
    static std::future<cv::Mat> futures[SPLIT];
    static cv::Mat in_sections[SPLIT];
    static cv::Mat out_sections[SPLIT];


    for (int i = 0; i < SPLIT; i++) {
        auto rowRange = cv::Range(frame.rows * i / SPLIT, (frame.rows * (i+1) / SPLIT));
        auto colRange = cv::Range(0, frame.cols);
        in_sections[i] = frame(rowRange, colRange);
        futures[i] = thread[i].push(do_transforms, &rt[i], in_sections[i]);
    }

    // recombine results and output
    for (int i = 0; i < SPLIT; i++) {
        out_sections[i] = futures[i].get();
    }
    cv::vconcat(out_sections, 3, result);

    if (showMagnification) {
        cv::imshow("result", result);
        cv::waitKey(1);
    }
    return result;
}

void MotionDetection::monitorMotion() {
    if (currentState == reset_st) {
        accumulator = cv::Mat::zeros(frameHeight, frameWidth, CV_8UC1);
        return;
    }
    // Bitwise OR all the frames in the window to aggregate motion
    cv::bitwise_or(evaluation, accumulator, accumulator);
}

void MotionDetection::calculateROI() {
    static int prevArea = frameWidth * frameHeight / 3;

    // Erode the remaining noise
    cv::erode(accumulator, accumulator, erodeKernel);

    // Dialate the remaining signal
    cv::dilate(accumulator, accumulator, dilateKernel);

    // NOTE: Uncomment this to view what the post-dilation view looks like
    if (showDiff) {
        cv::imshow("Accumulator", accumulator);
        cv::waitKey(0);
        cv::destroyAllWindows();
    }

    // Create bitmask
    cv::Mat maskFrame = accumulator > 200;

    int largestArea = 0;
    int largestContour = 0;
    std::vector<std::vector<cv::Point>> contours;
    findContours(maskFrame, contours, cv::noArray(), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    for(unsigned int i = 0; i < contours.size(); i++ ) {
        int area = (int) (cv::contourArea(contours[i], false) + 0.5); // + 0.5 for rounding
        if(area > largestArea) {
            largestArea = area;
            largestContour = i;
        }
    }

    if (contours.empty()) {
        printf("[info] Hmmm...didn't see any motion....\n");
        // If the ROI has been cropped before, just use that same one
        // otherwise, go ahead and just choose a crop for now.
        if ((roi.width * roi.height) > (frameWidth * frameHeight / 3)) {
            printf("[info] Choosing an arbitrary crop for now.\n");
            // Case where it's never been cropped just get a crop at (0,0)
            // In the future, could center this or something.
            roi = cv::Rect(0, 0, frameWidth/3, frameHeight/3);
        }
        return;
    }
    else {
        cv::Rect result = cv::boundingRect(contours[largestContour]);;
        // NOTE: Add smoothing function here. We want to enforce some size
        // constrainst and not allow HUGE changes in frame size (which would
        // indicate a bad read, and that we should just stay the same until
        // next read).
        if (largestArea >= frameWidth * frameHeight / 3) {
            // NOTE: Just trying to crop down in a reasonable way. Making
            // a 90k pixel square centered on the geometric center of the
            // original bounding rect.
            int c_x = result.x + result.width/2;
            int c_y = result.y + result.height/2;

            // make sure the roi is inside the image
            const int target_dim = 300;
            cv::Rect target_roi = cv::Rect(c_x - 150, c_y - 150, target_dim, target_dim);
            bool is_inside = (target_roi & cv::Rect(0, 0, accumulator.cols, accumulator.rows)) == target_roi;
            if (is_inside) {
                result = target_roi;
            }
            else {
                // [info] prevArea: 102400, largestArea: 105572
                // [200 x 200 from (484, 302)]
                if (target_roi.x + target_roi.width > frameWidth) {
                    target_roi.x -= target_roi.x + target_roi.width - frameWidth;
                }
                if (target_roi.y + target_roi.height > frameHeight) {
                    target_roi.y -= target_roi.y + target_roi.height - frameHeight;
                }
                if (target_roi.x < 0) {
                    target_roi.x = 0;
                }
                if (target_roi.y < 0) {
                    target_roi.y = 0;
                }
            }
            result = target_roi;
            largestArea = target_dim * target_dim;
        }
        else if (largestArea <= frameWidth * frameHeight / 20) {
            // TODO: Too small, enlarging it slightly around the geometric center
            int c_x = result.x + result.width/2;
            int c_y = result.y + result.height/2;
            result = cv::Rect(c_x - 100, c_y - 100, 200, 200);

            // make sure the roi is inside the image
            const int target_dim = 200;
            cv::Rect target_roi = cv::Rect(c_x - 150, c_y - 150, target_dim, target_dim);
            bool is_inside = (target_roi & cv::Rect(0, 0, accumulator.cols, accumulator.rows)) == target_roi;
            if (is_inside) {
                result = target_roi;
            }
            else {
                // [info] prevArea: 102400, largestArea: 105572
                // [200 x 200 from (484, 302)]
                if (target_roi.x + target_roi.width > frameWidth) {
                    target_roi.x -= target_roi.x + target_roi.width - frameWidth;
                }
                if (target_roi.y + target_roi.height > frameHeight) {
                    target_roi.y -= target_roi.y + target_roi.height - frameHeight;
                }
                if (target_roi.x < 0) {
                    target_roi.x = 0;
                }
                if (target_roi.y < 0) {
                    target_roi.y = 0;
                }
            }

            result = target_roi;
            largestArea = target_dim * target_dim;
        }

        // std::cout << result << std::endl;

        // Smooth the changes, if any. No changes greather than 30%
        if (std::abs(largestArea - prevArea) * 100 / prevArea <= 80) {
            prevArea = largestArea;
            roi = result;
        }
    }
}

void MotionDetection::pushFrameBuffer(cv::Mat newFrame) {
    // Update the frame buffer
    for (int i = 0; i < (MINIMUM_FRAMES - 1); i++) {
        frameBuffer[i] = frameBuffer[i + 1];
    }
    frameBuffer[MINIMUM_FRAMES-1] = cv::Mat();
    newFrame.copyTo(frameBuffer[MINIMUM_FRAMES-1]);
}

void MotionDetection::reinitializeReisz(cv::Mat frame, frame_size size) {
    static cv::Mat in_sections[SPLIT];
    for (int i = 0; i < SPLIT; i++) {
        auto rowRange = cv::Range(frame.rows * i / SPLIT, (frame.rows * (i+1) / SPLIT));
        auto colRange = cv::Range(0, frame.cols);
        in_sections[i] = frame(rowRange, colRange);
        rt[i].initialize(in_sections[i]);

        // NOTE: If we're reading from a file, we're not dropping anything, so
        // just read at the file's FPS (which was initialized already).
        if (usingCamera) {
            switch(size) {
                case FULL_FRAME:
                    rt[i].fps(full_fps);
                    break;
                case CROPPED_FRAME:
                    rt[i].fps(crop_fps);
                    break;
                default:
                    printf("[error] Invalid crop size passed in.\n");
            }
        }
        else {
            rt[i].fps(input_fps);
        }
    }
}

void MotionDetection::update(cv::Mat newFrame) {
    // Print states to terminal for debugging
    // debugStatePrint();

    static unsigned initTimer = 0;
    static unsigned validTimer = 0;
    static unsigned roiTimer = 0;
    static unsigned refillTimer = 0;

    //////////////////////////////////////
    // Perform state actions first      //
    //////////////////////////////////////
    switch(currentState) {
        case init_st:
            initTimer++;
            pushFrameBuffer(magnifyVideo(newFrame));
            break;
        case reset_st:
            initTimer++;
            pushFrameBuffer(magnifyVideo(newFrame));
            // Reset ReiszTransforms and window
            monitorMotion();
            break;
        case idle_st:
            validTimer++;
            printf("[info] Pixel Movement: %d\t [info] Motion Estimate: %f Hz\n", countNumChanges(),  getBreathingRate());
            pushFrameBuffer(magnifyVideo(newFrame(roi)));
            DifferentialCollins();
            break;
        case monitor_motion_st:
            roiTimer++;
            pushFrameBuffer(magnifyVideo(newFrame));
            DifferentialCollins();
            monitorMotion();
            break;
        case compute_roi_st: // spend 1 frame to just calculate ROI
            calculateROI();
            break;
        case valid_roi_st:
            refillTimer++;
            pushFrameBuffer(newFrame(roi)); // flushes frameBuffer with new size
            break;
        default:
            printf("[error] Invalid state reached.\n");
            break;
    }

    //////////////////////////////////////
    // Perform state update next        //
    //////////////////////////////////////
    switch(currentState) {
        case init_st:
            if (initTimer >= framesToSettle) {
                if (crop) {
                    currentState = monitor_motion_st;
                }
                else {  // if crop is false, just hang out.
                    currentState = idle_st;
                }
                initTimer = 0;
            }
            break;
        case reset_st:
            if (initTimer >= framesToSettle) {
                currentState = monitor_motion_st;
                initTimer = 0;
            }
            break;
        case idle_st:
            if (validTimer >= roiUpdateInterval) {
                if (crop) {
                    currentState = reset_st;
                    reinitializeReisz(newFrame, FULL_FRAME);
                }
                else {  // if crop is false, just hang out.
                    currentState = idle_st;
                }
                validTimer = 0;
            }
            break;
        case monitor_motion_st:
            if (roiTimer >= roiWindow) {
                currentState = compute_roi_st;
                roiTimer = 0;
            }
            break;
        case compute_roi_st:
            currentState = valid_roi_st;
            break;
        case valid_roi_st:
            if (refillTimer >= MINIMUM_FRAMES) {
                currentState = idle_st;
                reinitializeReisz(newFrame(roi), CROPPED_FRAME);
                refillTimer = 0;
            }
            break;
        default:
            printf("[error] Invalid state reached.\n");
            break;
    }
}

MotionDetection::MotionDetection(const CommandLine &cl) {
    frameCount = 0;
    diffThreshold = cl.diffThreshold;
    showDiff = cl.showDiff;
    showMagnification = cl.showMagnification;
    pixelThreshold = cl.pixelThreshold;
    motionDuration = cl.motionDuration;
    framesToSettle = cl.framesToSettle;
    roiUpdateInterval = cl.roiUpdateInterval;
    roiWindow = cl.roiWindow;
    crop = cl.crop;
    frameWidth = cl.frameWidth;
    frameHeight = cl.frameHeight;
    breathingRate = 1.0;
    full_fps = cl.full_fps;
    crop_fps = cl.crop_fps;
    input_fps = cl.input_fps;
    timeToAlarm = cl.timeToAlarm;
    roi = cv::Rect(cv::Point(0, 0), cv::Point(cl.frameWidth, cl.frameHeight));
    erodeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(cl.erodeDimension, cl.erodeDimension));
    dilateKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(cl.dilateDimension, cl.dilateDimension));
    accumulator = cv::Mat::zeros(cl.frameHeight, cl.frameWidth, CV_8UC1);
    usingCamera = (cl.cameraId >= 0);
    ca_context_create(&snd_context);
    ca_context_open(snd_context);

    for (int i = 0; i < SPLIT; i++) {
        cl.apply(rt[i]);
        if (usingCamera) {
            rt[i].fps(full_fps);
        }
        else {
            rt[i].fps(cl.input_fps);
        }

    }
}

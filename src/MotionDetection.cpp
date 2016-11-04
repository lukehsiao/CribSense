#include <opencv2/opencv.hpp>
#include "MotionDetection.hpp"

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
cv::Mat do_transforms(RieszTransform* rt, cv::Mat frame)
{
    return rt->transform(frame);
}

void debugStatePrint() {
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

    if (showDiff) {
        cv::imshow( "Evaluation", eval );                  // Show our image inside it.
        cv::waitKey(0);
        cv::destroyAllWindows();
    }
    evaluation =  eval;
}

int MotionDetection::isValidMotion() {
    if (currentState == idle_st) {
        int numberOfChanges = 0;
        cv::Rect rectangle(evaluation.cols, evaluation.rows, 0, 0);

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
                return numberOfChanges;
            }
        }
        else {
            if (duration > 0) {
                duration--;
            }
        }
    }
    return 0;
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
    cv::imshow("result", result);
    cv::waitKey(30);

    return result;
}


void MotionDetection::monitorMotion() {
    if (currentState == reset_st) {
        accumulator = cv::Mat::zeros(480, 640, CV_8UC1);
        return;
    }
    // Bitwise OR all the frames in the window to aggregate motion
    cv::bitwise_or(evaluation, accumulator, accumulator);
}

void MotionDetection::calculateROI() {
    static int prevArea = 640 * 480 / 3;

    // Erode the remaining noise
    cv::Mat erode_noise = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::erode(accumulator, accumulator, erode_noise);

    // Dialate the remaining signal
    cv::Mat dilateKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(60, 60));
    cv::dilate(accumulator, accumulator, dilateKernel);

    // Create bitmask
    cv::Mat maskFrame = accumulator > 200;

    double largest_area = 0;
    int largest_contour = 0;
    std::vector<std::vector<cv::Point>> contours;
    findContours(maskFrame, contours, cv::noArray(), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    for(unsigned int i = 0; i < contours.size(); i++ ) {
        double area = cv::contourArea(contours[i], false);
        if(area > largest_area) {
            largest_area = area;
            largest_contour = i;
        }
    }

    if (contours.empty()) {
        printf("[info] Uhh....didn't see any motion....\n");
        roi = cv::Rect(0, 0, 640, 480);
    }
    else {
        // TODO: Add smoothing function here. We want to enforce some size
        // constrainst and not allow HUGE changes in frame size (which would
        // indicate a bad read, and that we should just stay the same until
        // next read).
        roi = cv::boundingRect(contours[largest_contour]);
    }

    // NOTE: Uncomment this to view what the post-dilation view looks like
    // cv::imshow("ORed", accumulator);
    // cv::waitKey(0);
}

void MotionDetection::pushFrameBuffer(cv::Mat newFrame) {
    // Update the frame buffer
    for (int i = 0; i < (MINIMUM_FRAMES - 1); i++) {
        frameBuffer[i] = frameBuffer[i + 1];
    }
    frameBuffer[MINIMUM_FRAMES-1] = newFrame;
    cv::cvtColor(frameBuffer[MINIMUM_FRAMES-1], frameBuffer[MINIMUM_FRAMES-1], CV_BGR2GRAY);
}

void MotionDetection::reinitializeReisz(cv::Mat frame) {
    static cv::Mat in_sections[SPLIT];
    for (int i = 0; i < SPLIT; i++) {
        auto rowRange = cv::Range(frame.rows * i / SPLIT, (frame.rows * (i+1) / SPLIT));
        auto colRange = cv::Range(0, frame.cols);
        in_sections[i] = frame(rowRange, colRange);
        rt[i].initialize(in_sections[i]);
    }
}

void MotionDetection::update(cv::Mat newFrame) {
    debugStatePrint();
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
            pushFrameBuffer(magnifyVideo(newFrame(roi)));
            DifferentialCollins();
            break;
        case monitor_motion_st:
            roiTimer++;
            pushFrameBuffer(magnifyVideo(newFrame));
            DifferentialCollins();
            monitorMotion();
            break;
        case compute_roi_st:
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
                currentState = monitor_motion_st;
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
                currentState = reset_st;
                reinitializeReisz(newFrame);
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
                // std::cout << roi << std::endl;
                currentState = idle_st;
                reinitializeReisz(newFrame(roi));
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
    pixelThreshold = cl.pixelThreshold;
    motionDuration = cl.motionDuration;
    framesToSettle = cl.framesToSettle;
    roiUpdateInterval = cl.roiUpdateInterval;
    roiWindow = cl.roiWindow;
    roi = cv::Rect(cv::Point(0, 0), cv::Point(640, 480));
    erodeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(cl.erodeDimension, cl.erodeDimension));
    accumulator = cv::Mat::zeros(480, 640, CV_8UC1);

    for (int i = 0; i < SPLIT; i++) {
        cl.apply(rt[i]);
        rt[i].fps(cl.fps);
    }

}

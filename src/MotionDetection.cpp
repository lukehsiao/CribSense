#include <opencv2/opencv.hpp>
#include "MotionDetection.hpp"

enum motionDetection_st {
    init_st,        // enter this state while waiting for frames to settle
    reset_st,       // re-enlarge the video and recalculate ROI
    idle_st,        // evaluation is valid to compute from
    compute_roi_st, // occasionally recompute roi
    valid_roi_st    // Flag a valid ROI
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
    cv::erode(eval, eval, erodeKernel);

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

void MotionDetection::calculateROI() {
    static int maxNumChanges = 0;
    static int prevArea = 640 * 480 / 2;
    int numberOfChanges = 0;
    int x1 = evaluation.cols;
    int y1 = evaluation.rows;
    int x2 = 0;
    int y2 = 0;

    if (currentState == reset_st) {
      maxNumChanges = 0;
      return;
    }

    // -----------------------------------
    // loop over image and detect changes

    for(int i = 0; i < 640; i++)
    {
        for(int j = 0; j < 480; j++)
        {
            if(static_cast<int>(evaluation.at<uchar>(j,i)) == 255)
            {
                numberOfChanges++;
                if(x1>i) x1 = i;
                if(x2<i) x2 = i;
                if(y1>j) y1 = j;
                if(y2<j) y2 = j;
            }
        }
    }

    // -------------------------
    //check if not out of bounds

    if(x1-10 > 0) x1 -= 10;
    if(y1-10 > 0) y1 -= 10;
    if(x2+10 < evaluation.cols-1) x2 += 10;
    if(y2+10 < evaluation.rows-1) y2 += 10;

    if(numberOfChanges > maxNumChanges && numberOfChanges > 30) {
        maxNumChanges = numberOfChanges;
        int newArea = (x2-x1) * (y2-y1);
        // NOTE: Some sort of smoothing function here. Don't want drastic changes.
        if (std::abs(newArea - prevArea) <= 100000 && newArea <= 640 * 480 / 3 && newArea >= 640 * 480 / 10) {
            // printf("[info] numchanges: %d (%d, %d), (%d, %d)\n", numberOfChanges, x1, y1, x2, y2);
            prevArea = newArea;
            roi = cv::Rect(cv::Point(x1,y1), cv::Point(x2, y2));
        }
    }
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
            calculateROI();
            break;
        case idle_st:
            validTimer++;
            pushFrameBuffer(magnifyVideo(newFrame(roi)));
            DifferentialCollins();
            break;
        case compute_roi_st:
            roiTimer++;
            pushFrameBuffer(magnifyVideo(newFrame));
            DifferentialCollins();
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
                currentState = compute_roi_st;
                initTimer = 0;
            }
            break;
        case reset_st:
            if (initTimer >= framesToSettle) {
                currentState = compute_roi_st;
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
        case compute_roi_st:
            if (roiTimer >= roiWindow) {
                currentState = valid_roi_st;
                roiTimer = 0;
            }
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

    for (int i = 0; i < SPLIT; i++) {
        cl.apply(rt[i]);
        rt[i].fps(cl.fps);
    }

}

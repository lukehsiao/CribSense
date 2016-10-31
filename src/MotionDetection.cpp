#include <opencv2/opencv.hpp>
#include "MotionDetection.hpp"

cv::Mat MotionDetection::DifferentialCollins() {
    cv::Mat h_d1;
    cv::Mat h_d2;
    cv::Mat evaluation;

    // Check differences
    // NOTE: If further optimization is necessary, we can probably hand-code this
    // part to expose the loops to the compiler.
    cv::absdiff(frameBuffer[0], frameBuffer[2], h_d1);
    cv::absdiff(frameBuffer[1], frameBuffer[2], h_d2);
    cv::bitwise_and(h_d1, h_d2, evaluation);

    // threshold
    cv::threshold(evaluation, evaluation, diffThreshold, 255, CV_THRESH_BINARY);

    // erode
    cv::erode(evaluation, evaluation, erodeKernel);

    if (showDiff) {
        cv::imshow( "Evaluation", evaluation );                  // Show our image inside it.
        cv::waitKey(0);
        cv::destroyAllWindows();
    }
    return evaluation;
}


int MotionDetection::isValidMotion(cv::Mat newFrame) {
    frameBuffer[0] = frameBuffer[1];
    frameBuffer[1] = frameBuffer[2];
    frameBuffer[2] = newFrame;
    // convert to Grayscale
    cv::cvtColor(frameBuffer[2], frameBuffer[2], CV_BGR2GRAY);
    if (frameCount < 3) {
        frameCount++;
    }
    else {
        cv::Mat evaluation = DifferentialCollins();
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

MotionDetection::MotionDetection(const CommandLine &cl) {
    frameCount = 0;
    diffThreshold = cl.diffThreshold;
    showDiff = cl.showDiff;
    pixelThreshold = cl.pixelThreshold;
    motionDuration = cl.motionDuration;
    erodeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(cl.erodeDimension, cl.erodeDimension));
}

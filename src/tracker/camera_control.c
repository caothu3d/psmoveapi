/**
 * PS Move API - An interface for the PS Move Motion Controller
 * Copyright (c) 2012 Thomas Perl <m@thp.io>
 * Copyright (c) 2012 Benjamin Venditti <benjamin.venditti@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 **/

#include "camera_control.h"
#include "psmove_tracker.h"

#include "../external/iniparser/dictionary.h"
#include "../external/iniparser/iniparser.h"

#include "../psmove_private.h"

#include <stdio.h>
#include <stdint.h>

#include "camera_control_private.h"

#define PS3EYE_FOCAL_LENGTH_BLUE 554.2563  // Corresponds to 75 deg diagonal FOV
#define PS3EYE_FOCAL_LENGTH_RED 776.3782   // Corresponds to 56 deg diagonal FOV

extern enum PSMoveTracker_ErrorCode g_last_tracker_error_code;

#if defined(CAMERA_CONTROL_USE_PS3EYE_DRIVER)

/**
 * Taken from the PS3EYEDriver OpenFrameworks example
 * written by Eugene Zatepyakin, MIT license
 **/

static const int ITUR_BT_601_CY = 1220542;
static const int ITUR_BT_601_CUB = 2116026;
static const int ITUR_BT_601_CUG = -409993;
static const int ITUR_BT_601_CVG = -852492;
static const int ITUR_BT_601_CVR = 1673527;
static const int ITUR_BT_601_SHIFT = 20;

static void
yuv422_to_bgr(const uint8_t *yuv_src, const int stride, uint8_t *dst, const int width, const int height)
{
    const int bIdx = 2;
    const int uIdx = 0;
    const int yIdx = 0;

    const int uidx = 1 - yIdx + uIdx * 2;
    const int vidx = (2 + uidx) % 4;
    int j, i;

#define _max(a, b) (((a) > (b)) ? (a) : (b))
#define _saturate(v) (uint8_t)((uint32_t)(v) <= 0xff ? v : v > 0 ? 0xff : 0)

    for (j = 0; j < height; j++, yuv_src += stride)
    {
        uint8_t* row = dst + (width * 3) * j;

        for (i = 0; i < 2 * width; i += 4, row += 6)
        {
            int u = (int)(yuv_src[i + uidx]) - 128;
            int v = (int)(yuv_src[i + vidx]) - 128;

            int ruv = (1 << (ITUR_BT_601_SHIFT - 1)) + ITUR_BT_601_CVR * v;
            int guv = (1 << (ITUR_BT_601_SHIFT - 1)) + ITUR_BT_601_CVG * v + ITUR_BT_601_CUG * u;
            int buv = (1 << (ITUR_BT_601_SHIFT - 1)) + ITUR_BT_601_CUB * u;

            int y00 = _max(0, (int)(yuv_src[i + yIdx]) - 16) * ITUR_BT_601_CY;
            row[2-bIdx] = _saturate((y00 + buv) >> ITUR_BT_601_SHIFT);
            row[1]      = _saturate((y00 + guv) >> ITUR_BT_601_SHIFT);
            row[bIdx]   = _saturate((y00 + ruv) >> ITUR_BT_601_SHIFT);

            int y01 = _max(0, (int)(yuv_src[i + yIdx + 2]) - 16) * ITUR_BT_601_CY;
            row[5-bIdx] = _saturate((y01 + buv) >> ITUR_BT_601_SHIFT);
            row[4]      = _saturate((y01 + guv) >> ITUR_BT_601_SHIFT);
            row[3+bIdx] = _saturate((y01 + ruv) >> ITUR_BT_601_SHIFT);
        }
    }
}
#undef _max
#undef _saturate

#endif /* defined(CAMERA_CONTROL_USE_PS3EYE_DRIVER) */

void
get_metrics(int *width, int *height)
{
    *width = psmove_util_get_env_int(PSMOVE_TRACKER_WIDTH_ENV);
    *height = psmove_util_get_env_int(PSMOVE_TRACKER_HEIGHT_ENV);

    if (*width == -1) {
        *width = PSMOVE_TRACKER_DEFAULT_WIDTH;
    }

    if (*height == -1) {
        *height = PSMOVE_TRACKER_DEFAULT_HEIGHT;
    }
}


CameraControl *
camera_control_new(int cameraID)
{
    return camera_control_new_with_settings(cameraID, 0, 0, 0, 2);
}

CameraControl *
camera_control_new_with_settings(int cameraID, int width, int height, int framerate, int cam_type)
{
    CameraControl* cc = (CameraControl*) calloc(1, sizeof(CameraControl));
    cc->cameraID = cameraID;

    if (framerate <= 0) {
        framerate = PSMOVE_TRACKER_DEFAULT_FPS;
    }
    
    if (cam_type == PSMove_Camera_PS3EYE_BLUEDOT)
    {
        cc->focl_x = (float)PS3EYE_FOCAL_LENGTH_BLUE;
        cc->focl_y = (float)PS3EYE_FOCAL_LENGTH_BLUE;
    }
    else if (cam_type == PSMove_Camera_PS3EYE_REDDOT)
    {
        cc->focl_x = (float)PS3EYE_FOCAL_LENGTH_RED;
        cc->focl_y = (float)PS3EYE_FOCAL_LENGTH_RED;
        
    }
    else if (cam_type == PSMove_Camera_Unknown)
    {
        cc->focl_x = (float)PS3EYE_FOCAL_LENGTH_BLUE;
        cc->focl_y = (float)PS3EYE_FOCAL_LENGTH_BLUE;
    }

    // Needed for cbb tracker. Will be overwritten by camera calibration files if they exist.
    

#if defined(CAMERA_CONTROL_USE_CL_DRIVER)
    // Windows 32-bit. Either CL_SDK or Registry_requiring
    int cams = CLEyeGetCameraCount();

    if (cams <= cameraID) {
            free(cc);
            return NULL;
    }

    GUID cguid = CLEyeGetCameraUUID(cameraID);
    cc->camera = CLEyeCreateCamera(cguid,
        CLEYE_COLOR_PROCESSED, CLEYE_VGA, framerate);

    CLEyeCameraGetFrameDimensions(cc->camera, &width, &height);

    // Depending on color mode chosen, create the appropriate OpenCV image
    cc->frame4ch = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 4);
    cc->frame3ch = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

    CLEyeCameraStart(cc->camera);

#elif defined(CAMERA_CONTROL_USE_PS3EYE_DRIVER)
    // Mac or Windows
    // Initialize PS3EYEDriver
    ps3eye_init();
    int cams = ps3eye_count_connected();
    psmove_DEBUG("Found %i ps3eye(s) with CAMERA_CONTROL_USE_PS3EYE_DRIVER.\n", cams);
    if (cams <= cameraID) {
        g_last_tracker_error_code= PSMove_Camera_Not_Found;
        free(cc);
        return NULL;
    }

    if (width <= 0 || height <= 0) {
        get_metrics(&width, &height);
    }

    psmove_DEBUG("Attempting to open ps3eye with cameraId, width, height, framerate: %d, %d, %d, %d.\n", cameraID, width, height, framerate);
    cc->eye = ps3eye_open(cameraID, width, height, framerate);

    if (cc->eye == NULL) {
        g_last_tracker_error_code= PSMove_Camera_USB_Open_Failure;
        psmove_WARNING("Failed to open camera ID %d", cameraID);
        free(cc);
        return NULL;
    }

    cc->framebgr = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);

#else
    // Assume webcam accessible from OpenCV.
    char *video = psmove_util_get_env_string(PSMOVE_TRACKER_FILENAME_ENV);
    if (video) {
        psmove_DEBUG("Using '%s' as video input.\n", video);
        cc->capture = cvCaptureFromFile(video);
        free(video);
    } else {
        cc->capture = cvCaptureFromCAM(cc->cameraID);
        if (width <= 0 || height <= 0) {
            get_metrics(&width, &height);
        }
        cvSetCaptureProperty(cc->capture, CV_CAP_PROP_FRAME_WIDTH, width);
        cvSetCaptureProperty(cc->capture, CV_CAP_PROP_FRAME_HEIGHT, height);
    }

#endif
    cc->width = width;
    cc->height = height;
    cc->deinterlace = PSMove_False;

    return cc;
}

void
camera_control_set_deinterlace(CameraControl *cc,
        enum PSMove_Bool enabled)
{
    psmove_return_if_fail(cc != NULL);

    cc->deinterlace = enabled;
}

void
camera_control_read_calibration(CameraControl* cc,
        char* intrinsicsFile, char* distortionFile)
{
    CvMat *intrinsic = (CvMat*) cvLoad(intrinsicsFile, 0, 0, 0);
    CvMat *distortion = (CvMat*) cvLoad(distortionFile, 0, 0, 0);

    if (cc->mapx) {
        cvReleaseImage(&cc->mapx);
    }
    if (cc->mapy) {
        cvReleaseImage(&cc->mapy);
    }

    if (intrinsic && distortion) {
        if (!cc->frame3chUndistort) {
            enum PSMove_Bool new_frame;
            cc->frame3chUndistort = cvCloneImage(
                    camera_control_query_frame(cc, NULL, NULL, &new_frame));
        }
        int width, height;
        get_metrics(&width, &height);
        cc->mapx = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 1);
        cc->mapy = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 1);
        cvInitUndistortMap(intrinsic, distortion, cc->mapx, cc->mapy);
        cc->focl_x = CV_MAT_ELEM(*intrinsic, float, 0, 0);
        cc->focl_y = CV_MAT_ELEM(*intrinsic, float, 1, 1);

        // TODO: Shouldn't we free intrinsic and distortion here?
    } else {
        fprintf(stderr, "Warning: No lens calibration files found.\n");
    }
}

void
camera_control_reset_calibration(CameraControl* cc)
{
    if (cc->mapx) {
        cvReleaseImage(&cc->mapx);
    }
    if (cc->mapy) {
        cvReleaseImage(&cc->mapy);
    }
}

IplImage *
camera_control_query_frame( CameraControl* cc,
                            PSMove_timestamp *ts_grab,
                            PSMove_timestamp *ts_retrieve,
                            enum PSMove_Bool *out_new_frame)
{
    IplImage* result;
    *out_new_frame = PSMove_False;

#if defined(CAMERA_CONTROL_USE_CL_DRIVER)
    // assign buffer-pointer to address of buffer
    cvGetRawData(cc->frame4ch, &cc->pCapBuffer, 0, 0);

    CLEyeCameraGetFrame(cc->camera, cc->pCapBuffer, 2000);

    // convert 4ch image to 3ch image
    const int from_to[] = { 0, 0, 1, 1, 2, 2 };
    const CvArr** src = (const CvArr**) &cc->frame4ch;
    CvArr** dst = (CvArr**) &cc->frame3ch;
    cvMixChannels(src, 1, dst, 1, from_to, 3);
    *out_new_frame= PSMove_True;
    result = cc->frame3ch;
#elif defined(CAMERA_CONTROL_USE_PS3EYE_DRIVER)
    int stride = 0;
    unsigned char *pixels = ps3eye_grab_frame(cc->eye, &stride);
    // Convert pixels from camera to BGR
    unsigned char *cvpixels;
    cvGetRawData(cc->framebgr, &cvpixels, 0, 0);
    if (pixels != NULL)
    {
        yuv422_to_bgr(pixels, stride, cvpixels, cc->width, cc->height);
        *out_new_frame = PSMove_True;
    }
    result = cc->framebgr;
#else
    cvGrabFrame(cc->capture);
    if (ts_grab != NULL) {
        *ts_grab = psmove_timestamp();
    }
    result = cvRetrieveFrame(cc->capture, 0);
    if (ts_retrieve != NULL) {
        *ts_retrieve = psmove_timestamp();
    }
    *out_new_frame = PSMove_True;
#endif

    if (cc->deinterlace == PSMove_True) {
        /**
         * Dirty hack follows:
         *  - Clone image
         *  - Hack internal variables to make an image of all odd lines
         **/
        IplImage *tmp = cvCloneImage(result);
        tmp->imageData += tmp->widthStep; // odd lines
        tmp->widthStep *= 2;
        tmp->height /= 2;

        /**
         * Use nearest-neighbor to be faster. In my tests, this does not
         * cause a speed disadvantage, and tracking quality is still good.
         *
         * This will scale the half-height image "tmp" to the original frame
         * size by doubling lines (so we can still do normal circle tracking).
         **/
        cvResize(tmp, result, CV_INTER_NN);

        /**
         * Need to revert changes in tmp from above, otherwise the call
         * to cvReleaseImage would cause a crash.
         **/
        tmp->height = result->height;
        tmp->widthStep = result->widthStep;
        tmp->imageData -= tmp->widthStep; // odd lines
        cvReleaseImage(&tmp);
    }

    // undistort image
    if (cc->mapx && cc->mapy) {
        cvRemap(result, cc->frame3chUndistort,
                cc->mapx, cc->mapy,
                CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS,
                cvScalarAll(0));
        result = cc->frame3chUndistort;
    }


#if defined(CAMERA_CONTROL_DEBUG_CAPTURED_IMAGE)
    cvShowImage("camera input", result);
    cvWaitKey(1);
#endif

    return result;
}

void
camera_control_delete(CameraControl* cc)
{
#if defined(CAMERA_CONTROL_USE_CL_DRIVER)
    if (cc->frame3ch != 0x0)
        cvReleaseImage(&cc->frame3ch);

    if (cc->frame4ch != 0x0)
        cvReleaseImage(&cc->frame4ch);

    CLEyeDestroyCamera(cc->camera);
#elif defined(CAMERA_CONTROL_USE_PS3EYE_DRIVER)
    cvReleaseImage(&cc->framebgr);

    ps3eye_close(cc->eye);
    ps3eye_uninit();
#else
    // linux, others and windows opencv only
    cvReleaseCapture(&cc->capture);
#endif

    if (cc->frame3chUndistort) {
        cvReleaseImage(&cc->frame3chUndistort);
    }

    if (cc->mapx) {
        cvReleaseImage(&cc->mapx);
    }

    if (cc->mapy) {
        cvReleaseImage(&cc->mapy);
    }

    free(cc);
}


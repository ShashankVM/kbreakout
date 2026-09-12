/*
    SPDX-FileCopyrightText: 2026 KBreakOut contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cameracontroller.h"

#include "kbreakout_debug.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPermission>
#include <QPermissions>
#include <QThread>
#include <QVideoFrame>
#include <QVideoSink>

#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>

#include <algorithm>
#include <optional>
#include <vector>

namespace
{
constexpr int targetWidth = 480;
constexpr int minimumFrameIntervalMs = 16;
constexpr int markerId = 0;
constexpr qreal smoothingWeight = 0.70;

std::optional<qreal> findMarkerPosition(const QImage &image)
{
    cv::Mat rgb(image.height(), image.width(), CV_8UC3,
                const_cast<uchar *>(image.constBits()), image.bytesPerLine());
    cv::Mat gray;
    cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);

    static const cv::aruco::Dictionary dictionary =
        cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    static const cv::aruco::DetectorParameters parameters = [] {
        cv::aruco::DetectorParameters value;
        value.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
        return value;
    }();
    static const cv::aruco::ArucoDetector detector(dictionary, parameters);

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    detector.detectMarkers(gray, corners, ids);
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] != markerId || corners[i].size() != 4) {
            continue;
        }
        qreal centerX = 0.0;
        for (const cv::Point2f &corner : corners[i]) {
            centerX += corner.x;
        }
        centerX /= corners[i].size();
        return std::clamp(centerX / qMax(1, image.width() - 1), 0.0, 1.0);
    }
    return std::nullopt;
}
}

CameraController::CameraController(QObject *parent)
    : QObject(parent)
    , m_mirror(qEnvironmentVariableIntValue("KBREAKOUT_CAMERA_MIRROR") != 0)
{
    m_processingThread = new QThread(this);
    m_processor = new QObject;
    m_processor->moveToThread(m_processingThread);
    connect(m_processingThread, &QThread::finished, m_processor, &QObject::deleteLater);
    m_processingThread->start();
}

CameraController::~CameraController()
{
    if (m_camera) {
        m_camera->stop();
    }
    m_processingThread->quit();
    m_processingThread->wait();
}

void CameraController::start()
{
    QCameraPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Granted:
        openCamera();
        break;
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this](const QPermission &result) {
            if (result.status() == Qt::PermissionStatus::Granted) {
                openCamera();
            } else {
                Q_EMIT statusChanged(tr("Camera permission was denied"));
            }
        });
        break;
    case Qt::PermissionStatus::Denied:
        Q_EMIT statusChanged(tr("Camera permission was denied"));
        break;
    }
}

void CameraController::openCamera()
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        Q_EMIT statusChanged(tr("No camera was found"));
        return;
    }

    QCameraDevice selected = QMediaDevices::defaultVideoInput();
    const QByteArray requestedId = qgetenv("KBREAKOUT_CAMERA_ID");
    for (const QCameraDevice &camera : cameras) {
        const bool requested = !requestedId.isEmpty() && camera.id() == requestedId;
        const bool face2Face = camera.description().contains(QStringLiteral("Robo Face2Face K20"), Qt::CaseInsensitive)
            || camera.description().contains(QStringLiteral("Face2Face K20"), Qt::CaseInsensitive);
        if (requested || (requestedId.isEmpty() && face2Face)) {
            selected = camera;
            break;
        }
    }

    m_camera = new QCamera(selected, this);
    m_captureSession = new QMediaCaptureSession(this);
    m_videoSink = new QVideoSink(this);
    m_captureSession->setCamera(m_camera);
    m_captureSession->setVideoSink(m_videoSink);
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &CameraController::processFrame);
    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &error) {
        Q_EMIT statusChanged(error);
    });
    m_frameTimer.start();
    m_camera->start();
    Q_EMIT statusChanged(tr("Tracking with %1").arg(selected.description()));
    qCDebug(KBREAKOUT_General) << "Camera control using" << selected.description() << selected.id();
}

void CameraController::processFrame(const QVideoFrame &frame)
{
    if (m_frameTimer.isValid() && m_frameTimer.elapsed() < minimumFrameIntervalMs) {
        return;
    }
    m_frameTimer.restart();

    if (m_framePending.exchange(true)) {
        return;
    }

    QImage image = frame.toImage();
    if (image.isNull()) {
        m_framePending = false;
        return;
    }
    if (image.width() > targetWidth) {
        image = image.scaledToWidth(targetWidth, Qt::FastTransformation);
    }
    image = image.convertToFormat(QImage::Format_RGB888);

    QMetaObject::invokeMethod(m_processor, [this, image = std::move(image)] {
        const std::optional<qreal> detectedPosition = findMarkerPosition(image);
        if (detectedPosition) {
            qreal position = m_mirror ? 1.0 - *detectedPosition : *detectedPosition;
            m_smoothedPosition = m_smoothedPosition < 0.0
                ? position
                : smoothingWeight * position + (1.0 - smoothingWeight) * m_smoothedPosition;
            Q_EMIT positionChanged(std::clamp(m_smoothedPosition, 0.0, 1.0));
        }
        m_framePending = false;
    }, Qt::QueuedConnection);
}

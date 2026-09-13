/*
    SPDX-FileCopyrightText: 2026 KBreakOut contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QElapsedTimer>

#include <atomic>

class QCamera;
class QMediaCaptureSession;
class QThread;
class QVideoFrame;
class QVideoSink;

class CameraController : public QObject
{
    Q_OBJECT

public:
    explicit CameraController(QObject *parent = nullptr);
    ~CameraController() override;

    void start();

Q_SIGNALS:
    void positionChanged(qreal position);
    void statusChanged(const QString &status);

private Q_SLOTS:
    void processFrame(const QVideoFrame &frame);

private:
    void openCamera();
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_captureSession = nullptr;
    QVideoSink *m_videoSink = nullptr;
    QThread *m_processingThread = nullptr;
    QObject *m_processor = nullptr;
    QElapsedTimer m_frameTimer;
    std::atomic_bool m_framePending = false;
    bool m_mirror = false;
};

#endif

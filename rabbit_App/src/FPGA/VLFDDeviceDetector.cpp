#include <QApplication>
#include <QDebug>
#include <QMutex>
#include <QString>

#include <cstring>

#include "FPGA/VLFDDeviceDetector.h"

using namespace rabbit_App::fpga;

VLFDDeviceDetector *VLFDDeviceDetector::instance_ = nullptr;

VLFDDeviceDetector::VLFDDeviceDetector(QObject *parent) : QObject(parent) {
  instance_ = this;
  // thread_ = new QThread(this);
  // thread_->setObjectName("VLFDDeviceDetectorThread");
  // timer_ = new QTimer(this);
  // timer_->setInterval(kTimerInterval);
  // timer_->moveToThread(thread_);
  // connect(timer_, &QTimer::timeout, this,
  // &VLFDDeviceDetector::onTimerTimeOut,
  //         Qt::DirectConnection);
  // connect(timer_, &QTimer::timeout, this,
  // &VLFDDeviceDetector::onTimerTimeOut); connect(thread_, &QThread::started,
  // timer_, QOverload<>::of(&QTimer::start)); connect(thread_,
  // &QThread::finished, timer_, &QTimer::stop); connect(thread_,
  // &QThread::finished, this, &QObject::deleteLater());
  time_thread_ =
      new TimeThreadWorker(new DetectWorker(this), this, Qt::QueuedConnection);
  time_thread_->setInterval(kTimerInterval);
}

VLFDDeviceDetector::~VLFDDeviceDetector() {
  // stopDetect();
  // if (timer_ != nullptr && timer_->isActive()) {
  //   timer_->stop();
  // }
  // timer_->deleteLater();
  // if (thread_->isRunning()) {
  //   thread_->quit();
  // }
  // thread_->wait();
  // // thread_->deleteLater();
  // delete thread_;
}

void VLFDDeviceDetector::startDetect() {
  if (is_detecting_) {
    return;
  }
  device_connected_ = deviceExists();
  detectArrived(device_connected_);
  detectLeft(!device_connected_);
  is_detecting_ = true;
  // this->moveToThread(thread_);
  // thread_->start();
  // timer_->start();
  time_thread_->start();
}

void VLFDDeviceDetector::stopDetect() {
  if (!is_detecting_) {
    return;
  }
  is_detecting_ = false;
  // this->moveToThread(QApplication::instance()->thread());
  // timer_->stop();
  time_thread_->stop();
  // thread_->quit();
}

void VLFDDeviceDetector::detectArrived(bool arrived) {
  if (arrived) {
    device_connected_ = true;
    // qDebug() << "Device connected";
    emit deviceDetected();
  }
}
void VLFDDeviceDetector::detectLeft(bool left) {
  if (left) {
    device_connected_ = false;
    // qDebug() << "Device disconnected";
    emit deviceRemoved();
  }
}

bool VLFDDeviceDetector::deviceExists() {
  VlfdDevice *device = vlfd_io_open();
  if (device == nullptr) {
    return false;
  }

  if (vlfd_io_close(device) != 0) {
    const char *msg = vlfd_get_last_error_message();
    qWarning() << "Failed to close VLFD device after probe:" << (msg ? msg : "");
  }

  return true;
}

VlfdHotplugDeviceDetector::VlfdHotplugDeviceDetector(QObject *parent)
    : VLFDDeviceDetector(parent), registration_(nullptr),
      dispatch_events_(false) {}

VlfdHotplugDeviceDetector::~VlfdHotplugDeviceDetector() {
  stopDetect();
}

void VlfdHotplugDeviceDetector::startDetect() {
  if (registration_ == nullptr) {
    VlfdHotplugOptions options = vlfd_hotplug_options_default();
    options.filter_vendor_id = true;
    options.vendor_id = kVendorID;
    options.filter_product_id = true;
    options.product_id = kProductID;
    options.enumerate_existing = false;

    registration_ = vlfd_hotplug_register(
        &options, &VlfdHotplugDeviceDetector::hotplugCallback, this);
    if (registration_ == nullptr) {
      const char *msg = vlfd_get_last_error_message();
      QString error = tr("Failed to register VLFD hotplug callback");
      if (msg != nullptr && std::strlen(msg) > 0) {
        error += tr(": %1").arg(QString::fromUtf8(msg));
      }
      throw(error);
    }
  }

  VLFDDeviceDetector::startDetect();
  dispatch_events_.store(true, std::memory_order_release);
}

void VlfdHotplugDeviceDetector::stopDetect() {
  dispatch_events_.store(false, std::memory_order_release);

  VLFDDeviceDetector::stopDetect();

  if (registration_ != nullptr) {
    if (vlfd_hotplug_unregister(registration_) != 0) {
      const char *msg = vlfd_get_last_error_message();
      qWarning() << "Failed to unregister VLFD hotplug callback:" << (msg ? msg : "");
    }
    registration_ = nullptr;
  }
}

void VlfdHotplugDeviceDetector::onTimerTimeOut() {}

void VlfdHotplugDeviceDetector::hotplugCallback(
    void *user_data, const VlfdHotplugEvent *event) {
  auto detector = static_cast<VlfdHotplugDeviceDetector *>(user_data);
  if (detector == nullptr) {
    return;
  }
  detector->handleHotplugEvent(event);
}

void VlfdHotplugDeviceDetector::handleHotplugEvent(
    const VlfdHotplugEvent *event) {
  if (event == nullptr) {
    return;
  }

  if (!dispatch_events_.load(std::memory_order_acquire)) {
    return;
  }

  switch (event->kind) {
  case Arrived:
    detectArrived(true);
    break;
  case Left:
    detectLeft(true);
    break;
  default:
    break;
  }
}

#ifdef _WIN32
#include <dbt.h>
#include <initguid.h>
#include <usbiodef.h>
#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

WinVLFDDeviceDetector::WinVLFDDeviceDetector(QObject *parent)
    : VLFDDeviceDetector(parent) {
  hWnd = NULL;
  ZeroMemory(&wx, sizeof(wx));

  wx.cbSize = sizeof(WNDCLASSEX);
  wx.lpfnWndProc = reinterpret_cast<WNDPROC>(message_handler);
  wx.hInstance = reinterpret_cast<HINSTANCE>(GetModuleHandle(0));
  wx.style = CS_HREDRAW | CS_VREDRAW;
  wx.hInstance = GetModuleHandle(0);
  wx.hbrBackground = (HBRUSH)(COLOR_WINDOW);
  wx.lpszClassName = "DUMMY_CLASS";
  if (RegisterClassEx(&wx)) {
    hWnd = CreateWindow("DUMMY_CLASS", "DevNotifWnd", WS_ICONIC, 0, 0,
                        CW_USEDEFAULT, 0, ((HWND)-3), NULL, GetModuleHandle(0),
                        (void *)&guid);
  }
}

WinVLFDDeviceDetector::~WinVLFDDeviceDetector() {
  stopDetect();
  DestroyWindow(hWnd);
  UnregisterClass("DUMMY_CLASS", GetModuleHandle(0));
}

void WinVLFDDeviceDetector::onTimerTimeOut() {
  // qDebug() << "WinVLFDDeviceDetector thread: " << QThread::currentThreadId();
  MSG msg;
  while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

LRESULT CALLBACK WinVLFDDeviceDetector::message_handler(HWND__ *hwnd, UINT uint,
                                                        WPARAM wparam,
                                                        LPARAM lparam) {
  switch (uint) {
  case WM_CREATE: // the actual creation of the window
  {
    LPCREATESTRUCT params = (LPCREATESTRUCT)lparam;
    GUID InterfaceClassGuid = *((GUID *)params->lpCreateParams);
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    memcpy(&(NotificationFilter.dbcc_classguid),
           &(GUID_DEVINTERFACE_USB_DEVICE), sizeof(struct _GUID));
    HDEVNOTIFY dev_notify = RegisterDeviceNotification(
        hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (dev_notify == NULL) {
      throw std::runtime_error("Could not register for devicenotifications!");
    }
    break;
  }
  case WM_DEVICECHANGE: {
    PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lparam;
    PDEV_BROADCAST_DEVICEINTERFACE lpdbv = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
    if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
      VLFDDeviceDetector::instance()->detectArrived(wparam ==
                                                    DBT_DEVICEARRIVAL);
      VLFDDeviceDetector::instance()->detectLeft(wparam ==
                                                 DBT_DEVICEREMOVECOMPLETE);
    }
    break;
  }
  default:
    return true;
  }
  return 0L;
}

#endif // ifdef _WIN32

DetectWorker::DetectWorker(VLFDDeviceDetector *detector)
    : detector_(detector) {}

DetectWorker::~DetectWorker() {}

void DetectWorker::doWork() {
  // qDebug() << "DetectWorker::doWork() thread: " <<
  // QThread::currentThreadId();
  // QMutexLocker locker(&mutex_);
  detector_->onTimerTimeOut();
}

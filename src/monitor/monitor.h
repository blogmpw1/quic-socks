#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTableWidget>

#include "tunnel/http_proxy.h"
#include "ui_monitor.h"

namespace socks {

class UiMonitor {
 public:
  void Setup(QMainWindow *window);

  void AppendTable(const std::string &src, const std::string &dst, size_t len);

  QTableWidget *table;
};

class Monitor : public QMainWindow, public NetworkObserver {
  Q_OBJECT

 public:
  Monitor(QWidget *parent = Q_NULLPTR);

 private:
  void Connect(size_t idx, asio::ip::tcp::endpoint src,
               asio::ip::tcp::endpoint dst, std::string host) override;
  void Forward(size_t idx, bool outside, std::string data) override;
  void Disconnect(size_t idx) override;

  UiMonitor ui;
  std::shared_ptr<tunnel::HttpProxy> proxy_;
};

}  // namespace socks

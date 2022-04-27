#include "monitor.h"

#include <QtWidgets/QHeaderView>

namespace socks {

Monitor::Monitor(QWidget *parent)
    : QMainWindow(parent), proxy_{tunnel::HttpProxy::Create(8999)} {
  ui.Setup(this);
  proxy_->Register(this);
  proxy_->Start();

  ui.AppendTable("192.0.0.1", "8.8.8.8", 52);
}

void Monitor::Connect(size_t idx, asio::ip::tcp::endpoint src,
                      asio::ip::tcp::endpoint dst, std::string host) {}

void Monitor::Forward(size_t idx, bool outside, std::string data) {
  ui.AppendTable(std::to_string(idx), std::to_string(int(outside)),
                 data.size());
}

void Monitor::Disconnect(size_t idx) {}

void UiMonitor::Setup(QMainWindow *window) {
  window->setObjectName(QStringLiteral("Monitor"));
  window->setWindowTitle("Monitor");
  window->resize(800, 600);

  auto *widget = new QWidget(window);
  window->setCentralWidget(widget);

  auto *layout = new QVBoxLayout(widget);
  layout->setMargin(0);

  auto *button = new QPushButton(widget);
  button->setText("Push");
  layout->addWidget(button);

  table = new QTableWidget(widget);
  layout->addWidget(table);

  table->setColumnCount(3);
  table->setHorizontalHeaderLabels({"src", "dst", "len"});
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void UiMonitor::AppendTable(const std::string &src, const std::string &dst,
                            size_t len) {
  table->insertRow(table->rowCount());
  auto row_len = table->rowCount();
  table->setItem(row_len - 1, 0,
                 new QTableWidgetItem(QString::fromStdString(src)));
  table->setItem(row_len - 1, 1,
                 new QTableWidgetItem(QString::fromStdString(dst)));
  table->setItem(row_len - 1, 2, new QTableWidgetItem(QString::number(len)));
}

}  // namespace socks

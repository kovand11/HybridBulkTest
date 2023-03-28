#include "mainwindow.h"
#include "QDebug"
#include "QTimer"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->loadStartStopPushButton,&QPushButton::clicked,this,&MainWindow::onStartStopLoadPushButtonClicked);

    loaderThreadRun = false;
    writeThreadRun = false;


    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [&]()
    {
        ui->bufferSizeLabel->setText(QString::number(this->totalQueuedRecords));
        ui->progressBar->setValue(this->totalQueuedRecords/100000);
    });
    timer->start(100);
}

void MainWindow::onStartStopLoadPushButtonClicked(bool checked)
{
    if (this->loaderThreadRun)
    {
        loaderThreadRun = false;
        loaderThread->join();
        delete loaderThread;
    }
    else
    {
        loaderThreadRun = true;
        loaderThread = new std::thread([&](){
            while(loaderThreadRun)
            {
                if (this->totalQueuedRecords <= 10000000)
                {
                    this->buffer.push(RandomString(20));
                    this->totalQueuedRecords++;
                }
                else
                {
                    ui->alarmLabel->setText("overload");
                }

            }
        });
    }
}


std::string MainWindow::RandomString(const int len)
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

MainWindow::~MainWindow()
{
    delete ui;
}


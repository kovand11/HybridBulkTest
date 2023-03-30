#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include<queue>
#include<thread>
#include<mutex>

#include "QTimer"

#include <QMainWindow>

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <odbcss.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

//Utility function
private:
    std::string RandomString(const int len);

    void ConnectMSSQL();

    void DisplayWarning(QString errorText);

    void ExecuteDirectMSSQL(HDBC hdbc, std::string command);

//For mssql storage simulation
private:



    uint64_t loadPerInterval;
    uint64_t intervalMs;
    uint64_t bufferMaxSize;

    std::mutex bufferMutex;
    std::queue<std::string> buffer;

    std::atomic_uint64_t totalQueuedRecords;
    std::atomic_uint64_t totalPersistedRecords;

    std::string server;
    std::string databaseName;


    bool isloaderTimerRunning;
    QTimer *loaderTimer;

    std::thread *writeThread;
    std::atomic_bool isWriteThreadRunning;

private slots:
    void onStartStopLoadPushButtonClicked(bool checked);
    void onStartStopWritePushButtonClicked(bool checked);
    void onLoaderTimerInterval();

};
#endif // MAINWINDOW_H

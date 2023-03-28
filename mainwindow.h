#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include<queue>
#include<thread>

#include <QMainWindow>

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

//For mssql storage simulation
private:
    std::queue<std::string> buffer;

    std::atomic_uint64_t totalQueuedRecords;
    std::atomic_uint64_t totalPersistedRecords;


    std::thread *loaderThread;
    std::atomic_bool loaderThreadRun;
    std::thread *writeThread;
    std::atomic_bool writeThreadRun;

private slots:
    void onStartStopLoadPushButtonClicked(bool checked);

};
#endif // MAINWINDOW_H

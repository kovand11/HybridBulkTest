#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "QDebug"
#include "QMessageBox"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->startStopLoadPushButton,&QPushButton::clicked,this,&MainWindow::onStartStopLoadPushButtonClicked);
    connect(ui->startStopWritePushButton,&QPushButton::clicked,this,&MainWindow::onStartStopWritePushButtonClicked);

    isloaderTimerRunning = false;
    isWriteThreadRunning = false;

    //Applying defaults
    static constexpr uint64_t DEFAULT_LOAD_PER_INTERVAL = 100;
    static constexpr uint64_t DEFAULT_INTERVAL_MS = 100;
    static constexpr uint64_t DEFAULT_BUFFER_MAX_SIZE = 50000;
    static std::string DEFAULT_SERVER = "(local)";
    static std::string DEFAULT_DATABASE_NAME = "MyDatabase";

    ui->loadPerIntervalLineEdit->setText(QString::number(DEFAULT_LOAD_PER_INTERVAL));
    ui->intervalMsLineEdit->setText(QString::number(DEFAULT_INTERVAL_MS));
    ui->bufferSizeLineEdit->setText(QString::number(DEFAULT_BUFFER_MAX_SIZE));
    ui->serverLineEdit->setText(QString::fromStdString(DEFAULT_SERVER));
    ui->databaseNameLineEdit->setText(QString::fromStdString(DEFAULT_DATABASE_NAME));

    this->loadPerInterval = DEFAULT_LOAD_PER_INTERVAL;
    this->intervalMs = DEFAULT_INTERVAL_MS;
    this->bufferMaxSize = DEFAULT_BUFFER_MAX_SIZE;
    this->server = DEFAULT_SERVER;
    this->databaseName = DEFAULT_DATABASE_NAME;

    loaderTimer = new QTimer(this);
    connect(loaderTimer, &QTimer::timeout, this, &MainWindow::onLoaderTimerInterval);


    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [&]()
    {
        std::unique_lock<std::mutex> bufferLock(this->bufferMutex);
        auto bufferSize = this->buffer.size();
        bufferLock.unlock();
        ui->bufferSizeLabel->setText(QString::number(bufferSize));
        ui->progressBar->setValue(100*bufferSize/bufferMaxSize);
    });
    timer->start(100);

    this->ConnectMSSQL();
}

void MainWindow::onStartStopLoadPushButtonClicked(bool checked)
{
    if (this->isloaderTimerRunning)
    {
        loaderTimer->stop();
        this->isloaderTimerRunning = false;
        ui->startStopLoadPushButton->setText("Start Load");
    }
    else
    {
        loaderTimer->start(this->intervalMs);
        this->isloaderTimerRunning = true;
        ui->startStopLoadPushButton->setText("Stop Load");
    }
}

void MainWindow::onStartStopWritePushButtonClicked(bool checked)
{
    if (this->isWriteThreadRunning)
    {
        this->isWriteThreadRunning = false;

        if (this->writeThread->joinable())
        {
            this->writeThread->join();
            delete writeThread;
        }

        ui->startStopWritePushButton->setText("Start Write");
    }
    else
    {
        this->isWriteThreadRunning = true;
        this->writeThread = new std::thread([&](){
            while(this->isWriteThreadRunning)
            {
                std::unique_lock<std::mutex> bufferLock(this->bufferMutex);
                if (this->buffer.size() > 0)
                {
                    qDebug() << "pre pop " << this->buffer.size();
                    this->buffer.pop();
                    qDebug() << "post pop ";
                }
                bufferLock.unlock();
                //write time simulation
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        ui->startStopWritePushButton->setText("Stop Write");
    }
}

void MainWindow::onLoaderTimerInterval()
{
    for (uint64_t i = 0; i < this->loadPerInterval; i++)
    {
        std::unique_lock<std::mutex> bufferLock(this->bufferMutex);
        if (buffer.size() < this->bufferMaxSize)
        {

            this->buffer.push(RandomString(20));
            this->totalQueuedRecords++;
        }
        else
        {
            ui->alarmLabel->setText("overload");
        }
        bufferLock.unlock();
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

void MainWindow::ConnectMSSQL()
{
    RETCODE retcode;

    SQLHENV henv = SQL_NULL_HENV;
    HDBC hdbc1 = SQL_NULL_HDBC, hdbc2 = SQL_NULL_HDBC;
    SQLHSTMT hstmt2 = SQL_NULL_HSTMT;

    // BCP variables.
    const char *terminator = "\0";

    // bcp_done takes a different format return code because it returns number of rows bulk copied
    // after the last bcp_batch call.
    DBINT cRowsDone = 0;

    // Set up separate return code for bcp_sendrow so it is not using the same retcode as SQLFetch.
    RETCODE SendRet;

    // Column variables.  cbCola and cbColb must be defined right before Cola and szColb because
    // they are used as bulk copy indicator variables.
    struct ColaData
    {
        int cbCola;
        SQLINTEGER Cola;
    } ColaInst;

    struct ColbData
    {
        int cbColb;
        SQLCHAR szColb[11];
    } ColbInst;

   // Allocate the ODBC environment and save handle.
   retcode = SQLAllocHandle (SQL_HANDLE_ENV, NULL, &henv);
   if ( (retcode != SQL_SUCCESS_WITH_INFO) && (retcode != SQL_SUCCESS))
   {
      this->DisplayWarning("SQLAllocHandle(Env) Failed");
      //Cleanup();
      return;
   }

   // Notify ODBC that this is an ODBC 3.0 app.
    retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_INTEGER);
    if ( (retcode != SQL_SUCCESS_WITH_INFO) && (retcode != SQL_SUCCESS))
    {
        this->DisplayWarning("SQLSetEnvAttr(ODBC version) Failed");
        //Cleanup();
        return;
    }

    // Allocate ODBC connection handle, set bulk copy mode, and connect.
    retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1);
    if ( (retcode != SQL_SUCCESS_WITH_INFO) && (retcode != SQL_SUCCESS))
    {
        this->DisplayWarning("SQLAllocHandle(hdbc1) Failed");
        //Cleanup();
        return;
    }

    retcode = SQLSetConnectAttr(hdbc1, SQL_COPT_SS_BCP, (void *)SQL_BCP_ON, SQL_IS_INTEGER);
    if ( (retcode != SQL_SUCCESS_WITH_INFO) && (retcode != SQL_SUCCESS))
    {
        this->DisplayWarning("SQLSetConnectAttr(hdbc1) Failed");
        //Cleanup();
        return;
    }

    // sample uses Integrated Security, create the SQL Server DSN using Windows NT authentication
    QString server = "(local)";//ui->serverLineEdit->text();
    QString connectionString = "driver=ODBC Driver 17 for SQL Server;server=" + server + ";database=MyDatabase;trusted_connection=yes";
    SQLCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen = 0;
    retcode = SQLDriverConnectA(hdbc1,
                                nullptr,
                                (SQLCHAR*)connectionString.toStdString().c_str(),
                                connectionString.length(),
                                outConnStr,
                                1024,
                                &outConnStrLen,
                                SQL_DRIVER_NOPROMPT);

    if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO))
    {
        //SQL_Disconnect();
        this->DisplayWarning("SQLDriverConnect() Failed");
        return;
    }

    /*// Initialize the bulk copy.
    retcode = bcp_init(hdbc1, (SQLWCHAR*)"AdventureWorks..BCPTarget", NULL, NULL, DB_IN);
    if ( (retcode != SUCCEED) ) {
       this->DisplayWarning("bcp_init(hdbc1) Failed\n\n");
       //Cleanup();
       return;
    }*/

    std::string tableDropStatement = "IF EXISTS (SELECT name FROM sysobjects WHERE name = 'MyTable')"
                                     " DROP TABLE MyTable";

    this->ExecuteDirectMSSQL(hdbc1, tableDropStatement);
}

void MainWindow::DisplayWarning(QString warningText)
{
    QMessageBox::warning(this, "Warning", warningText);
}

void MainWindow::ExecuteDirectMSSQL(HDBC hdbc, std::string command)
{
    RETCODE retcode;
    SQLHSTMT hstmt = SQL_NULL_HSTMT;

    retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if ((retcode != SQL_SUCCESS_WITH_INFO) && (retcode != SQL_SUCCESS))
    {
        this->DisplayWarning(QString::fromStdString("SQLAllocHandle() failed for command: " + command));
        return;
    }

    retcode = SQLExecDirectA(hstmt, (UCHAR*)command.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO))
    {
        this->DisplayWarning(QString::fromStdString("SQLExecDirectA() failed for command: " + command));
    }

    if (hstmt != SQL_NULL_HSTMT)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    }
}

MainWindow::~MainWindow()
{
    delete loaderTimer;
    delete ui;
}


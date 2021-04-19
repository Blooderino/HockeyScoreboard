#include "ServerWindow.h"
#include "ui_ServerWindow.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QtCore/QDebug>

// Конструктор класса
ServerWindow::ServerWindow(bool debug, quint16 port, QWidget* parent):
    QMainWindow(parent), currentPort(QString::number(port)), ui(new Ui::ServerWindow),
    connectionWebSocketServer(new QWebSocketServer(ServerWindow::SEARCH_CLIENT, QWebSocketServer::NonSecureMode, this))
{
    this->debug = debug;

    this->threadPeriodTime = nullptr;
    this->threadCurrentTime = nullptr;

    this->oldTime = this->getCurrentTime();

    this->toConsole("Запуск графической оболочки.");

    this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_FINISH;

    this->ui->setupUi(this);
    connect(this->ui->usersListWidget, &QListWidget::itemClicked, this, &ServerWindow::eventUsersListWidgetItemClicked);

    this->toConsole("Проверка инициализации веб-сокета сервера...");

    // Если веб-сокет сервера слушает, то связываются события обработки нового подключения к серверу, а также делает сервер доступным для обнаружения
    if (this->connectionWebSocketServer->listen(QHostAddress::Any, port))
    {
        this->toConsole("Веб-сокет инициализирован.");
        this->toConsole("Подключение UDP-сокета...");

        connect(this->connectionWebSocketServer, &QWebSocketServer::newConnection, this, &ServerWindow::eventClientSocketConnected);
        this->searchUdpSocket.bind(ServerWindow::PORT_UDP_CLIENT, QUdpSocket::ShareAddress);
        connect(&this->searchUdpSocket, &QUdpSocket::readyRead, this, &ServerWindow::eventSearchServer);

        this->toConsole("UDP-сокет покдлючен.");
    }

    this->toConsole("Список всех доступных локальных IPv4:");

    // Заполняем список всех доступных локальных IPv4 адресов
    for (const QHostAddress& address: QNetworkInterface::allAddresses())
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost)
        {
            this->toConsole("Доступен адрес: " + address.toString() + ":" + this->currentPort);

            this->ui->addressesListWidget->addItem(address.toString() + ":" + this->currentPort);
        }

    // Генерация конца ключа шифрования (состоит из года, месяца, дня, часа, минуты, секунды и милисекунды запуска сервера)
    this->startDateTime = this->getCurrentDateTime();

    // Открытие локального веб-сокета сервера
    this->serverWebSocket = new QWebSocket();
    this->serverWebSocket->open(QUrl("ws://" + this->connectionWebSocketServer->serverAddress().toString() + ":" +
                                     this->currentPort + "/?" + ServerWindow::SENDER_SERVER));

    this->searchUdpSocket.writeDatagram(QString(ServerWindow::SEARCH_CLIENT + ServerWindow::MESSAGE_SEPARATOR + ServerWindow::SEARCH_SERVER).toUtf8(),
                                        QHostAddress::Broadcast, ServerWindow::PORT_UDP_SERVER);

    // Создание потоков времени
    this->threadCurrentTime = QThread::create([this] { workThreadCurrentTime(); });
    this->threadPeriodTime = nullptr;
}

// Деструктор класса
ServerWindow::~ServerWindow()
{
    QString message = "Сервер был отключен.";

    this->toConsole(message);

    this->ui->logPlainTextEdit->appendHtml("<font color=\"red\">" + message + "</font>");

    // Отправляем сообщение об подключении нового пользователя
    this->toConsole("Закрытие подключеных сокетов окна.");

    for (int i = 0; i < this->clientsWebSockets.size(); i++)
    {
        this->toConsole("Пользователь " + this->clientsWebSockets[i].second + " отключен.");

        this->clientsWebSockets[i].first->close();
    }

    this->clientsWebSockets.clear();

    this->toConsole("Отключение веб-сервера.");

    this->connectionWebSocketServer->close();
    this->connectionWebSocketServer->deleteLater();

    QString fullPath = QCoreApplication::applicationDirPath() + "/" + ServerWindow::CONFIG_FILE_NAME;

    // Сохранение параметров
    QSettings settings(fullPath, QSettings::Format::IniFormat);
    settings.beginGroup(ServerWindow::CONFIG_SAVED_GAME);
    settings.setValue(ServerWindow::CONFIG_PERIOD_NUMBER, QString::number(this->periodNumber));
    settings.setValue(ServerWindow::CONFIG_PERIOD_TIME_MINUTES, QString::number(this->periodTime.first));
    settings.setValue(ServerWindow::CONFIG_PERIOD_TIME_SECONDS, QString::number(this->periodTime.second));
    settings.setValue(ServerWindow::CONFIG_FIRST_NAME, this->firstTeamName);
    settings.setValue(ServerWindow::CONFIG_FIRST_SCORE, QString::number(this->firstTeamScore));
    settings.setValue(ServerWindow::CONFIG_SECOND_NAME, this->secondTeamName);
    settings.setValue(ServerWindow::CONFIG_SECOND_SCORE, QString::number(this->secondTeamScore));
    settings.setValue(ServerWindow::CONFIG_GAME_STATUS, this->gameStatus);
    settings.sync();
    settings.endGroup();

    // Очищаем поток обновления времени периода
    if (this->threadPeriodTime != nullptr)
    {
        if (this->threadPeriodTime->isRunning())
        {
            this->toConsole("Поток обновления времени периода работает.");

            this->threadPeriodTime->requestInterruption();
            this->threadPeriodTime->exit();

            if (!this->threadPeriodTime->wait(5000))
            {
                this->toConsole("Не удается закрыть поток обновления времени периода, закрыть принудительно.");

                this->threadPeriodTime->terminate();
                this->threadPeriodTime->wait();
            }

            this->toConsole("Поток обновления времени периода успешно закрыт.");
        }

        this->toConsole("Очищение потока обновления времени периода.");

        this->threadPeriodTime->deleteLater();
    }

    this->toConsole("Закрытие потока обновления текущего времени.");

    // Очищаем поток обновления текущего времени
    if (this->threadCurrentTime != nullptr)
    {
        if (this->threadCurrentTime->isRunning())
        {
            this->toConsole("Поток обновления текущего времени работает.");

            this->threadCurrentTime->requestInterruption();
            this->threadCurrentTime->exit();

            if (!this->threadCurrentTime->wait(5000))
            {
                this->toConsole("Не удается закрыть поток обновления текущего времени, закрыть принудительно.");

                this->threadCurrentTime->terminate();
                this->threadPeriodTime->wait();
            }
        }

        this->toConsole("Очищение потока обновления текущего времени.");

        this->threadCurrentTime->deleteLater();
    }

    this->toConsole("Закрытие графического окна.");

    this->toConsole("Сохранение параметров: " + fullPath);

    delete this->ui;
}

// Стандартный ответ клиента при поиске сервера
const QString ServerWindow::SEARCH_CLIENT = "LANCHAT_SEARCH_CLIENT";

// Стандартный ответ сервера клиенту
const QString ServerWindow::SEARCH_SERVER = "LANCHAT_SEARCH_SERVER";

// Стандартный разделитель в ответе от сервера
const QString ServerWindow::MESSAGE_SEPARATOR = "[|!|]";

// Имя пользователя, если получатель - все пользователи
const QString ServerWindow::RECIEVER_ALL = "RECIEVER_ALL";

// Имя пользователя, если отправитель - сервер
const QString ServerWindow::SENDER_SERVER = "SENDER_SERVER";

// Отправитель сообщения клиент
const QString ServerWindow::SENDER_CLIENT = "SENDER_CLIENT";

// Сервер отключен
const QString ServerWindow::SERVER_DISCONNECTED = "SERVER_DISCONNECTED";

// Статус игры
const QString ServerWindow::GAME_SATUS = "GAME_STATUS";

// Запуск игры
const QString ServerWindow::GAME_START = "GAME_START";

// Окончание игры
const QString ServerWindow::GAME_STOP = "GAME_STOP";

// Приостановка игры
const QString ServerWindow::GAME_PAUSE = "GAME_PAUSE";

// Продолжение игры
const QString ServerWindow::GAME_CONTINUE = "GAME_CONTINUE";

// Текущий период
const QString ServerWindow::PERIOD_CURRENT = "PERIOD_CURRENT";

// Переключение на предыдущий период
const QString ServerWindow::PERIOD_PREVIOUS = "PERIOD_PREVIOUS";

// Переключение на следующий период
const QString ServerWindow::PERIOD_NEXT = "PERIOD_NEXT";

// Время периода
const QString ServerWindow::PERIOD_TIME = "PERIOD_TIME";

// Текущее время
const QString ServerWindow::CURRENT_TIME = "CURRENT_TIME";

// Вернуть очки для первой команды
const QString ServerWindow::SCORE_GET_FIRST = "SCORE_GET_FIRST";

// Вернуть очки для второй команды
const QString ServerWindow::SCORE_GET_SECOND = "SCORE_GET_SECOND";

// Добавить очки для первой команды
const QString ServerWindow::SCORE_ADD_FIRST = "SCORE_ADD_FIRST";

// Добавить очки для второй команды
const QString ServerWindow::SCORE_ADD_SECOND = "SCORE_ADD_SECOND";

// Отнять очки у первой команды
const QString ServerWindow::SCORE_SUBTRACT_FIRST = "SCORE_SUBTRACT_FIRST";

// Отнять очки у второй команды
const QString ServerWindow::SCORE_SUBTRACT_SECOND = "SCORE_SUBTRACT_SECOND";

// Получить имя первой команды
const QString ServerWindow::NAME_FIRST = "NAME_FIRST";

// Получить имя второй команды
const QString ServerWindow::NAME_SECOND = "NAME_SECOND";

// Отправитель сообщения поток обновления текущего времени
const QString ServerWindow::SENDER_THREAD_CURRENT_TIME = "SENDER_THREAD_CURRENT_TIME";

// Отправитель сообщения потоко обновления времени периода
const QString ServerWindow::SENDER_THREAD_PERIOD_TIME = "SENDER_THREAD_PERIOD_TIME";

// Стандартное имя первой команды
const QString ServerWindow::NAME_FIRST_DEFAULT = "Team1";

// Стандартное имя второй команды
const QString ServerWindow::NAME_SECOND_DEFAULT = "Team2";

// Стандартное имя для конфигурационного файла
const QString ServerWindow::CONFIG_FILE_NAME = "setting.ini";

// Стандартное имя для поля с сохраненной игрой
const QString ServerWindow::CONFIG_SAVED_GAME = "SavedGame";

// Стандартное имя поля для номера периода
const QString ServerWindow::CONFIG_PERIOD_NUMBER = "period.number";

// Стандартное имя поля для минут периода
const QString ServerWindow::CONFIG_PERIOD_TIME_MINUTES = "period.time.minutes";

// Стандартное имя поля для секунд периода
const QString ServerWindow::CONFIG_PERIOD_TIME_SECONDS = "period.time.seconds";

// Станадртное имя поля для имени первой команды
const QString ServerWindow::CONFIG_FIRST_NAME = "first.name";

// Стандартное имя поля для количества очков первой команды
const QString ServerWindow::CONFIG_FIRST_SCORE = "first.score";

// Станадртное имя поля для имени второй команды
const QString ServerWindow::CONFIG_SECOND_NAME = "second.name";

// Стандартное имя поля для количества очков второй команды
const QString ServerWindow::CONFIG_SECOND_SCORE = "second.score";

// Стандартное имя поля для статуса игры
const QString ServerWindow::CONFIG_GAME_STATUS = "game.status";

// Возвращает найденный веб-сокет клиента
QWebSocket* ServerWindow::getClientWebSocketByIPDateTime(const QString& ipDateTime)
{
    QWebSocket* foundClientWebSocket = nullptr;

    for (int i = 0; i < this->clientsWebSockets.size() && foundClientWebSocket == nullptr; i++)
        if (this->clientsWebSockets[i].second == ipDateTime)
            foundClientWebSocket = this->clientsWebSockets[i].first;

    return foundClientWebSocket;
}

// Возвращает индекс веб-сокета клиента в общем списке
int ServerWindow::getIndexOfClientWebSocket(QWebSocket* clientWebSocket)
{
    int foundIndex = -1;

    for (int i = 0; i < this->clientsWebSockets.size() && foundIndex < 0; i++)
        if (this->clientsWebSockets[i].first == clientWebSocket)
            foundIndex = i;

    return foundIndex;
}

// Возвращает адрес сервера, который находится в той же под сети
QString ServerWindow::findServerAddress(const QString& address)
{
    QString foundAddress = "";

    // Разбиваем полученный адрес на 4 части
    QStringList addressBytes = address.split(".");

    // Если получилось 4 байта, то адресс указан в корректном формате
    if (addressBytes.size() == 4)
        for (int i = 0; i < this->ui->addressesListWidget->count() && foundAddress.size() == 0; i++)
        {
            // Разбиваем взятый из списка адрес
            QStringList foundBytes = this->ui->addressesListWidget->item(i)->text().split(".");

            // Если получилось 4 байта, то адрес указан в корректном формате
            if (foundBytes.size() == 4)
                // Если первые 3 байта адреса клиента и сервера совпали, то они находятся в одной сети, запоминаем адрес сервера
                if (addressBytes[0] == foundBytes[0] && addressBytes[1] == foundBytes[1] && addressBytes[2] == foundBytes[2])
                {
                    foundAddress = this->ui->addressesListWidget->item(i)->text();
                    foundAddress = foundAddress.mid(0, foundAddress.indexOf(":"));
                }
        }

    return foundAddress;
}

// Выводит сообщение в консоль
void ServerWindow::toConsole(const QString &text)
{
    if (this->debug)
        qDebug() << "[" << this->metaObject()->className() << "]: "  << text;
}

// Возвращает текущие даут и время
QString ServerWindow::getCurrentDateTime(const QString& separator)
{
    return QString::number(QDate::currentDate().day()) + "." +
           QString::number(QDate::currentDate().month()) + "." +
           QString::number(QDate::currentDate().year()) + separator +
           QString::number(QTime::currentTime().hour()) + "-" +
           QString::number(QTime::currentTime().minute()) + "-" +
           QString::number(QTime::currentTime().second()) + "-" +
           QString::number(QTime::currentTime().msec());
}

// Вовзаращает время, сконвертированное в строку
QString ServerWindow::getTime(quint16 big, quint16 small)
{
    QString bigger = big > 9 ? QString::number(big) : "0" +  QString::number(big);
    QString smaller = small > 9 ? QString::number(small) : "0" + QString::number(small);

    return bigger + ":" + smaller;
}

// Возвращает текущее время в часах и минутах
QString ServerWindow::getCurrentTime()
{
    return this->getTime(QTime::currentTime().hour(), QTime::currentTime().minute());
}

// Выполняет обновление текущего времени
void ServerWindow::workThreadCurrentTime()
{
    this->toConsole("Запущен поток получения текущего времени сервера.");

    // Поток будет активен, пока не будет закрыт
    while (!this->threadCurrentTime->isInterruptionRequested())
    {
        emit this->eventServerSocketSendTextMessage(ServerWindow::SENDER_CLIENT + ServerWindow::MESSAGE_SEPARATOR + ServerWindow::CURRENT_TIME);

        QThread::sleep(1);
    }
}

// Выполняет обновление времени периода
void ServerWindow::workThreadPeriodTime()
{
    this->toConsole("Запущен поток получения текущего времени периода.");

    // Поток активен
    while (this->threadPeriodTimeMode)
    {
        // Поток не спит
        if (this->threadPeriodTimeMode == ServerWindow::THREAD_PERIOD_RUN && !this->threadPeriodTime->isInterruptionRequested())
        {
            // Ожидание 1 секунды
            QThread::sleep(1);

            // Если минута не прошла, то секунда прибавляется
            if (this->periodTime.second < ServerWindow::PERIOD_TIME_MAX_SECONDS)
            {
                this->periodTime.second++;
            }
            // Иначе секунда обнуляется
            else
            {
                this->periodTime.second = 0;

                // Если минут прошло меньше 20, то количество минут прибавляется
                if (this->periodTime.first < ServerWindow::PERIOD_TIME_MAX_MINUTES)
                {
                    this->toConsole("Новая минута периода.");

                    this->periodTime.first++;
                }
                // Иначе минута обнуляется
                else
                {
                    this->toConsole("Период завершился.");

                    this->periodTime.first = 0;
                }
            }

             while (this->threadPeriodTimeMode == ServerWindow::THREAD_PERIOD_SLEEP)
             {
                 QThread::msleep(10);
             }

            // Посылка сообщения с новым временем периода
            emit this->eventServerSocketSendTextMessage(ServerWindow::SENDER_CLIENT + ServerWindow::MESSAGE_SEPARATOR + ServerWindow::PERIOD_TIME);

            // Если период закончился, то посылка сообщения о новом периоде
            if (this->periodTime.first == 0 && this->periodTime.second == 0)
            {
                if (this->periodNumber < ServerWindow::PERIOD_MAX)
                {
                    this->toConsole("Переключение на новый период.");

                    emit this->eventServerSocketSendTextMessage(ServerWindow::SENDER_CLIENT + ServerWindow::MESSAGE_SEPARATOR + ServerWindow::PERIOD_NEXT);
                }
                else
                {
                    this->toConsole("Игра завершается.");

                    this->threadPeriodTime->requestInterruption();
                    this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_FINISH;
                }
            }
        }
    }
}

// Выполняет первоначальную конфигурацию
void ServerWindow::setConfiguration()
{
    // Установка стандартных значений для отслеживаемых показателей
    this->gameStatus = ServerWindow::GAME_STOP;
    this->periodNumber = ServerWindow::PERIOD_MIN;
    this->periodTime.first = ServerWindow::PERIOD_TIME_MIN;
    this->periodTime.second = ServerWindow::PERIOD_TIME_MIN;
    this->oldPeriodTimeMinute = this->periodTime.first;
    this->firstTeamName = ServerWindow::NAME_FIRST_DEFAULT;
    this->firstTeamScore = ServerWindow::SCORE_MIN;
    this->secondTeamName = ServerWindow::NAME_SECOND_DEFAULT;
    this->secondTeamScore = ServerWindow::SCORE_MIN;

    if (QFile::exists(QCoreApplication::applicationDirPath() + "/" + ServerWindow::CONFIG_FILE_NAME))
    {
        this->toConsole("Конфигурационный файл существует.");

        QString
            configPeriodNumber = "",
            configPeriodTimeMinutes = "",
            configPeriodTimeSeconds = "",
            configFirstName = "",
            configFirstScore = "",
            configSecondName = "",
            configSecondScore = "",
            configGameStatus = "",
            empty = "";

        // Считываем все данные из конфигурационного файла
        QSettings settings(QCoreApplication::applicationDirPath() + "/" + ServerWindow::CONFIG_FILE_NAME, QSettings::Format::IniFormat);
        settings.beginGroup(ServerWindow::CONFIG_SAVED_GAME);
        configPeriodNumber = settings.value(ServerWindow::CONFIG_PERIOD_NUMBER, empty).toString();
        configPeriodTimeMinutes = settings.value(ServerWindow::CONFIG_PERIOD_TIME_MINUTES, empty).toString();
        configPeriodTimeSeconds = settings.value(ServerWindow::CONFIG_PERIOD_TIME_SECONDS, empty).toString();
        configFirstName = settings.value(ServerWindow::CONFIG_FIRST_NAME, empty).toString();
        configFirstScore = settings.value(ServerWindow::CONFIG_FIRST_SCORE, empty).toString();
        configSecondName = settings.value(ServerWindow::CONFIG_SECOND_NAME, empty).toString();
        configSecondScore = settings.value(ServerWindow::CONFIG_SECOND_SCORE, empty).toString();
        configGameStatus = settings.value(ServerWindow::CONFIG_GAME_STATUS, empty).toString();
        settings.sync();
        settings.endGroup();

        // Все поля были найдены
        if (configPeriodNumber.size() > 0 &&
            configPeriodTimeMinutes.size() > 0 &&
            configPeriodTimeSeconds.size() > 0 &&
            configFirstName.size() > 0 &&
            configFirstScore.size() > 0 &&
            configSecondName.size() > 0 &&
            configSecondScore.size() > 0 &&
            configGameStatus.size() > 0)
        {
            this->toConsole("Параметры найдены.");

            // Значения всех полей совпадают с допустимыми значениями
            if (configPeriodNumber.toInt() >= ServerWindow::PERIOD_MIN &&
                configPeriodNumber.toInt() <= ServerWindow::PERIOD_MAX &&
                configPeriodTimeMinutes.toInt() >= ServerWindow::PERIOD_TIME_MIN &&
                configPeriodTimeMinutes.toInt() <= ServerWindow::PERIOD_TIME_MAX_MINUTES &&
                configPeriodTimeSeconds.toInt() >= ServerWindow::PERIOD_TIME_MIN &&
                configPeriodTimeSeconds.toInt() <= ServerWindow::PERIOD_TIME_MAX_SECONDS &&
                this->isCorrectTeamName(configFirstName) &&
                this->isCorrectTeamName(configSecondName) &&
                configFirstScore.toInt() >= ServerWindow::SCORE_MIN &&
                configFirstScore.toInt() <= ServerWindow::SCORE_MAX &&
                configSecondScore.toInt() >= ServerWindow::SCORE_MIN &&
                configSecondScore.toInt() <= ServerWindow::SCORE_MAX &&
                (configGameStatus == ServerWindow::GAME_START ||
                 configGameStatus == ServerWindow::GAME_PAUSE ||
                 configGameStatus == ServerWindow::GAME_CONTINUE))
            {
                this->toConsole("Параметры имеют допустимые значения, записаны в память приложения.");

                this->gameStatus = configGameStatus;
                this->periodNumber = configPeriodNumber.toInt();
                this->periodTime.first = configPeriodTimeMinutes.toInt();
                this->periodTime.second = configPeriodTimeSeconds.toInt();
                this->oldPeriodTimeMinute = this->periodTime.first;
                this->firstTeamName = configFirstName;
                this->firstTeamScore = configFirstScore.toInt();
                this->secondTeamName = configSecondName;
                this->secondTeamScore = configSecondScore.toInt();

                // Запуск потока обновления времени периода
                if (this->threadPeriodTime == nullptr)
                {
                    this->toConsole("Запуск потока обновления времени периода.");

                    this->threadPeriodTime = QThread::create([this] { workThreadPeriodTime(); });

                    if (this->gameStatus == ServerWindow::GAME_PAUSE)
                        this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_SLEEP;
                    else
                        this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_RUN;

                    this->threadPeriodTime->start();
                }
            }
            else
                this->toConsole("Параметры имеют недопустимые значения, загружены значения по умолчанию.");
        }
        else
            this->toConsole("Не удалось найти параметры, загружены значения по умолчанию.");
    }
    else
        this->toConsole("Не удалось найти файл настроек, загружены значения по умолчанию.");
}

// Возвращает, введено ли корректное имя пользователя
bool ServerWindow::isCorrectTeamName(const QString& teamName)
{
    bool check = teamName.size() > 2 && teamName.size() < 17;


    for (int i = 0; i < teamName.size() && check; i++)
        check = ((teamName[i] >= 'a' && teamName[i] <= 'z') ||
                (teamName[i] >= 'A' && teamName[i] <= 'Z') ||
                (teamName[i] >= '0' && teamName[i] <= '9'));


    if (check)
        check = teamName != ServerWindow::SENDER_SERVER;

    return check;
}

// Событие при подключении веб-соекта нового клиента
void ServerWindow::eventClientSocketConnected()
{
    // Получение веб-сокета нового клиента
    QWebSocket* newClientWebSocket = this->connectionWebSocketServer->nextPendingConnection();

    this->toConsole("Веб-сокет подключен: " + newClientWebSocket->requestUrl().toString());

    // Отбрасываем URL сервера
    QStringList parameters = newClientWebSocket->requestUrl().toString().split("?");

    // Связываем событие получения сообщения
    connect(newClientWebSocket, &QWebSocket::textMessageReceived, this, &ServerWindow::eventClientTextMessageRecieved, Qt::QueuedConnection);

    if (parameters.size() == 2)
    {
        this->toConsole("Имя клиента: " + parameters[1]);

        this->toConsole("Состояние веб-сокета клиента: " + QString::number((short) newClientWebSocket->state()));

        // Связываем событие получения отключения веб-сокета клиента от сервера
        connect(newClientWebSocket, &QWebSocket::disconnected, this, &ServerWindow::eventClientSocketDisconnected, Qt::QueuedConnection);

        // Запоминаем веб-сокет нового клиента
        if (parameters[1] != ServerWindow::SENDER_SERVER)
        {
            QPair<QWebSocket*, QString> newClient;
            newClient.first = newClientWebSocket;
            newClient.second = parameters[1];
            this->clientsWebSockets.push_back(newClient);
            this->ui->usersListWidget->addItem(newClient.second);

            this->ui->logPlainTextEdit->appendHtml("<font color=\"lime\">Подключен новый пользователь.</font>");
        }
        else
        {
            connect(this, &ServerWindow::eventServerSocketSendTextMessage, serverWebSocket, &QWebSocket::sendTextMessage, Qt::QueuedConnection);
            this->threadCurrentTime->start();
            this->setConfiguration();

            this->ui->logPlainTextEdit->appendHtml("<font color=\"lime\">Подключен локальный веб-сокет.</font>");
        }
    }
    else
    {
         // Посылаем команду отключения веб-сокета от сервера
         newClientWebSocket->close();
         newClientWebSocket->deleteLater();

         this->toConsole("Не найдено имя пользователя в запросе веб-сокета клиента при подключении к серверу.");
    }
}

// Событие при получении текстового сообщения сервером
void ServerWindow::eventClientTextMessageRecieved(const QString& data)
{
    /*
     * Шаблоны запроса от клиента:
     * 1. SENDER_CLIENT MESSAGE_SEPARATOR command (2 элемента)
     * 2. SENDER_CLIENT MESSAGE_SEPARATOR command MESSAGE_SEPARATOR value (3 элемента)
     */

    // Парсим полученное сообщение
    QStringList parameters = data.split(ServerWindow::MESSAGE_SEPARATOR);

    // Если сообщение примерно соответствует шаблону, то выполняется поиск веб-сокета пользователя-получателя
    if (parameters.size() == 2 || parameters.size() == 3)
    {
        if (parameters[0] == ServerWindow::SENDER_CLIENT)
        {
            QString
                preparing = ServerWindow::SENDER_SERVER + ServerWindow::MESSAGE_SEPARATOR,
                newData = preparing;

            // Клиент запросил о статусе игры
            if (parameters[1] == ServerWindow::GAME_SATUS ||
                parameters[1] == ServerWindow::GAME_START ||
                parameters[1] == ServerWindow::GAME_STOP ||
                parameters[1] == ServerWindow::GAME_PAUSE ||
                parameters[1] == ServerWindow::GAME_CONTINUE)
            {
                if (parameters[1] != ServerWindow::GAME_SATUS)
                {
                    // Запуск потока обновления времени периода
                    if (parameters[1] == ServerWindow::GAME_START && this->threadPeriodTime == nullptr)
                    {
                        this->threadPeriodTime = QThread::create([this] { workThreadPeriodTime(); });
                        this->threadPeriodTime->start();
                        this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_RUN;
                    }
                    // Приостановка потока обновления времени периода
                    else if (parameters[1] == ServerWindow::GAME_PAUSE && this->threadPeriodTime != nullptr)
                    {
                        if (this->threadPeriodTime->isRunning())
                            this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_SLEEP;
                    }
                    // Возобновление потока обновления времени периода
                    else if (parameters[1] == ServerWindow::GAME_CONTINUE && this->threadPeriodTime != nullptr)
                    {
                        this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_RUN;
                    }
                    // Закрытие потока обновления времени периода
                    else if (parameters[1] == ServerWindow::GAME_STOP && this->threadPeriodTime != nullptr)
                    {
                        if (this->threadPeriodTime->isRunning())
                        {
                            this->threadPeriodTime->requestInterruption();
                            this->threadPeriodTime->exit();
                            this->threadPeriodTimeMode = ServerWindow::THREAD_PERIOD_FINISH;

                            if (!this->threadPeriodTime->wait(5000))
                            {
                                this->threadPeriodTime->terminate();
                                this->threadPeriodTime->wait();
                            }

                            this->threadPeriodTime->deleteLater();
                            this->threadPeriodTime = nullptr;

                            this->periodNumber = ServerWindow::PERIOD_MIN;
                            this->periodTime.first = ServerWindow::PERIOD_TIME_MIN;
                            this->periodTime.second = ServerWindow::PERIOD_TIME_MIN;
                            this->firstTeamScore = ServerWindow::SCORE_MIN;
                            this->secondTeamScore = ServerWindow::SCORE_MIN;
                        }
                    }

                    this->gameStatus = parameters[1];
                }

                QString message = "Клиент послал запрос о статусе игры: " + this->gameStatus;

                this->toConsole(message);

                this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");

                newData += ServerWindow::GAME_SATUS + ServerWindow::MESSAGE_SEPARATOR + this->gameStatus;
            }
            // Клиент запросил о текущем периоде
            else if (parameters[1] == ServerWindow::PERIOD_CURRENT ||
                     parameters[1] == ServerWindow::PERIOD_PREVIOUS ||
                     parameters[1] == ServerWindow::PERIOD_NEXT)
            {
                if (parameters[1] == ServerWindow::PERIOD_PREVIOUS && this->periodNumber > ServerWindow::PERIOD_MIN)
                    this->periodNumber--;
                else if (parameters[1] == ServerWindow::PERIOD_NEXT && this->periodNumber < ServerWindow::PERIOD_MAX)
                    this->periodNumber++;

                QString
                    periodNumber = QString::number(this->periodNumber),
                    message = "Клиент послал запрос о номере периода: " + periodNumber;

                this->toConsole(message);

                this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");

                newData += ServerWindow::PERIOD_CURRENT + ServerWindow::MESSAGE_SEPARATOR + periodNumber;
            }
            // Клиент запросил текущее время сервера
            else if (parameters[1] == ServerWindow::CURRENT_TIME)
            {
                QString currentTime = this->getCurrentTime();

                if (currentTime != oldTime)
                {
                    oldTime = currentTime;
                    QString message = "Клиент послал запрос о текущем времени (прошла минута): " + currentTime;

                    this->toConsole(message);

                    this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");
                }

                 newData += parameters[1] + ServerWindow::MESSAGE_SEPARATOR + currentTime;
            }
            // Клиент запросил текущее количество очков первой команды
            else if (parameters[1] == ServerWindow::SCORE_GET_FIRST ||
                     parameters[1] == ServerWindow::SCORE_ADD_FIRST ||
                     parameters[1] == ServerWindow::SCORE_SUBTRACT_FIRST)
            {
                if (parameters[1] == ServerWindow::SCORE_SUBTRACT_FIRST && this->firstTeamScore > ServerWindow::SCORE_MIN)
                    this->firstTeamScore--;
                else if (parameters[1] == ServerWindow::SCORE_ADD_FIRST && this->firstTeamScore < ServerWindow::SCORE_MAX)
                    this->firstTeamScore++;

                QString
                    firstTeamScore = QString::number(this->firstTeamScore),
                    message = "Клиент послал запрос о текущем количестве очков первой команды: " + firstTeamScore;

                this->toConsole(message);

                this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");

                newData += ServerWindow::SCORE_GET_FIRST + ServerWindow::MESSAGE_SEPARATOR + firstTeamScore;
            }
            // Клиент запросил текущее количество очков второй команды
            else if (parameters[1] == ServerWindow::SCORE_GET_SECOND ||
                     parameters[1] == ServerWindow::SCORE_ADD_SECOND ||
                     parameters[1] == ServerWindow::SCORE_SUBTRACT_SECOND)
            {
                if (parameters[1] == ServerWindow::SCORE_SUBTRACT_SECOND && this->secondTeamScore > ServerWindow::SCORE_MIN)
                    this->secondTeamScore--;
                else if (parameters[1] == ServerWindow::SCORE_ADD_SECOND && this->secondTeamScore < ServerWindow::SCORE_MAX)
                    this->secondTeamScore++;

                QString
                    secondTeamScore = QString::number(this->secondTeamScore),
                    message = "Клиент послал запрос о текущем количестве очков второй команды: " + secondTeamScore;

                this->toConsole(message);

                this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");

                newData += ServerWindow::SCORE_GET_SECOND + ServerWindow::MESSAGE_SEPARATOR + secondTeamScore;
            }
            // Клиент запросил название первой команды
            else if (parameters[1] == ServerWindow::NAME_FIRST)
            {
                if (parameters.size() == 3)
                    this->firstTeamName = parameters[2];

                QString message = "Клиент послал запрос о названии первой команды: " + this->firstTeamName;

                this->toConsole(message);

                this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");

                newData += ServerWindow::NAME_FIRST + ServerWindow::MESSAGE_SEPARATOR + this->firstTeamName;
            }
            // Клиент запросил название второй команды
            else if (parameters[1] == ServerWindow::NAME_SECOND)
            {
                this->toConsole("Клиент послал запрос о названии второй команды");

                if (parameters.size() == 3)
                    this->secondTeamName = parameters[2];

                QString message = "Клиент послал запрос о названии второй команды: " + this->secondTeamName;

                this->toConsole(message);

                this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");

                newData += ServerWindow::NAME_SECOND + ServerWindow::MESSAGE_SEPARATOR + this->secondTeamName;
            }
            // Клиент запросил время периода
            else if (parameters[1] == ServerWindow::PERIOD_TIME)
            {
                QString periodTime = this->getTime(this->periodTime.first, this->periodTime.second);

                if (this->oldPeriodTimeMinute != this->periodTime.first)
                {
                   this->oldPeriodTimeMinute = this->periodTime.first;

                   QString message = "Клиент послал запрос о времени текущего периода (прошла минута): " + periodTime;

                   this->toConsole(message);

                   this->ui->logPlainTextEdit->appendHtml("<font color=\"white\">" + message + "</font>");
                }

                newData += ServerWindow::PERIOD_TIME + ServerWindow::MESSAGE_SEPARATOR + periodTime;
            }


            // Отправляем сформированное сообщение всем клиентам
            if (newData != preparing && newData.size() > 0)
            {
                for (int i = 0; i < this->clientsWebSockets.size(); i++)
                    this->clientsWebSockets[i].first->sendTextMessage(newData);
            }
        }
    }
}

// Событие при отключении веб-сокета существующего клиента
void ServerWindow::eventClientSocketDisconnected()
{
    // Получение веб-сокета клиента-отправителя
    QWebSocket* senderClientWebSocket = qobject_cast<QWebSocket*>(sender());

    this->toConsole("Веб-сокет клиента был отключен.");

    // Если веб-сокет валидный, то выполняется удаения данного веб-сокета из списка
    if (senderClientWebSocket)
    {
        QString clientName = "";

        for (int i = 0; i < this->clientsWebSockets.size(); i++)
            // Если веб-сокет найден в списке, то он удаляется из него
            if (this->clientsWebSockets[i].first == senderClientWebSocket)
            {
                // Запоминаем имя пользователя
                if (clientName.size() == 0)
                {
                    clientName = this->clientsWebSockets[i].second;

                    this->toConsole("Имя отключенного клиента: " + clientName);

                    this->ui->logPlainTextEdit->appendHtml("<font color =\"lightsalmon\"> Пользователь " + clientName + " был отключен.</font>");
                }

                this->clientsWebSockets.removeAt(i--);
            }

        // Находим пользователя в списке пользователей и удаляем оттуда
        for (int i = 0; i < this->ui->usersListWidget->count(); i++)
        {
            // Получаем элемент из списка
            QListWidgetItem* itemToRemove = this->ui->usersListWidget->item(i);

            // Если имена совпадают, то извлекаем из списка элемент и удаляем его
            if (itemToRemove->text() == clientName)
            {
                itemToRemove = this->ui->usersListWidget->takeItem(i);
                delete itemToRemove;
                i--;
            }
        }

        senderClientWebSocket->deleteLater();
    }
}

// Событие, возникающее при поиске сервера (получение данных в UDP-сокет)
void ServerWindow::eventSearchServer()
{
    QByteArray request;

    this->toConsole("Поиск запросов от клиентов на подключение...");

    while (this->searchUdpSocket.hasPendingDatagrams())
    {
        request.resize(this->searchUdpSocket.pendingDatagramSize());
        searchUdpSocket.readDatagram(request.data(), request.size());

        this->toConsole("Есть запросы для подлкючения.");

        // Параметры, полученные в UDP-сокет
        QStringList parameters = QString::fromUtf8(request).split(ServerWindow::MESSAGE_SEPARATOR);

        // Количество полученных параметров должно быть ровно два
        if (parameters.size() == 2)
        {
            this->toConsole("Корректное количество параметров запроса подключения.");

            // Найденная пара IPv4 с портом в списке адресов сервера
            QString foundAddress = this->findServerAddress(parameters[1]);

            /*
             * Если это был запрос о поиске сервера и полученный адрес клиента имеет корректный формат, то посылается сообщение формата:
             * COMMAND_CLIENT_SEARCH MESSAGE_SEPARATOR server_ip MESSAGE_SEPARATOR server_port
             * MESSAGE_SEPARATOR client_ip MESSAGE_SEPARTOR game_status (5 элементов)
             */
            if (parameters[0] == ServerWindow::SEARCH_SERVER && foundAddress.size() > 0)
            {
                this->toConsole("Запрос задан верно. Адрес сервера: " +
                                foundAddress + ":" + this->currentPort + " Адрес клиента: " + parameters[1]);

                this->searchUdpSocket.writeDatagram(QString(ServerWindow::SEARCH_CLIENT + ServerWindow::MESSAGE_SEPARATOR +
                                                      foundAddress + ServerWindow::MESSAGE_SEPARATOR +
                                                      this->currentPort + ServerWindow::MESSAGE_SEPARATOR +
                                                      parameters[1]).toUtf8(),
                                                   QHostAddress::Broadcast, ServerWindow::PORT_UDP_SERVER);
            }
            else
                this->toConsole("Запрос задан неверно. Адрес сервера: " + foundAddress + ":" + this->currentPort + "Адрес клиента: " + parameters[1]);
        }
        else
            this->toConsole("Некорректное количество параметров запроса подключения.");
    }
}

// Событие при нажатии на элемент списка пользователей
void ServerWindow::eventUsersListWidgetItemClicked(QListWidgetItem* item)
{
    if (item == this->currentUserListWidgetItem)
    {
        this->currentUserListWidgetItem = nullptr;
        this->ui->usersListWidget->setCurrentItem(nullptr);
    }
    else
        this->currentUserListWidgetItem = item;
}

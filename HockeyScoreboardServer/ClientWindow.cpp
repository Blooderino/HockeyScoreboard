#include "ClientWindow.h"
#include "ui_ClientWindow.h"

// Конструктор класса
ClientWindow::ClientWindow(bool debug, QWidget* parent) : QMainWindow(parent), ui(new Ui::ClientWindow)
{
    this->debug = debug;

    this->toConsole("Запуск графической оболочки.");

    this->ui->setupUi(this);
    this->serverUrl = "ws://";
    this->serverAddressFound = false;
    this->gameStatus = ClientWindow::GAME_STOP;
    this->oldPeriodTimeMinutes = "00";

    // Связываем события элементов графического интерфейса
    connect(this->ui->generalGameStartPushButton, &QPushButton::clicked, this, &ClientWindow::eventGeneralGameStartPushButtonClicked);
    connect(this->ui->generalGameStopPushButton, &QPushButton::clicked, this, &ClientWindow::eventGeneralGameStopPushButtonClicked);
    connect(this->ui->generalGamePausePushButton, &QPushButton::clicked, this, &ClientWindow::eventGeneralGamePausePushButtonClicked);
    connect(this->ui->generalGameContinuePushButton, &QPushButton::clicked, this, &ClientWindow::eventGeneralGameContinuePushButtonClicked);
    connect(this->ui->firstTeamScoreAddPushButton, &QPushButton::clicked, this, &ClientWindow::eventFirstTeamScoreAddPushButtonClicked);
    connect(this->ui->firstTeamScoreSubtractPushButton, &QPushButton::clicked, this, &ClientWindow::eventFirstTeamScoreSubtractPushButtonClicked);
    connect(this->ui->secondTeamScoreAddPushButton, &QPushButton::clicked, this, &ClientWindow::eventSecondTeamScoreAddPushButtonClicked);
    connect(this->ui->secondTeamScoreSubtractPushButton, &QPushButton::clicked, this, &ClientWindow::eventSecondTeamScoreSubtractPushButtonClicked);
    connect(this->ui->generalPeriodPreviousPushButton, &QPushButton::clicked, this, &ClientWindow::eventGeneralPeriodPreviousPushButton);
    connect(this->ui->generalPeriodNextPushButton, &QPushButton::clicked, this, &ClientWindow::eventGeneralPeriodNextPushButton);
    connect(this->ui->firstTeamNameLineEdit, &QLineEdit::textChanged, this, &ClientWindow::eventTeamsNamesTextChanged);
    connect(this->ui->secondTeamNameLineEdit, &QLineEdit::textChanged, this, &ClientWindow::eventTeamsNamesTextChanged);

    this->toConsole("Подключение UDP-сокета...");

    // Открываем UDP-сокет для поиска севера в локальной сети
    this->searchUdpSocket.bind(ClientWindow::PORT_UDP_SERVER, QUdpSocket::ShareAddress);
    connect(&this->searchUdpSocket, &QUdpSocket::readyRead, this, &ClientWindow::eventSearchServer);

    this->toConsole("UDP-сокет покдлючен.");
    this->toConsole("Список всех доступных локальных IPv4:");

    // Ищем все доступные локальные IPv4 адреса. Найденные адреса посылаем серверу как запросы на подключение.
    this->doSendMessageToSearchServer();
}

// Деструктор класса
ClientWindow::~ClientWindow()
{
    this->clientWebSocket.close();
    delete this->ui;
}

// Стандартный ответ клиента при поиске сервера
const QString ClientWindow::SEARCH_CLIENT = "LANCHAT_SEARCH_CLIENT";

// Стандартный ответ сервера клиенту
const QString ClientWindow::SEARCH_SERVER = "LANCHAT_SEARCH_SERVER";

// Стандартный разделитель в ответе от сервера
const QString ClientWindow::MESSAGE_SEPARATOR = "[|!|]";

// Имя пользователя, если получатель - все пользователи
const QString ClientWindow::RECIEVER_ALL = "RECIEVER_ALL";

// Имя пользователя, если отправитель - сервер
const QString ClientWindow::SENDER_SERVER = "SENDER_SERVER";

// Отправитель сообщения клиент
const QString ClientWindow::SENDER_CLIENT = "SENDER_CLIENT";

// Сервер отключен
const QString ClientWindow::SERVER_DISCONNECTED = "SERVER_DISCONNECTED";

// Станадратное имя окна
const QString ClientWindow::WINDOW_TITLE = "HockeyScoreboardClient";

// Статус игры
const QString ClientWindow::GAME_SATUS = "GAME_STATUS";

// Запуск игры
const QString ClientWindow::GAME_START = "GAME_START";

// Окончание игры
const QString ClientWindow::GAME_STOP = "GAME_STOP";

// Приостановка игры
const QString ClientWindow::GAME_PAUSE = "GAME_PAUSE";

// Продолжение игры
const QString ClientWindow::GAME_CONTINUE = "GAME_CONTINUE";

// Текущий период
const QString ClientWindow::PERIOD_CURRENT = "PERIOD_CURRENT";

// Переключение на предыдущий период
const QString ClientWindow::PERIOD_PREVIOUS = "PERIOD_PREVIOUS";

// Переключение на следующий период
const QString ClientWindow::PERIOD_NEXT = "PERIOD_NEXT";

// Время периода
const QString ClientWindow::PERIOD_TIME = "PERIOD_TIME";

// Текущее время
const QString ClientWindow::CURRENT_TIME = "CURRENT_TIME";

// Вернуть очки для первой команды
const QString ClientWindow::SCORE_GET_FIRST = "SCORE_GET_FIRST";

// Вернуть очки для второй команды
const QString ClientWindow::SCORE_GET_SECOND = "SCORE_GET_SECOND";

// Добавить очки для первой команды
const QString ClientWindow::SCORE_ADD_FIRST = "SCORE_ADD_FIRST";

// Добавить очки для второй команды
const QString ClientWindow::SCORE_ADD_SECOND = "SCORE_ADD_SECOND";

// Отнять очки у первой команды
const QString ClientWindow::SCORE_SUBTRACT_FIRST = "SCORE_SUBTRACT_FIRST";

// Отнять очки у второй команды
const QString ClientWindow::SCORE_SUBTRACT_SECOND = "SCORE_SUBTRACT_SECOND";

// Получить имя первой команды
const QString ClientWindow::NAME_FIRST = "NAME_FIRST";

// Получить имя второй команды
const QString ClientWindow::NAME_SECOND = "NAME_SECOND";

// Стандартное время
const QString ClientWindow::TIME_DEFAULT = "00:00";

// Стандартное имя первой команды
const QString ClientWindow::NAME_FIRST_DEFAULT = "Team1";

// Стандартное имя второй команды
const QString ClientWindow::NAME_SECOND_DEFAULT = "Team2";

// Выводит сообщение в консоль
void ClientWindow::toConsole(const QString &text)
{
    if (this->debug)
        qDebug() << "[" << this->metaObject()->className() << "]: "  << text;
}

// Посылает сообщения для поиска сервера в сети
void ClientWindow::doSendMessageToSearchServer()
{
    // Ищем все доступные локальные IPv4 адреса. Найденные адреса посылаем серверу как запросы на подключение.
    for (const QHostAddress& address: QNetworkInterface::allAddresses())
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost)
        {
            this->toConsole("Доступен адрес: " + address.toString() + " (отправка запроса на подключение).");

            this->searchUdpSocket.writeDatagram(QString(ClientWindow::SEARCH_SERVER + ClientWindow::MESSAGE_SEPARATOR + address.toString()).toUtf8(),
                                                QHostAddress::Broadcast, ClientWindow::PORT_UDP_CLIENT);
        }
}

// Возвращает, введено ли корректное имя пользователя
bool ClientWindow::isCorrectTeamName(const QString& teamName)
{
    bool check = teamName.size() > 2 && teamName.size() < 17;


    for (int i = 0; i < teamName.size() && check; i++)
        check = ((teamName[i] >= 'a' && teamName[i] <= 'z') ||
                (teamName[i] >= 'A' && teamName[i] <= 'Z') ||
                (teamName[i] >= '0' && teamName[i] <= '9'));


    if (check)
        check = teamName != ClientWindow::SENDER_SERVER;

    return check;
}

// Возвращает текущие даут и время
QString ClientWindow::getCurrentDateTime(const QString& separator)
{
    return QString::number(QDate::currentDate().day()) + "." +
           QString::number(QDate::currentDate().month()) + "." +
           QString::number(QDate::currentDate().year()) + separator +
           QString::number(QTime::currentTime().hour()) + "-" +
           QString::number(QTime::currentTime().minute()) + "-" +
           QString::number(QTime::currentTime().second()) + "-" +
           QString::number(QTime::currentTime().msec());
}

// Возвращает, готов ли клиент к запуску игры
bool ClientWindow::isReadyToGameStart()
{
    return this->gameStatus == ClientWindow::GAME_STOP &&
           this->isCorrectTeamName(this->ui->firstTeamNameLineEdit->text()) &&
           this->isCorrectTeamName(this->ui->secondTeamNameLineEdit->text());
}

// Событие, возникающее при завершении подключения веб-сокета клиента к серверу
void ClientWindow::eventConnected()
{
    // Статус игры
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::GAME_SATUS);

    // Данные о первой команде
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::NAME_FIRST);
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_GET_FIRST);

    // Данные о второй команде
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::NAME_SECOND);
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_GET_SECOND);

    // Данные о текущем периоде
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::PERIOD_CURRENT);

    // Включение текущего времени клиента
    this->ui->generalCurrentTimeValueLabel->setEnabled(true);
}

// Событие, возникающее при завершении подключения веб-сокета клиента к серверу
void ClientWindow::eventDisconnected()
{
    this->serverAddressFound = false;
    this->setWindowTitle(ClientWindow::WINDOW_TITLE);

    disconnect(&this->clientWebSocket, &QWebSocket::connected, this, &ClientWindow::eventConnected);
    disconnect(&this->clientWebSocket, &QWebSocket::textMessageReceived, this, &ClientWindow::eventTextMessageRecieved);
    disconnect(&this->clientWebSocket, &QWebSocket::disconnected, this, &ClientWindow::eventDisconnected);

    // Элементы управления основного раздела
    this->ui->generalGameStartPushButton->setEnabled(false);
    this->ui->generalGameStopPushButton->setEnabled(false);
    this->ui->generalGamePausePushButton->setEnabled(false);
    this->ui->generalGameContinuePushButton->setEnabled(false);
    this->ui->generalPeriodTimeValueLabel->setEnabled(false);
    this->ui->generalCurrentTimeValueLabel->setEnabled(false);
    this->ui->generalPeriodTimeValueLabel->setText(ClientWindow::TIME_DEFAULT);
    this->ui->generalCurrentTimeValueLabel->setText(ClientWindow::TIME_DEFAULT);
    this->ui->generalPeriodValueLabel->setEnabled(false);
    this->ui->generalPeriodValueLabel->setText(QString::number(ClientWindow::PERIOD_MIN));
    this->ui->generalPeriodPreviousPushButton->setEnabled(false);
    this->ui->generalPeriodNextPushButton->setEnabled(false);

    // Элементы управления первой команды
    this->ui->firstTeamNameLineEdit->setEnabled(false);
    this->ui->firstTeamNameLineEdit->setText(ClientWindow::NAME_FIRST_DEFAULT);
    this->ui->firstTeamScoreValueLabel->setEnabled(false);
    this->ui->firstTeamScoreValueLabel->setText(QString::number(ClientWindow::SCORE_MIN));
    this->ui->firstTeamScoreAddPushButton->setEnabled(false);
    this->ui->firstTeamScoreSubtractPushButton->setEnabled(false);

    // Элементы управления второй команды
    this->ui->secondTeamNameLineEdit->setEnabled(false);
    this->ui->secondTeamNameLineEdit->setText(ClientWindow::NAME_SECOND_DEFAULT);
    this->ui->secondTeamScoreValueLabel->setEnabled(false);
    this->ui->secondTeamScoreValueLabel->setText(QString::number(ClientWindow::SCORE_MIN));
    this->ui->secondTeamScoreAddPushButton->setEnabled(false);
    this->ui->secondTeamScoreSubtractPushButton->setEnabled(false);

    this->doSendMessageToSearchServer();
}

// Событие, возникающее при поиске сервера (получение данных в UDP-сокет)
void ClientWindow::eventSearchServer()
{
    // Ответ по UDP-сокету
    QByteArray answer;

    this->toConsole("Поиск ответа от сервера на подключение...");

    while (this->searchUdpSocket.hasPendingDatagrams())
    {
        // Получаем сообщение
        answer.resize(this->searchUdpSocket.pendingDatagramSize());
        this->searchUdpSocket.readDatagram(answer.data(), answer.size());

        // Формируем список параметров
        QStringList parameters = QString(answer).split(ClientWindow::MESSAGE_SEPARATOR);

        this->toConsole("Есть ответ для подлкючения.");

        // Запрос должен содержать 2 параметра
        if (parameters.size() == 2)
        {
             this->toConsole("Корректное количество параметров ответа подключения.");

            // Сервер выполняет поиск активных клиентов
            if (parameters[0] == ClientWindow::SEARCH_CLIENT && parameters[1] == ClientWindow::SEARCH_SERVER)
            {
                this->toConsole("Сервер выполняет поиск клиентов.");

                this->doSendMessageToSearchServer();
            }
            else
                this->toConsole("Некорректный ответ от сервера при поиске клиентов.");
        }
        // Запрос должен содержать 4 параметра
        else if (parameters.size() == 4)
        {
            this->toConsole("Корректное количество параметров запроса.");

            // Если получен ответ от искомого сервера и текущий веб-сокет клиента ещё не подключен к нему, то клиент подгатавливается к подключению
            if (parameters[0] == ClientWindow::SEARCH_CLIENT &&
                clientWebSocket.state() != QAbstractSocket::ConnectedState && !this->serverAddressFound)
            {
                this->clientIP = parameters[3];

                this->toConsole("Запрос задан верно. Адрес сервера: " + parameters[1] + ":" + parameters[2] + " Адрес клиента: " + parameters[3]);

                this->serverAddressFound = true;
                this->setWindowTitle(
                            this->windowTitle() +
                            " [Server IP: " + parameters[1] + ":" + parameters[2] +
                            ", Client IP: " + this->clientIP + "]");
                this->serverUrl.setHost(parameters[1]);
                this->serverUrl.setPort(parameters[2].toInt());

                this->connectionDateTime = this->getCurrentDateTime();
                this->clientWebSocket.open(QUrl(serverUrl.toString() + "/?" + this->clientIP + "=" + this->connectionDateTime));

                // Если веб-сокет подключен, то связываются события получения сообщения, разблокируется пользовательский интерфейс
                if (this->clientWebSocket.state() == QAbstractSocket::SocketState::ConnectingState)
                {
                    this->toConsole("Веб-сокет клиента подключается.");

                    connect(&this->clientWebSocket, &QWebSocket::connected, this, &ClientWindow::eventConnected);
                    connect(&this->clientWebSocket, &QWebSocket::textMessageReceived, this, &ClientWindow::eventTextMessageRecieved);
                    connect(&this->clientWebSocket, &QWebSocket::disconnected, this, &ClientWindow::eventDisconnected);
                }
                // Иначе происходит повторная попытка подключиться
                else
                {
                    this->toConsole("Веб-сокет клиента не подключен.");

                    this->doSendMessageToSearchServer();
                }
            }
            else
                this->toConsole("Запрос задан неверно или подключение уже существует. Адрес сервера: " +
                                parameters[1] + ":" + parameters[2] + " Адрес клиента: " + parameters[3]);
        }
        else
            this->toConsole("Некорректное количество параметров ответа подключения.");
    }
}

// Событие, возникающее при получении сообщения
void ClientWindow::eventTextMessageRecieved(const QString& data)
{
    /*
     * Шаблон ответа от сервера:
     * SENDER_SERVER MESSAGE_SEPARATOR command MESSAGE_SEPARATOR value (3 элемента)
     */

    // Парсинг полученного сообщения
    QStringList parameters = data.split(ClientWindow::MESSAGE_SEPARATOR);

    // Если количество параметров ответа равно 3, то ответ сформирован корректно
    if (parameters.size() == 3)
    {
        // Если отправитель сообщения был сервер, то сообщение готово к обработке со стороны клиента
        if (parameters[0] == ClientWindow::SENDER_SERVER)
        {
           // Пришло сообщение о статусе игры
           if (parameters[1] == ClientWindow::GAME_SATUS)
           {
               this->toConsole("Текущий статус игры: " + parameters[2]);

               bool
                   gameStart = parameters[2] == ClientWindow::GAME_START,
                   gameStop = parameters[2] == ClientWindow::GAME_STOP,
                   gamePause = parameters[2] == ClientWindow::GAME_PAUSE,
                   gameContinue = parameters[2] == ClientWindow::GAME_CONTINUE,
                   gameProcess = gameStart || gameContinue;

               this->gameStatus = parameters[2];

               // Элементы управления основного раздела
               this->ui->generalGameStartPushButton->setEnabled(gameStop);
               this->ui->generalGameStopPushButton->setEnabled(!gameStop);
               this->ui->generalGamePausePushButton->setEnabled(gameProcess);
               this->ui->generalGameContinuePushButton->setEnabled(gamePause);
               this->ui->generalPeriodTimeValueLabel->setEnabled(!gameStop);
               this->ui->generalPeriodValueLabel->setEnabled(gameProcess);
               this->ui->generalPeriodPreviousPushButton->setEnabled(
                           gameProcess && this->ui->generalPeriodValueLabel->text().toInt() > ClientWindow::PERIOD_MIN);
               this->ui->generalPeriodNextPushButton->setEnabled(
                           gameProcess && this->ui->generalPeriodValueLabel->text().toInt() < ClientWindow::PERIOD_MAX);

               // Элементы управления первой команды
               this->ui->firstTeamNameLineEdit->setEnabled(gameStop);
               this->ui->firstTeamScoreValueLabel->setEnabled(!gameStop);
               this->ui->firstTeamScoreAddPushButton->setEnabled(
                           gameProcess && this->ui->firstTeamScoreValueLabel->text().toInt() < ClientWindow::SCORE_MAX);
               this->ui->firstTeamScoreSubtractPushButton->setEnabled(
                           gameProcess && this->ui->firstTeamScoreValueLabel->text().toInt() > ClientWindow::SCORE_MIN);

               // Элементы управления второй команды
               this->ui->secondTeamNameLineEdit->setEnabled(gameStop);
               this->ui->secondTeamScoreValueLabel->setEnabled(!gameStop);
               this->ui->secondTeamScoreAddPushButton->setEnabled(
                           gameProcess && this->ui->secondTeamScoreValueLabel->text().toInt() < ClientWindow::SCORE_MAX);
               this->ui->secondTeamScoreSubtractPushButton->setEnabled(
                           gameProcess && this->ui->secondTeamScoreValueLabel->text().toInt() > ClientWindow::SCORE_MIN);
           }
           // Пришло сообщение с названием первой команды
           else if (parameters[1] == ClientWindow::NAME_FIRST)
           {
               this->toConsole("Получено имя первой команды: " + parameters[2]);

               this->ui->firstTeamNameLineEdit->setText(parameters[2]);
           }
           // Пришло сообщение с названием второй команды
           else if (parameters[1] == ClientWindow::NAME_SECOND)
           {
               this->toConsole("Получено имя второй команды: " + parameters[2]);

               this->ui->secondTeamNameLineEdit->setText(parameters[2]);
           }
           // Пришло сообщение с номером периода
           else if (parameters[1] == ClientWindow::PERIOD_CURRENT)
           {
               this->toConsole("Получен номер текущего периода: " + parameters[2]);

               this->ui->generalPeriodValueLabel->setText(parameters[2]);
               bool gameProcess = this->gameStatus == ClientWindow::GAME_START || this->gameStatus == ClientWindow::GAME_CONTINUE;

               quint16 period = parameters[2].toInt();
               this->ui->generalPeriodPreviousPushButton->setEnabled(period > ClientWindow::PERIOD_MIN && gameProcess);
               this->ui->generalPeriodNextPushButton->setEnabled(period < ClientWindow::PERIOD_MAX && gameProcess);
           }
           // Пришло текущее время сервера
           else if (parameters[1] == ClientWindow::CURRENT_TIME && this->ui->generalCurrentTimeValueLabel->text() != parameters[2])
           {
               this->toConsole("Получено текущее время сервера: " + parameters[2]);

               this->ui->generalCurrentTimeValueLabel->setText(parameters[2]);
           }
           // Пришло текущее количество очков первой команды
           else if (parameters[1] == ClientWindow::SCORE_GET_FIRST)
           {
               this->toConsole("Получено текущее количество очков первой команды: " + parameters[2]);

               this->ui->firstTeamScoreValueLabel->setText(parameters[2]);
               bool gameProcess = this->gameStatus == ClientWindow::GAME_START || this->gameStatus == ClientWindow::GAME_CONTINUE;

               quint16 score = parameters[2].toInt();
               this->ui->firstTeamScoreAddPushButton->setEnabled(score < ClientWindow::SCORE_MAX && gameProcess);
               this->ui->firstTeamScoreSubtractPushButton->setEnabled(score > ClientWindow::SCORE_MIN && gameProcess);
           }
           // Пришло текущее количество очков второй команды
           else if (parameters[1] == ClientWindow::SCORE_GET_SECOND)
           {
               this->toConsole("Получено текущее количество очков второй команды: " + parameters[2]);

               this->ui->secondTeamScoreValueLabel->setText(parameters[2]);
               bool gameProcess = this->gameStatus == ClientWindow::GAME_START || this->gameStatus == ClientWindow::GAME_CONTINUE;

               quint16 score = parameters[2].toInt();
               this->ui->secondTeamScoreAddPushButton->setEnabled(score < ClientWindow::SCORE_MAX && gameProcess);
               this->ui->secondTeamScoreSubtractPushButton->setEnabled(score > ClientWindow::SCORE_MIN && gameProcess);
           }
           // Пришло текущее время периода
           else if (parameters[1] == ClientWindow::PERIOD_TIME)
           {
               QStringList time = parameters[2].split(":");

               if (time[0] != this->oldPeriodTimeMinutes)
               {
                   this->toConsole("Получено текущее время периода (прошла минута): " + parameters[2]);

                   this->oldPeriodTimeMinutes = time[0];
               }

               this->ui->generalPeriodTimeValueLabel->setText(parameters[2]);
           }
        }
    }
}

// Событие, возникающее при нажатии кнопки начала игры
void ClientWindow::eventGeneralGameStartPushButtonClicked(bool click)
{
    this->toConsole("Игра запущена.");

    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::GAME_START);

    // Данные о первой команде
    this->clientWebSocket.sendTextMessage(
                ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR +
                ClientWindow::NAME_FIRST + ClientWindow::MESSAGE_SEPARATOR +
                this->ui->firstTeamNameLineEdit->text());
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_GET_FIRST);

    // Данные о второй команде
    this->clientWebSocket.sendTextMessage(
                ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR +
                ClientWindow::NAME_SECOND + ClientWindow::MESSAGE_SEPARATOR +
                this->ui->secondTeamNameLineEdit->text());
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_GET_SECOND);

    // Данные о текущем периоде
    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::PERIOD_CURRENT);
}

// Событие, возникающее при нажатии кнопки завершения игры
void ClientWindow::eventGeneralGameStopPushButtonClicked(bool click)
{
    this->toConsole("Игра прекращена.");

    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::GAME_STOP);
}

// Событие, возникающее при нажатии кнопки приостановки игры
void ClientWindow::eventGeneralGamePausePushButtonClicked(bool click)
{
    this->toConsole("Игра приостановлена.");

    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::GAME_PAUSE);
}

// Событие, возникающее при нажатии кнопки продолжения игры
void ClientWindow::eventGeneralGameContinuePushButtonClicked(bool click)
{
    this->toConsole("Игра продолжена.");

    this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::GAME_CONTINUE);
}

// Событие, возникающее при нажатии кнопки добавить у первой команды
void ClientWindow::eventFirstTeamScoreAddPushButtonClicked(bool click)
{
    if (this->ui->firstTeamScoreValueLabel->text().toInt() < ClientWindow::SCORE_MAX)
    {
        this->toConsole("Добавлены очки первой команде.");

        this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_ADD_FIRST);
    }
    else
        this->toConsole("Очки не будут добавлены первой команде.");
}

// Событие, возникающее при нажатии кнопки отнять у первой команды
void ClientWindow::eventFirstTeamScoreSubtractPushButtonClicked(bool click)
{
    if (this->ui->firstTeamScoreValueLabel->text().toInt() > ClientWindow::SCORE_MIN)
    {
        this->toConsole("Убавлены очки у первой команды.");

        this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_SUBTRACT_FIRST);
    }
    else
        this->toConsole("Очки не будут убавлены у первой команды.");
}

// Событие, возникающее при нажатии кнопки добавить у второй команды
void ClientWindow::eventSecondTeamScoreAddPushButtonClicked(bool click)
{
    if (this->ui->secondTeamScoreValueLabel->text().toInt() < ClientWindow::SCORE_MAX)
    {
        this->toConsole("Добавлены очки второй команде.");

        this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_ADD_SECOND);
    }
    else
        this->toConsole("Очки не будут добавлены второй команде.");
}

// Событие, возникающее при нажатии кнопки отнять у второй команды
void ClientWindow::eventSecondTeamScoreSubtractPushButtonClicked(bool click)
{
    if (this->ui->secondTeamScoreValueLabel->text().toInt() > ClientWindow::SCORE_MIN)
    {
        this->toConsole("Убавлены очки у второй команды.");

        this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::SCORE_SUBTRACT_SECOND);
    }
    else
        this->toConsole("Очки не будут убавлены у второй команды.");
}

// Событие, возникающее при нажатии кнопки возврата к предыдущему периоду
void ClientWindow::eventGeneralPeriodPreviousPushButton(bool click)
{
    if (this->ui->generalPeriodValueLabel->text().toInt() > ClientWindow::PERIOD_MIN)
    {
        this->toConsole("Уменьшен номер периода.");

        this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::PERIOD_PREVIOUS);
    }
    else
        this->toConsole("Номер периода не будет уменьшен.");
}

// Событие, возникающее при нажатии кнопки перехода к следующему периоду
void ClientWindow::eventGeneralPeriodNextPushButton(bool click)
{
    if (this->ui->generalPeriodValueLabel->text().toInt() < ClientWindow::PERIOD_MAX)
    {
        this->toConsole("Увеличен номер периода.");

        this->clientWebSocket.sendTextMessage(ClientWindow::SENDER_CLIENT + ClientWindow::MESSAGE_SEPARATOR + ClientWindow::PERIOD_NEXT);
    }
    else
        this->toConsole("Номер периода не будет увеличен.");
}

// Событие, возникающее при редактировании имен команд
void ClientWindow::eventTeamsNamesTextChanged(const QString& text)
{
    this->ui->generalGameStartPushButton->setEnabled(this->isReadyToGameStart());
}

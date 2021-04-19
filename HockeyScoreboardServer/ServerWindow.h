#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QTime>
#include <QDate>
#include <QListWidgetItem>
#include <QThread>
#include <functional>
#include <QSettings>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

QT_BEGIN_NAMESPACE
namespace Ui { class ServerWindow; }
QT_END_NAMESPACE

// Класс окна сервера чата
class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:

    // Конструктор класса
    ServerWindow(bool debug = true, quint16 port = ServerWindow::PORT_CONNECTION, QWidget *parent = nullptr);

    // Деструктор класса
    ~ServerWindow();

    // Стандартный порт подключения к веб-сокету сервера
    static constexpr quint16 PORT_CONNECTION = 7200;

    // Стандартный порт подключения к UDP-сокету клиента
    static constexpr quint16 PORT_UDP_CLIENT = 7272;

    // Стандартный порт подключения к UDP-сокету сервера
    static constexpr quint16 PORT_UDP_SERVER = 2727;

    // Стандартный ответ клиента при поиске сервера
    const static QString SEARCH_CLIENT;

    // Стандартный ответ сервера клиенту
    const static QString SEARCH_SERVER;

    // Стандартный разделитель в ответе от сервера
    const static QString MESSAGE_SEPARATOR;

    // Имя пользователя, если получатель - все пользователи
    const static QString RECIEVER_ALL;

    // Имя пользователя, если отправитель - сервер
    const static QString SENDER_SERVER;

    // Отправитель сообщения клиент
    const static QString SENDER_CLIENT;

    // Сервер отключен
    const static QString SERVER_DISCONNECTED;

    // Статус игры
    const static QString GAME_SATUS;

    // Запуск игры
    const static QString GAME_START;

    // Окончание игры
    const static QString GAME_STOP;

    // Приостановка игры
    const static QString GAME_PAUSE;

    // Продолжение игры
    const static QString GAME_CONTINUE;

    // Текущий период
    const static QString PERIOD_CURRENT;

    // Переключение на предыдущий период
    const static QString PERIOD_PREVIOUS;

    // Переключение на следующий период
    const static QString PERIOD_NEXT;

    // Время периода
    const static QString PERIOD_TIME;

    // Текущее время
    const static QString CURRENT_TIME;

    // Вернуть очки для первой команды
    const static QString SCORE_GET_FIRST;

    // Вернуть очки для второй команды
    const static QString SCORE_GET_SECOND;

    // Добавить очки для первой команды
    const static QString SCORE_ADD_FIRST;

    // Добавить очки для второй команды
    const static QString SCORE_ADD_SECOND;

    // Отнять очки у первой команды
    const static QString SCORE_SUBTRACT_FIRST;

    // Отнять очки у второй команды
    const static QString SCORE_SUBTRACT_SECOND;

    // Минимальное количество очков
    static constexpr quint16 SCORE_MIN = 0;

    // Максимальное количество очков
    static constexpr quint16 SCORE_MAX = 99;

    // Минимальный номер периода
    static constexpr quint16 PERIOD_MIN = 1;

    // Максимальный номер периода
    static constexpr quint16 PERIOD_MAX = 3;

    // Минимальное время периода
    static constexpr quint16 PERIOD_TIME_MIN = 0;

    // Максимальное время периода в минутах
    static constexpr quint16 PERIOD_TIME_MAX_MINUTES = 19;

    // Максимальное время периода в секундах
    static constexpr quint16 PERIOD_TIME_MAX_SECONDS = 59;

    // Получить имя первой команды
    const static QString NAME_FIRST;

    // Получить имя второй команды
    const static QString NAME_SECOND;

    // Режим завершения работы потока обновления времени периода
    static constexpr quint8 THREAD_PERIOD_FINISH = 0;

    // Режим запуска/продолжения работы потока обновления времени периода
    static constexpr quint8 THREAD_PERIOD_RUN = 1;

    // Режим ожидания работы потока обновления времени периода
    static constexpr quint8 THREAD_PERIOD_SLEEP = 2;

    // Отправитель сообщения поток обновления текущего времени
    const static QString SENDER_THREAD_CURRENT_TIME;

    // Отправитель сообщения потоко обновления времени периода
    const static QString SENDER_THREAD_PERIOD_TIME;

    // Стандартное имя первой команды
    const static QString NAME_FIRST_DEFAULT;

    // Стандартное имя второй команды
    const static QString NAME_SECOND_DEFAULT;

    // Стандартное имя для конфигурационного файла
    const static QString CONFIG_FILE_NAME;

    // Стандартное имя для поля с сохраненной игрой
    const static QString CONFIG_SAVED_GAME;

    // Стандартное имя поля для номера периода
    const static QString CONFIG_PERIOD_NUMBER;

    // Стандартное имя поля для минут периода
    const static QString CONFIG_PERIOD_TIME_MINUTES;

    // Стандартное имя поля для секунд периода
    const static QString CONFIG_PERIOD_TIME_SECONDS;

    // Станадртное имя поля для имени первой команды
    const static QString CONFIG_FIRST_NAME;

    // Стандартное имя поля для количества очков первой команды
    const static QString CONFIG_FIRST_SCORE;

    // Станадртное имя поля для имени второй команды
    const static QString CONFIG_SECOND_NAME;

    // Стандартное имя поля для количества очков второй команды
    const static QString CONFIG_SECOND_SCORE;

    // Стандартное имя поля для статуса игры
    const static QString CONFIG_GAME_STATUS;

    // Возвращает найденный веб-сокет клиента
    QWebSocket* getClientWebSocketByIPDateTime(const QString& ipDateTime);

    // Возвращает индекс веб-сокета клиента в общем списке
    int getIndexOfClientWebSocket(QWebSocket* clientWebSocket);

    // Возвращает адрес сервера, который находится в той же под сети
    QString findServerAddress(const QString& address);

    // Выводит сообщение в консоль
    void toConsole(const QString& text);

    // Возвращает текущие даут и время
    QString getCurrentDateTime(const QString& spearator = "/");

    // Вовзаращает время, сконвертированное в строку
    QString getTime(quint16 big, quint16 small);

    // Возвращает текущее время в часах и минутах
    QString getCurrentTime();

    // Выполняет обновление текущего времени
    void workThreadCurrentTime();

    // Выполняет обновление времени периода
    void workThreadPeriodTime();

    // Выполняет первоначальную конфигурацию
    void setConfiguration();

    // Возвращает, введено ли корректное имя команды
    bool isCorrectTeamName(const QString& teamName);

Q_SIGNALS:

    // Событие для посылки сообщения веб-сокетом сервера
    void eventServerSocketSendTextMessage(const QString& data);

private Q_SLOTS:

    // Событие при подключении веб-соекта нового клиента
    void eventClientSocketConnected();

    // Событие при получении текстового сообщения сервером
    void eventClientTextMessageRecieved(const QString& data);

    // Событие при отключении веб-сокета существующего клиента
    void eventClientSocketDisconnected();

    // Событие, возникающее при поиске сервера (получение данных в UDP-сокет)
    void eventSearchServer();

    // Событие при нажатии на элемент списка пользователей
    void eventUsersListWidgetItemClicked(QListWidgetItem* item);

private:

    // Текущий порт подключения веб-сокета
    QString currentPort;

    // Указатель на окно сервера чата
    Ui::ServerWindow *ui;

    // Веб-сокет сервера
    QWebSocketServer* connectionWebSocketServer;

    // Локальный веб-сокет для посылки сообщений клиентам
    QWebSocket* serverWebSocket;

    // Список веб-сокетов клиентов
    QList<QPair<QWebSocket*, QString>> clientsWebSockets;

    // UDP-сокет для поиска сервера
    QUdpSocket searchUdpSocket;

    // Конец ключа шифрования
    QString startDateTime;

    // Выбран режим подробных сообщений
    bool debug;

    // Текущий пользователь для отправки
    QListWidgetItem* currentUserListWidgetItem;

    // Время периода
    QPair<quint16, quint16> periodTime;

    // Номер периода
    quint16 periodNumber;

    // Имя первой команды
    QString firstTeamName;

    // Очки первой команды
    quint16 firstTeamScore;

    // Имя второй команды
    QString secondTeamName;

    // Очки второй команды
    quint16 secondTeamScore;

    // Статус игры
    QString gameStatus;

    // Поток обновления текущего времени
    QThread* threadCurrentTime;

    // Поток обновления времени периода
    QThread* threadPeriodTime;

    // Режим работы потока обновления времени периода
    quint8 threadPeriodTimeMode;

    // Старое время сервера
    QString oldTime;

    // Старое время периода в минутах
    quint16 oldPeriodTimeMinute;
};
#endif // SERVERWINDOW_H

#ifndef CLIENTWINDOW_H
#define CLIENTWINDOW_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtWebSockets/QWebSocket>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QListWidgetItem>
#include <QFileDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientWindow; }
QT_END_NAMESPACE

// Класс окна клиента чата
class ClientWindow : public QMainWindow
{
    Q_OBJECT

public:

    // Конструктор окна клиента чата
    ClientWindow(bool debug = true, QWidget* parent = nullptr);

    // Деструктор клиента чата
    ~ClientWindow();

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

    // Станадратное имя окна
    const static QString WINDOW_TITLE;

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

    // Максимальное время периода
    static constexpr quint16 PERIOD_TIME_MAX_MINUTES = 19;

    // Максимальное время периода в секундах
    static constexpr quint16 PERIOD_TIME_MAX_SECONDS = 59;

    // Получить имя первой команды
    const static QString NAME_FIRST;

    // Получить имя второй команды
    const static QString NAME_SECOND;

    // Стандартное время
    const static QString TIME_DEFAULT;

    // Стандартное имя первой команды
    const static QString NAME_FIRST_DEFAULT;

    // Стандартное имя второй команды
    const static QString NAME_SECOND_DEFAULT;

    // Выводит сообщение в консоль
    void toConsole(const QString& text);

    // Посылает сообщения для поиска сервера в сети
    void doSendMessageToSearchServer();

    // Возвращает, введено ли корректное имя команды
    bool isCorrectTeamName(const QString& teamName);

    // Возвращает текущие дату и время
    QString getCurrentDateTime(const QString& spearator = "/");

    // Возвращает, готов ли клиент к запуску игры
    bool isReadyToGameStart();

private Q_SLOTS:

    // Событие, возникающее при получении текстового сообщения
    void eventTextMessageRecieved(const QString& data);

    // Событие, возникающее при завершении подключения веб-сокета клиента к серверу
    void eventConnected();

    // Событие, возникающее при поиске сервера (получение данных в UDP-сокет)
    void eventSearchServer();

    // Событие, возникающее при завершении подключения веб-сокета клиента к серверу
    void eventDisconnected();

    // Событие, возникающее при нажатии кнопки начала игры
    void eventGeneralGameStartPushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки завершения игры
    void eventGeneralGameStopPushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки приостановки игры
    void eventGeneralGamePausePushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки продолжения игры
    void eventGeneralGameContinuePushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки добавить у первой команды
    void eventFirstTeamScoreAddPushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки отнять у первой команды
    void eventFirstTeamScoreSubtractPushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки добавить у второй команды
    void eventSecondTeamScoreAddPushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки отнять у второй команды
    void eventSecondTeamScoreSubtractPushButtonClicked(bool click = true);

    // Событие, возникающее при нажатии кнопки возврата к предыдущему периоду
    void eventGeneralPeriodPreviousPushButton(bool click = true);

    // Событие, возникающее при нажатии кнопки перехода к следующему периоду
    void eventGeneralPeriodNextPushButton(bool click = true);

    // Событие, возникающее при редактировании имен команд
    void eventTeamsNamesTextChanged(const QString& text);

private:

    // Указатель на окно клиента чата
    Ui::ClientWindow* ui;

    // Веб-сокет клиента
    QWebSocket clientWebSocket;

    // UDP-сокет для поиска сервера
    QUdpSocket searchUdpSocket;

    // URL сервера
    QUrl serverUrl;

    // IP клиента
    QString clientIP;

    // Время подключения клиента
    QString connectionDateTime;

    // Найден ли адрес сервера
    bool serverAddressFound;

    // Выбран режим подробных сообщений
    bool debug;

    // Статус игры
    QString gameStatus;

    // Старое время периода
    QString oldPeriodTimeMinutes;
};
#endif // CLIENTWINDOW_H

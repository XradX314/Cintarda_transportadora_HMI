#pragma once
#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include "protocol.h"
#include "cintatracker.h"

class QLabel;
class QPushButton;
class QComboBox;
class QSpinBox;
class QTextEdit;
class QGroupBox;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectToggle();
    void onRefreshPorts();
    void onSerialDataReceived();
    void onHeartbeatTimeout();

    // Comandos de control
    void onStartNormal();
    void onStartBlind();
    void onStop();

    // Comandos de configuración
    void onSetPiso();
    void onSetUmbrales();
    void onSetDelay0();
    void onSetDelay1();
    void onSetDelay2();
    void onSetGeometriaCiega();
    void onCalibrate();

private:
    // ── Construcción de UI ────────────────────────────────────────────────────
    void     setupUI();
    QWidget* buildConnectionBar();
    QWidget* buildControlPanel();
    QWidget* buildTelemetryPanel();
    QWidget* buildConfigBasic();
    QWidget* buildConfigEjectors();
    QWidget* buildConfigBlind();
    QWidget* buildTrackingPanel();
    QWidget* buildLogPanel();

    // ── Comunicación ──────────────────────────────────────────────────────────
    void sendCommand(uint8_t cmd, const QByteArray& payload = {});
    void processFrame(const Protocol::Frame& frame, const QByteArray& raw);
    void setEjectorDelay(int idx);

    // ── Logging ───────────────────────────────────────────────────────────────
    void logTx(const QByteArray& raw, const QString& desc);
    void logRx(const QByteArray& raw, const QString& desc);
    void logInfo(const QString& msg);
    void logWarn(const QString& msg);
    void appendLog(const QString& html);

    // ── Estado ────────────────────────────────────────────────────────────────
    void updateSystemState(const QString& label, const QString& color);
    void updateControls();
    void refreshPortList();

    enum SysState { OFF, NORMAL, BLIND };

    QSerialPort* m_serial;
    QByteArray   m_rxBuf;
    QTimer*      m_hbTimer;
    SysState     m_state   = OFF;
    bool         m_connected = false;

    // ── Widgets: Conexión ─────────────────────────────────────────────────────
    QComboBox*   m_portCombo;
    QPushButton* m_refreshBtn;
    QPushButton* m_connectBtn;
    QLabel*      m_connLight;
    QLabel*      m_connLabel;

    // ── Widgets: Control ──────────────────────────────────────────────────────
    QLabel*      m_stateLabel;
    QPushButton* m_startNormalBtn;
    QPushButton* m_startBlindBtn;
    QPushButton* m_stopBtn;

    // ── Widgets: Telemetría ───────────────────────────────────────────────────
    QLabel*      m_distLabel;
    QLabel*      m_irLabel[4];
    QLabel*      m_hbLight;
    QLabel*      m_hbTimeLabel;

    // ── Widgets: Config Básica ────────────────────────────────────────────────
    QSpinBox*    m_pisoSpin;
    QSpinBox*    m_umbralChicaSpin;
    QSpinBox*    m_umbralMediaSpin;
    QSpinBox*    m_umbralGrandeSpin;
    QSpinBox*    m_toleranciaSpin;
    QLabel*      m_calibResultLabel;
    QPushButton* m_calibBtn;

    // ── Widgets: Config Eyectores ─────────────────────────────────────────────
    QComboBox*   m_ejTypeCombo[3];
    QSpinBox*    m_ejDelaySpin[3];

    // ── Widgets: Config Modo Ciego ────────────────────────────────────────────
    QSpinBox*    m_ciegaLargoSpin;
    QSpinBox*    m_ciegaDSpin[3];
    QSpinBox*    m_ciegaOffsetSpin;

    // ── Widgets: Log ─────────────────────────────────────────────────────────
    QTextEdit*   m_logEdit;

    // ── Panel de config (para enable/disable global) ──────────────────────────
    QWidget*     m_configWidget;

    // ── Tracker visual ────────────────────────────────────────────────────────
    CintaTracker* m_tracker;
};

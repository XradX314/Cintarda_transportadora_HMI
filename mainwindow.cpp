#include "mainwindow.h"

#include <QApplication>
#include <QWidget>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QTabWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QFrame>
#include <QTime>
#include <QFont>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QScrollBar>

// ════════════════════════════════════════════════════════════════════════════
//  Helpers de estilo
// ════════════════════════════════════════════════════════════════════════════

static QString sectionStyle()
{
    return "QGroupBox { font-weight: bold; border: 1px solid #555; border-radius:4px;"
           " margin-top: 8px; padding-top: 6px; }"
           "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }";
}

static QLabel* makeDot(const QString& color)
{
    auto* lbl = new QLabel("●");
    lbl->setStyleSheet(QString("color: %1; font-size: 18px;").arg(color));
    lbl->setFixedWidth(22);
    return lbl;
}

// ════════════════════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_serial  = new QSerialPort(this);
    m_hbTimer = new QTimer(this);
    m_hbTimer->setInterval(9000);   // 9 s: heartbeat cada 5 s + 4 s tolerancia

    setupUI();

    connect(m_serial,  &QSerialPort::readyRead,  this, &MainWindow::onSerialDataReceived);
    connect(m_hbTimer, &QTimer::timeout,         this, &MainWindow::onHeartbeatTimeout);

    setWindowTitle("HMI – Cinta Transportadora (Protocolo UNER)");
    resize(1100, 780);
    updateControls();
}

MainWindow::~MainWindow() {}

// ════════════════════════════════════════════════════════════════════════════
//  Construcción de la UI
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::setupUI()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* outerVBox = new QVBoxLayout(central);
    outerVBox->setContentsMargins(6, 6, 6, 6);
    outerVBox->setSpacing(4);

    // Barra de conexión
    outerVBox->addWidget(buildConnectionBar());

    // Splitter vertical: contenido | log
    auto* vSplitter = new QSplitter(Qt::Vertical, central);
    vSplitter->setChildrenCollapsible(false);

    // Splitter horizontal: izquierda (control+telemetría) | derecha (config)
    auto* hSplitter = new QSplitter(Qt::Horizontal, vSplitter);
    hSplitter->setChildrenCollapsible(false);

    // Panel izquierdo
    auto* leftWidget = new QWidget;
    auto* leftVBox   = new QVBoxLayout(leftWidget);
    leftVBox->setContentsMargins(0, 0, 0, 0);
    leftVBox->addWidget(buildControlPanel());
    leftVBox->addWidget(buildTelemetryPanel());
    leftVBox->addStretch();
    leftWidget->setMaximumWidth(230);
    hSplitter->addWidget(leftWidget);

    // Panel derecho: tabs de configuración + tracking
    auto* tabs = new QTabWidget;
    tabs->addTab(buildTrackingPanel(),  "Cinta / Tracking");
    tabs->addTab(buildConfigBasic(),    "Config Básica");
    tabs->addTab(buildConfigEjectors(), "Eyectores");
    tabs->addTab(buildConfigBlind(),    "Modo Ciego");
    m_configWidget = tabs;
    hSplitter->addWidget(tabs);
    hSplitter->setStretchFactor(1, 1);

    vSplitter->addWidget(hSplitter);
    vSplitter->addWidget(buildLogPanel());
    vSplitter->setStretchFactor(0, 2);
    vSplitter->setStretchFactor(1, 1);

    outerVBox->addWidget(vSplitter);
    statusBar()->showMessage("Desconectado");
}

// ── Barra de conexión ────────────────────────────────────────────────────────

QWidget* MainWindow::buildConnectionBar()
{
    auto* bar   = new QFrame;
    bar->setFrameShape(QFrame::StyledPanel);
    auto* hbox  = new QHBoxLayout(bar);
    hbox->setContentsMargins(6, 4, 6, 4);

    hbox->addWidget(new QLabel("<b>Puerto:</b>"));

    m_portCombo = new QComboBox;
    m_portCombo->setMinimumWidth(120);
    hbox->addWidget(m_portCombo);

    m_refreshBtn = new QPushButton("↺");
    m_refreshBtn->setToolTip("Refrescar lista de puertos");
    m_refreshBtn->setFixedWidth(32);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    hbox->addWidget(m_refreshBtn);

    hbox->addWidget(new QLabel("  Baud: <b>115200</b>  "));

    m_connectBtn = new QPushButton("Conectar");
    m_connectBtn->setMinimumWidth(90);
    m_connectBtn->setStyleSheet("QPushButton { background:#2d862d; color:white; font-weight:bold; border-radius:4px; padding:4px 12px; }"
                                "QPushButton:hover { background:#3aa83a; }");
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectToggle);
    hbox->addWidget(m_connectBtn);

    m_connLight = makeDot("#cc3333");
    hbox->addWidget(m_connLight);

    m_connLabel = new QLabel("Desconectado");
    m_connLabel->setStyleSheet("color: #cc3333;");
    hbox->addWidget(m_connLabel);

    hbox->addStretch();

    refreshPortList();
    return bar;
}

// ── Panel de control ─────────────────────────────────────────────────────────

QWidget* MainWindow::buildControlPanel()
{
    auto* grp  = new QGroupBox("Control del Sistema");
    grp->setStyleSheet(sectionStyle());
    auto* vbox = new QVBoxLayout(grp);

    // Estado
    auto* stateBox = new QHBoxLayout;
    stateBox->addWidget(new QLabel("Estado:"));
    m_stateLabel = new QLabel("APAGADO");
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_stateLabel->setStyleSheet(
        "background:#444; color:#aaa; font-weight:bold; font-size:13px;"
        " border-radius:4px; padding:4px 8px;");
    stateBox->addWidget(m_stateLabel);
    vbox->addLayout(stateBox);

    vbox->addSpacing(8);

    m_startNormalBtn = new QPushButton("▶  Iniciar Normal");
    m_startNormalBtn->setStyleSheet(
        "QPushButton{background:#1a6b1a;color:white;font-weight:bold;"
        "border-radius:4px;padding:6px;}"
        "QPushButton:hover{background:#228b22;}"
        "QPushButton:disabled{background:#333;color:#666;}");
    connect(m_startNormalBtn, &QPushButton::clicked, this, &MainWindow::onStartNormal);
    vbox->addWidget(m_startNormalBtn);

    m_startBlindBtn = new QPushButton("▶  Iniciar Modo Ciego");
    m_startBlindBtn->setStyleSheet(
        "QPushButton{background:#1a4a8a;color:white;font-weight:bold;"
        "border-radius:4px;padding:6px;}"
        "QPushButton:hover{background:#2255a0;}"
        "QPushButton:disabled{background:#333;color:#666;}");
    connect(m_startBlindBtn, &QPushButton::clicked, this, &MainWindow::onStartBlind);
    vbox->addWidget(m_startBlindBtn);

    m_stopBtn = new QPushButton("■  Detener Sistema");
    m_stopBtn->setStyleSheet(
        "QPushButton{background:#8b1a1a;color:white;font-weight:bold;"
        "border-radius:4px;padding:6px;}"
        "QPushButton:hover{background:#b02020;}"
        "QPushButton:disabled{background:#333;color:#666;}");
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    vbox->addWidget(m_stopBtn);

    return grp;
}

// ── Panel de telemetría ──────────────────────────────────────────────────────

QWidget* MainWindow::buildTelemetryPanel()
{
    auto* grp  = new QGroupBox("Telemetría en Tiempo Real");
    grp->setStyleSheet(sectionStyle());
    auto* vbox = new QVBoxLayout(grp);

    // Distancia ultrasonido
    vbox->addWidget(new QLabel("Distancia (HC-SR04):"));
    m_distLabel = new QLabel("--- mm");
    m_distLabel->setAlignment(Qt::AlignCenter);
    m_distLabel->setStyleSheet(
        "font-size:26px; font-weight:bold; color:#00ccff;"
        " background:#111; border-radius:4px; padding:6px;");
    vbox->addWidget(m_distLabel);

    vbox->addSpacing(6);

    // Sensores IR (S0–S3)
    vbox->addWidget(new QLabel("Sensores IR (TCRT5000):"));
    auto* irGrid = new QGridLayout;
    irGrid->setSpacing(4);
    const QString irNames[] = {"S0 (Entrada)", "S1 (Ey.0)", "S2 (Ey.1)", "S3 (Ey.2)"};
    for (int i = 0; i < 4; i++) {
        m_irLabel[i] = new QLabel(irNames[i] + " ○");
        m_irLabel[i]->setAlignment(Qt::AlignCenter);
        m_irLabel[i]->setStyleSheet(
            "background:#2b2b2b; color:#888; border-radius:4px;"
            " padding:3px 6px; font-size:11px;");
        irGrid->addWidget(m_irLabel[i], i / 2, i % 2);
    }
    vbox->addLayout(irGrid);

    vbox->addSpacing(6);

    // Heartbeat
    auto* hbRow = new QHBoxLayout;
    hbRow->addWidget(new QLabel("Heartbeat:"));
    m_hbLight = makeDot("#555");
    hbRow->addWidget(m_hbLight);
    m_hbTimeLabel = new QLabel("--:--:--");
    m_hbTimeLabel->setStyleSheet("color:#888; font-size:11px;");
    hbRow->addWidget(m_hbTimeLabel);
    hbRow->addStretch();
    vbox->addLayout(hbRow);

    return grp;
}

// ── Config: Básica ───────────────────────────────────────────────────────────

QWidget* MainWindow::buildConfigBasic()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setAlignment(Qt::AlignTop);

    // ─ Piso ─
    auto* grpPiso = new QGroupBox("Piso de Referencia (HC-SR04 sin caja)");
    grpPiso->setStyleSheet(sectionStyle());
    auto* pisoForm = new QHBoxLayout(grpPiso);
    pisoForm->addWidget(new QLabel("Distancia al piso:"));
    m_pisoSpin = new QSpinBox;
    m_pisoSpin->setRange(0, 255);
    m_pisoSpin->setValue(180);
    m_pisoSpin->setSuffix(" mm");
    pisoForm->addWidget(m_pisoSpin);
    auto* pisoBtn = new QPushButton("Aplicar");
    connect(pisoBtn, &QPushButton::clicked, this, &MainWindow::onSetPiso);
    pisoForm->addWidget(pisoBtn);
    pisoForm->addStretch();
    vbox->addWidget(grpPiso);

    // ─ Umbrales ─
    auto* grpUmb = new QGroupBox("Umbrales de Clasificación por Altura");
    grpUmb->setStyleSheet(sectionStyle());
    auto* umbForm = new QFormLayout(grpUmb);
    m_umbralChicaSpin = new QSpinBox; m_umbralChicaSpin->setRange(0,255); m_umbralChicaSpin->setValue(120); m_umbralChicaSpin->setSuffix(" mm");
    m_umbralMediaSpin = new QSpinBox; m_umbralMediaSpin->setRange(0,255); m_umbralMediaSpin->setValue(100); m_umbralMediaSpin->setSuffix(" mm");
    m_umbralGrandeSpin= new QSpinBox; m_umbralGrandeSpin->setRange(0,255);m_umbralGrandeSpin->setValue(80);  m_umbralGrandeSpin->setSuffix(" mm");
    m_toleranciaSpin  = new QSpinBox; m_toleranciaSpin->setRange(0,255);  m_toleranciaSpin->setValue(10);   m_toleranciaSpin->setSuffix(" mm");
    umbForm->addRow("Umbral caja chica:",   m_umbralChicaSpin);
    umbForm->addRow("Umbral caja mediana:", m_umbralMediaSpin);
    umbForm->addRow("Umbral caja grande:",  m_umbralGrandeSpin);
    umbForm->addRow("Tolerancia:",          m_toleranciaSpin);
    auto* umbBtn = new QPushButton("Aplicar Umbrales");
    connect(umbBtn, &QPushButton::clicked, this, &MainWindow::onSetUmbrales);
    umbForm->addRow("", umbBtn);
    vbox->addWidget(grpUmb);

    // ─ Calibración ─
    auto* grpCalib = new QGroupBox("Calibración (promedio de 5 mediciones)");
    grpCalib->setStyleSheet(sectionStyle());
    auto* calibHbox = new QHBoxLayout(grpCalib);
    m_calibBtn = new QPushButton("Medir (5 muestras)");
    m_calibBtn->setStyleSheet(
        "QPushButton{background:#555533;color:white;border-radius:4px;padding:5px 12px;}"
        "QPushButton:hover{background:#777744;}"
        "QPushButton:disabled{background:#333;color:#666;}");
    connect(m_calibBtn, &QPushButton::clicked, this, &MainWindow::onCalibrate);
    calibHbox->addWidget(m_calibBtn);
    calibHbox->addWidget(new QLabel("Resultado:"));
    m_calibResultLabel = new QLabel("---");
    m_calibResultLabel->setStyleSheet(
        "font-size:18px; font-weight:bold; color:#ffcc00;"
        " background:#111; border-radius:4px; padding:4px 10px;");
    calibHbox->addWidget(m_calibResultLabel);
    calibHbox->addStretch();
    vbox->addWidget(grpCalib);

    vbox->addStretch();
    return w;
}

// ── Config: Eyectores ────────────────────────────────────────────────────────

QWidget* MainWindow::buildConfigEjectors()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setAlignment(Qt::AlignTop);

    const QString names[3] = {"Eyector 0 (Servo 1 – SG90)", "Eyector 1 (Servo 2 – SG90)", "Eyector 2 (Servo 3 – SG90)"};
    const QStringList boxTypes = {"NONE (desactivado)", "CHICA", "MEDIANA", "GRANDE"};

    for (int i = 0; i < 3; i++) {
        auto* grp  = new QGroupBox(names[i]);
        grp->setStyleSheet(sectionStyle());
        auto* form = new QFormLayout(grp);

        m_ejTypeCombo[i] = new QComboBox;
        m_ejTypeCombo[i]->addItems(boxTypes);
        m_ejTypeCombo[i]->setCurrentIndex(i + 1);   // default: 0→CHICA, 1→MEDIANA, 2→GRANDE
        form->addRow("Tipo de caja asignada:", m_ejTypeCombo[i]);

        m_ejDelaySpin[i] = new QSpinBox;
        m_ejDelaySpin[i]->setRange(0, 9999);
        m_ejDelaySpin[i]->setValue(500 + i * 200);
        m_ejDelaySpin[i]->setSuffix(" ms");
        m_ejDelaySpin[i]->setToolTip("Retardo entre detección en IR y activación del servo");
        form->addRow("Delay post-detección:", m_ejDelaySpin[i]);

        auto* btn = new QPushButton(QString("Aplicar Eyector %1").arg(i));
        connect(btn, &QPushButton::clicked, this, [this, i]{ setEjectorDelay(i); });
        form->addRow("", btn);

        vbox->addWidget(grp);
    }

    auto* note = new QLabel(
        "<i>Los delays se aplican cuando se detecta la caja en el sensor IR del eyector.<br>"
        "Servo: 0° = retraído, 90° = extendido (400 ms), luego retorno (200 ms).</i>");
    note->setWordWrap(true);
    note->setStyleSheet("color:#888; font-size:11px;");
    vbox->addWidget(note);
    vbox->addStretch();
    return w;
}

// ── Config: Modo Ciego ───────────────────────────────────────────────────────

QWidget* MainWindow::buildConfigBlind()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setAlignment(Qt::AlignTop);

    auto* grp  = new QGroupBox("Geometría del Sistema (Modo Ciego)");
    grp->setStyleSheet(sectionStyle());
    auto* form = new QFormLayout(grp);

    m_ciegaLargoSpin = new QSpinBox; m_ciegaLargoSpin->setRange(1,255); m_ciegaLargoSpin->setValue(8);  m_ciegaLargoSpin->setSuffix(" cm");
    m_ciegaDSpin[0]  = new QSpinBox; m_ciegaDSpin[0]->setRange(1,255);  m_ciegaDSpin[0]->setValue(10); m_ciegaDSpin[0]->setSuffix(" cm");
    m_ciegaDSpin[1]  = new QSpinBox; m_ciegaDSpin[1]->setRange(1,255);  m_ciegaDSpin[1]->setValue(20); m_ciegaDSpin[1]->setSuffix(" cm");
    m_ciegaDSpin[2]  = new QSpinBox; m_ciegaDSpin[2]->setRange(1,255);  m_ciegaDSpin[2]->setValue(30); m_ciegaDSpin[2]->setSuffix(" cm");
    m_ciegaOffsetSpin= new QSpinBox; m_ciegaOffsetSpin->setRange(0,255);m_ciegaOffsetSpin->setValue(2);m_ciegaOffsetSpin->setSuffix(" cm");

    form->addRow("Largo de caja:",              m_ciegaLargoSpin);
    form->addRow("Distancia IR0 → Servo 0:",    m_ciegaDSpin[0]);
    form->addRow("Distancia IR0 → Servo 1:",    m_ciegaDSpin[1]);
    form->addRow("Distancia IR0 → Servo 2:",    m_ciegaDSpin[2]);
    form->addRow("Offset de impacto:",          m_ciegaOffsetSpin);

    auto* btn = new QPushButton("Aplicar Geometría");
    connect(btn, &QPushButton::clicked, this, &MainWindow::onSetGeometriaCiega);
    form->addRow("", btn);

    vbox->addWidget(grp);

    auto* note = new QLabel(
        "<b>Modo Ciego:</b> el sistema mide la velocidad de la caja usando el tiempo de tránsito "
        "a través del sensor S0 y calcula dinámicamente el instante de disparo de cada servo.<br><br>"
        "<b>Fórmula:</b> v = largo_caja / t_tránsito<br>"
        "delay_servo_n = (distancia_n / v) + offset_impacto / v");
    note->setWordWrap(true);
    note->setStyleSheet("color:#aaa; font-size:11px; padding:6px;");
    vbox->addWidget(note);
    vbox->addStretch();
    return w;
}

// ── Panel de Tracking (Cinta) ────────────────────────────────────────────────

QWidget* MainWindow::buildTrackingPanel()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(6);

    // Título
    auto* title = new QLabel("<b>Visualización de la Cinta Transportadora</b>");
    title->setStyleSheet("color:#aaa; font-size:12px; padding:2px;");
    vbox->addWidget(title);

    // Widget principal de la cinta
    m_tracker = new CintaTracker;
    m_tracker->setStyleSheet(
        "border: 1px solid #444; border-radius: 6px; background: #16181e;");
    vbox->addWidget(m_tracker, 1);

    // Controles inferiores
    auto* ctrlRow = new QHBoxLayout;

    auto* resetBtn = new QPushButton("↺  Reiniciar Tracking");
    resetBtn->setStyleSheet(
        "QPushButton{background:#3a3a1a;color:#dddd44;border-radius:4px;padding:5px 14px;}"
        "QPushButton:hover{background:#555522;}");
    connect(resetBtn, &QPushButton::clicked, this, [this]{
        m_tracker->reset();
        logInfo("Tracking reiniciado manualmente.");
    });
    ctrlRow->addWidget(resetBtn);
    ctrlRow->addStretch();

    auto* note = new QLabel(
        "<font color='#888'>Cada trama 0x5D del µC actualiza el estado en tiempo real.</font>");
    note->setStyleSheet("font-size:10px;");
    ctrlRow->addWidget(note);

    vbox->addLayout(ctrlRow);

    // Conectar mensajes del tracker al log
    connect(m_tracker, &CintaTracker::statusMessage, this, &MainWindow::logInfo);

    return w;
}

// ── Panel de Log ─────────────────────────────────────────────────────────────

QWidget* MainWindow::buildLogPanel()
{
    auto* frame = new QFrame;
    frame->setFrameShape(QFrame::StyledPanel);
    auto* vbox = new QVBoxLayout(frame);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("<b>Log / Terminal USART</b>");
    titleRow->addWidget(title);
    titleRow->addStretch();

    auto* clearBtn = new QPushButton("Limpiar");
    clearBtn->setFixedWidth(70);
    connect(clearBtn, &QPushButton::clicked, this, [this]{ m_logEdit->clear(); });
    titleRow->addWidget(clearBtn);
    vbox->addLayout(titleRow);

    m_logEdit = new QTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Courier New", 9));
    m_logEdit->setStyleSheet(
        "background:#0d0d0d; color:#cccccc;"
        " border: 1px solid #333; border-radius:4px;");
    m_logEdit->document()->setMaximumBlockCount(2000);
    vbox->addWidget(m_logEdit);

    // Leyenda de colores
    auto* legend = new QLabel(
        "<font color='#5599ff'>■ TX</font>  "
        "<font color='#44dd88'>■ RX</font>  "
        "<font color='#ffdd44'>■ INFO</font>  "
        "<font color='#ff6644'>■ WARN/ERROR</font>");
    legend->setStyleSheet("font-size:10px;");
    vbox->addWidget(legend);

    return frame;
}

// ════════════════════════════════════════════════════════════════════════════
//  Conexión serial
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::refreshPortList()
{
    QString current = m_portCombo->currentText();
    m_portCombo->clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
        m_portCombo->addItem(info.portName());
    int idx = m_portCombo->findText(current);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
}

void MainWindow::onRefreshPorts()
{
    refreshPortList();
    logInfo("Lista de puertos actualizada.");
}

void MainWindow::onConnectToggle()
{
    if (m_connected) {
        m_serial->close();
        m_connected = false;
        m_hbTimer->stop();
        m_state = OFF;
        m_connLight->setStyleSheet("color:#cc3333; font-size:18px;");
        m_connLabel->setStyleSheet("color:#cc3333;");
        m_connLabel->setText("Desconectado");
        m_connectBtn->setText("Conectar");
        m_connectBtn->setStyleSheet(
            "QPushButton{background:#2d862d;color:white;font-weight:bold;"
            "border-radius:4px;padding:4px 12px;}"
            "QPushButton:hover{background:#3aa83a;}");
        updateSystemState("APAGADO", "#888888");
        updateControls();
        statusBar()->showMessage("Desconectado");
        logWarn("Puerto cerrado.");
    } else {
        QString port = m_portCombo->currentText();
        if (port.isEmpty()) {
            QMessageBox::warning(this, "Error", "Seleccione un puerto serial.");
            return;
        }
        m_serial->setPortName(port);
        m_serial->setBaudRate(QSerialPort::Baud115200);
        m_serial->setDataBits(QSerialPort::Data8);
        m_serial->setParity(QSerialPort::NoParity);
        m_serial->setStopBits(QSerialPort::OneStop);
        m_serial->setFlowControl(QSerialPort::NoFlowControl);

        if (!m_serial->open(QIODevice::ReadWrite)) {
            QMessageBox::critical(this, "Error",
                "No se pudo abrir " + port + ":\n" + m_serial->errorString());
            return;
        }

        // Evitar reset del AVR por flanco en DTR (circuit auto-reset de adaptadores USB-UART)
        m_serial->setDataTerminalReady(false);
        m_serial->setRequestToSend(false);

        m_connected = true;
        m_rxBuf.clear();
        m_connLight->setStyleSheet("color:#33cc33; font-size:18px;");
        m_connLabel->setStyleSheet("color:#33cc33;");
        m_connLabel->setText("Conectado – " + port);
        m_connectBtn->setText("Desconectar");
        m_connectBtn->setStyleSheet(
            "QPushButton{background:#882222;color:white;font-weight:bold;"
            "border-radius:4px;padding:4px 12px;}"
            "QPushButton:hover{background:#aa2222;}");
        m_hbTimer->start();
        updateControls();
        statusBar()->showMessage("Conectado en " + port + " @ 115200");
        logInfo(QString("Puerto %1 abierto @ 115200 bps. DTR/RTS desactivados.").arg(port));
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Envío de comandos
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::sendCommand(uint8_t cmd, const QByteArray& payload)
{
    if (!m_connected) {
        logWarn("No conectado. Comando ignorado.");
        return;
    }
    QByteArray raw = Protocol::encode(cmd, payload);

    qint64 written = m_serial->write(raw);

    // En Windows, write() solo llena el buffer del driver; flush fuerza la transmisión.
    if (!m_serial->waitForBytesWritten(200)) {
        logWarn(QString("Timeout al enviar %1 (¿cable desconectado?)").arg(Protocol::cmdName(cmd)));
        return;
    }

    if (written != raw.size()) {
        logWarn(QString("TX incompleto: %1/%2 bytes – %3")
                .arg(written).arg(raw.size()).arg(Protocol::cmdName(cmd)));
        return;
    }

    logTx(raw, Protocol::cmdName(cmd));
}

// ── Slots de control ─────────────────────────────────────────────────────────

void MainWindow::onStartNormal()
{
    sendCommand(Protocol::CMD_START_SISTEMA);
    logInfo("Solicitado: Iniciar modo normal. Esperando ACK…");
}

void MainWindow::onStartBlind()
{
    sendCommand(Protocol::CMD_START_CIEGO);
    logInfo("Solicitado: Iniciar modo ciego. Esperando ACK…");
}

void MainWindow::onStop()
{
    sendCommand(Protocol::CMD_STOP_SISTEMA);
    logInfo("Solicitado: Detener sistema. Esperando ACK…");
}

// ── Slots de configuración ────────────────────────────────────────────────────

void MainWindow::onSetPiso()
{
    QByteArray p;
    p.append(char(m_pisoSpin->value()));
    sendCommand(Protocol::CMD_SET_PISO, p);
}

void MainWindow::onSetUmbrales()
{
    QByteArray p;
    p.append(char(m_umbralChicaSpin->value()));
    p.append(char(m_umbralMediaSpin->value()));
    p.append(char(m_umbralGrandeSpin->value()));
    p.append(char(m_toleranciaSpin->value()));
    sendCommand(Protocol::CMD_SET_UMBRALES, p);
}

void MainWindow::setEjectorDelay(int idx)
{
    uint8_t  tipo     = (uint8_t)m_ejTypeCombo[idx]->currentIndex();
    uint32_t delay_ms = (uint32_t)m_ejDelaySpin[idx]->value();

    QByteArray p;
    p.append(char(idx));
    p.append(char(tipo));
    // uint32_t en little-endian
    p.append(char( delay_ms        & 0xFF));
    p.append(char((delay_ms >>  8) & 0xFF));
    p.append(char((delay_ms >> 16) & 0xFF));
    p.append(char((delay_ms >> 24) & 0xFF));
    sendCommand(Protocol::CMD_SET_DELAYS, p);
}

void MainWindow::onSetDelay0() { setEjectorDelay(0); }
void MainWindow::onSetDelay1() { setEjectorDelay(1); }
void MainWindow::onSetDelay2() { setEjectorDelay(2); }

void MainWindow::onSetGeometriaCiega()
{
    QByteArray p;
    p.append(char(m_ciegaLargoSpin->value()));
    p.append(char(m_ciegaDSpin[0]->value()));
    p.append(char(m_ciegaDSpin[1]->value()));
    p.append(char(m_ciegaDSpin[2]->value()));
    p.append(char(m_ciegaOffsetSpin->value()));
    sendCommand(Protocol::CMD_SET_GEOMETRIA_CIEGA, p);
}

void MainWindow::onCalibrate()
{
    m_calibResultLabel->setText("…");
    sendCommand(Protocol::CMD_MEDIR_CALIBRACION);
    logInfo("Calibración iniciada (5 mediciones). Esperando resultado…");
}

// ════════════════════════════════════════════════════════════════════════════
//  Recepción y parsing de frames
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::onSerialDataReceived()
{
    m_rxBuf += m_serial->readAll();

    Protocol::Frame frame;
    QByteArray      rawFrame;
    while (Protocol::tryDecode(m_rxBuf, frame, &rawFrame))
        processFrame(frame, rawFrame);
}

void MainWindow::processFrame(const Protocol::Frame& frame, const QByteArray& raw)
{
    QString desc;

    switch (frame.cmd) {

    // ── ACK ──────────────────────────────────────────────────────────────────
    case Protocol::CMD_ACK:
        if (frame.payload.size() >= 1) {
            uint8_t acked = (uint8_t)frame.payload[0];
            desc = QString("ACK → %1").arg(Protocol::cmdName(acked));
            switch (acked) {
            case Protocol::CMD_START_SISTEMA:
                m_state = NORMAL;
                updateSystemState("NORMAL", "#33cc33");
                break;
            case Protocol::CMD_STOP_SISTEMA:
                m_state = OFF;
                updateSystemState("APAGADO", "#888888");
                break;
            case Protocol::CMD_START_CIEGO:
                m_state = BLIND;
                updateSystemState("MODO CIEGO", "#5599ff");
                break;
            default:
                break;
            }
            updateControls();
        }
        break;

    // ── Respuesta a calibración ───────────────────────────────────────────────
    case Protocol::CMD_MEDIR_CALIBRACION:
        if (frame.payload.size() >= 1) {
            uint8_t dist = (uint8_t)frame.payload[0];
            desc = QString("CALIB_RESULT = %1 mm").arg(dist);
            m_calibResultLabel->setText(QString("%1 mm").arg(dist));
            logInfo(QString("Calibración completada: distancia promedio = %1 mm").arg(dist));
        }
        break;

    // ── Telemetría: distancia ─────────────────────────────────────────────────
    case Protocol::CMD_TELEMETRY_DIST:
        if (frame.payload.size() >= 1) {
            uint8_t d = (uint8_t)frame.payload[0];
            desc = QString("DIST = %1 mm").arg(d);
            m_distLabel->setText(QString("%1 mm").arg(d));
        }
        break;

    // ── Telemetría: estado IR ─────────────────────────────────────────────────
    case Protocol::CMD_TELEMETRY_IR:
        if (frame.payload.size() >= 2) {
            uint8_t idx   = (uint8_t)frame.payload[0];
            uint8_t state = (uint8_t)frame.payload[1];
            if (idx < 4) {
                const QString sNames[] = {"S0 (Entrada)", "S1 (Ey.0)", "S2 (Ey.1)", "S3 (Ey.2)"};
                desc = QString("IR %1 = %2").arg(sNames[idx]).arg(state ? "DETECTADO" : "LIBRE");
                if (state) {
                    m_irLabel[idx]->setStyleSheet(
                        "background:#1a5c1a; color:#55ff55; border-radius:4px;"
                        " padding:3px 6px; font-size:11px; font-weight:bold;");
                    m_irLabel[idx]->setText(sNames[idx] + " ●");
                } else {
                    m_irLabel[idx]->setStyleSheet(
                        "background:#2b2b2b; color:#888; border-radius:4px;"
                        " padding:3px 6px; font-size:11px;");
                    m_irLabel[idx]->setText(sNames[idx] + " ○");
                }
            }
        }
        break;

    // ── Tracker 0x5D ────────────────────────────────────────────────────────
    case Protocol::CMD_TRACKER:
        if (frame.payload.size() >= 3) {
            uint8_t id   = (uint8_t)frame.payload[0];
            uint8_t tipo = (uint8_t)frame.payload[1];
            uint8_t zona = (uint8_t)frame.payload[2];
            desc = QString("TRACKER id=#%1  %2  %3")
                   .arg(id)
                   .arg(Protocol::boxName(tipo))
                   .arg(Protocol::zoneName(zona));
            m_tracker->handleTracker(id, tipo, zona);
        } else {
            desc = "TRACKER (payload inválido — se esperan 3 bytes)";
            logWarn("CMD_TRACKER con payload incompleto.");
        }
        break;

    // ── Heartbeat ─────────────────────────────────────────────────────────────
    case Protocol::CMD_HEARTBEAT:
        desc = "HEARTBEAT";
        m_hbTimer->start();   // reiniciar timeout
        m_hbLight->setStyleSheet("color:#33cc33; font-size:18px;");
        m_hbTimeLabel->setText(QTime::currentTime().toString("hh:mm:ss"));
        // Apagar el indicador después de 500 ms
        QTimer::singleShot(500, this, [this]{
            m_hbLight->setStyleSheet("color:#555; font-size:18px;");
        });
        break;

    default:
        desc = QString("DESCONOCIDO cmd=0x%1").arg(frame.cmd, 2, 16, QChar('0')).toUpper();
        break;
    }

    logRx(raw, desc);
}

void MainWindow::onHeartbeatTimeout()
{
    m_hbLight->setStyleSheet("color:#cc3333; font-size:18px;");
    m_hbTimeLabel->setText("timeout");
    logWarn("Sin heartbeat del dispositivo hace más de 9 segundos.");
}

// ════════════════════════════════════════════════════════════════════════════
//  Estado y controles
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::updateSystemState(const QString& label, const QString& color)
{
    m_stateLabel->setText(label);
    m_stateLabel->setStyleSheet(
        QString("background:#1a1a1a; color:%1; font-weight:bold; font-size:13px;"
                " border-radius:4px; padding:4px 8px; border: 1px solid %1;")
        .arg(color));
}

void MainWindow::updateControls()
{
    bool conn = m_connected;
    bool off  = (m_state == OFF);
    bool run  = (m_state == NORMAL || m_state == BLIND);

    m_startNormalBtn->setEnabled(conn && off);
    m_startBlindBtn->setEnabled(conn && off);
    m_stopBtn->setEnabled(conn && run);
    m_configWidget->setEnabled(conn && off);
    m_calibBtn->setEnabled(conn && off);

    if (!conn)
        updateSystemState("APAGADO", "#555555");
}

// ════════════════════════════════════════════════════════════════════════════
//  Logging
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::logTx(const QByteArray& raw, const QString& desc)
{
    QString ts = QTime::currentTime().toString("hh:mm:ss.zzz");
    appendLog(QString("<font color='#5599ff'>[%1] <b>TX</b>  %2  →  <i>%3</i></font>")
              .arg(ts, Protocol::toHex(raw), desc));
}

void MainWindow::logRx(const QByteArray& raw, const QString& desc)
{
    QString ts = QTime::currentTime().toString("hh:mm:ss.zzz");
    appendLog(QString("<font color='#44dd88'>[%1] <b>RX</b>  %2  →  <i>%3</i></font>")
              .arg(ts, Protocol::toHex(raw), desc));
}

void MainWindow::logInfo(const QString& msg)
{
    QString ts = QTime::currentTime().toString("hh:mm:ss.zzz");
    appendLog(QString("<font color='#ffdd44'>[%1] <b>INFO</b>  %2</font>")
              .arg(ts, msg.toHtmlEscaped()));
}

void MainWindow::logWarn(const QString& msg)
{
    QString ts = QTime::currentTime().toString("hh:mm:ss.zzz");
    appendLog(QString("<font color='#ff6644'>[%1] <b>WARN</b>  %2</font>")
              .arg(ts, msg.toHtmlEscaped()));
}

void MainWindow::appendLog(const QString& html)
{
    m_logEdit->append(html);
    auto* sb = m_logEdit->verticalScrollBar();
    sb->setValue(sb->maximum());
}

#include "cintatracker.h"
#include "protocol.h"

#include <QPainter>
#include <QLinearGradient>
#include <QFont>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / reset
// ─────────────────────────────────────────────────────────────────────────────

CintaTracker::CintaTracker(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(560, 265);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(200);
    connect(m_flashTimer, &QTimer::timeout, this, [this]{
        m_flashState = !m_flashState;
        update();
    });

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(40);   // ~25 fps
    connect(m_animTimer, &QTimer::timeout, this, &CintaTracker::onAnimTick);
}

void CintaTracker::reset()
{
    m_cajas.clear();
    for (int& c : m_ejected) c = 0;
    m_discarded  = 0;
    m_flashState = false;
    m_flashTimer->stop();
    m_animTimer->stop();
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Gestor de estado (parser de zonas)
// ─────────────────────────────────────────────────────────────────────────────

void CintaTracker::handleTracker(uint8_t id, uint8_t tipo, uint8_t zona)
{
    switch (zona) {

    // ── ZONA 1: nace la caja entre el sensor entrada y ARM1 ──────────────────
    case Protocol::ZONA_1_SENSOR_A_ARM1: {
        BoxState b;
        b.id = id; b.tipo = tipo; b.zona = 1;
        m_cajas[id] = b;
        emit statusMessage(
            QString("Caja #%1 (%2) detectada — ZONA 1 (Sensor→ARM1)")
            .arg(id).arg(Protocol::boxName(tipo)));
        break;
    }

    // ── ZONA 2: pasó ARM1 sin eyección ───────────────────────────────────────
    case Protocol::ZONA_2_ARM1_A_ARM2: {
        auto it = m_cajas.find(id);
        if (it != m_cajas.end()) {
            it->tipo = tipo;
            it->zona = 2;
        } else {
            BoxState b; b.id = id; b.tipo = tipo; b.zona = 2;
            m_cajas[id] = b;
        }
        emit statusMessage(
            QString("Caja #%1 (%2) en ZONA 2 (pasó ARM1 sin eyección)")
            .arg(id).arg(Protocol::boxName(tipo)));
        break;
    }

    // ── ZONA 3: pasó ARM2 sin eyección ───────────────────────────────────────
    case Protocol::ZONA_3_ARM2_A_ARM3: {
        auto it = m_cajas.find(id);
        if (it != m_cajas.end()) {
            it->tipo = tipo;
            it->zona = 3;
        } else {
            BoxState b; b.id = id; b.tipo = tipo; b.zona = 3;
            m_cajas[id] = b;
        }
        emit statusMessage(
            QString("Caja #%1 (%2) en ZONA 3 (pasó ARM2 sin eyección)")
            .arg(id).arg(Protocol::boxName(tipo)));
        break;
    }

    // ── ZONA 4: descarte final — cayó al final de la cinta ───────────────────
    case Protocol::ZONA_4_DESCARTE: {
        auto it = m_cajas.find(id);
        if (it != m_cajas.end()) {
            it->tipo = tipo;
            it->zona = 4;
            it->exit = BoxState::Exit::Discarding;
            it->animStep = 0;
        } else {
            BoxState b; b.id = id; b.tipo = tipo; b.zona = 4;
            b.exit = BoxState::Exit::Discarding;
            m_cajas[id] = b;
        }
        m_discarded++;
        if (!m_animTimer->isActive()) m_animTimer->start();
        emit statusMessage(
            QString("Caja #%1 descartada al final (no reconocida). Descarte total: %2")
            .arg(id).arg(m_discarded));
        break;
    }

    // ── ZONA 5: eyectada exitosamente por un brazo ───────────────────────────
    case Protocol::ZONA_5_EYECTADA: {
        auto it = m_cajas.find(id);
        if (it != m_cajas.end()) {
            it->tipo = tipo;
            it->zona = 5;
            it->exit = BoxState::Exit::Ejecting;
            it->animStep = 0;
        } else {
            BoxState b; b.id = id; b.tipo = tipo; b.zona = 5;
            b.exit = BoxState::Exit::Ejecting;
            m_cajas[id] = b;
        }
        int t = (tipo >= 1 && tipo <= 3) ? tipo : 0;
        if (t > 0) m_ejected[t]++;
        if (!m_flashTimer->isActive()) m_flashTimer->start();
        if (!m_animTimer->isActive()) m_animTimer->start();
        emit statusMessage(
            QString("Caja #%1 (%2) eyectada por ARM%3. "
                    "Chicas:%4 Medianas:%5 Grandes:%6")
            .arg(id).arg(Protocol::boxName(tipo)).arg(armFor(tipo) + 1)
            .arg(m_ejected[1]).arg(m_ejected[2]).arg(m_ejected[3]));
        break;
    }

    default: break;
    }

    update();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tick de animación (40ms)
// ─────────────────────────────────────────────────────────────────────────────

void CintaTracker::onAnimTick()
{
    bool anyAnim    = false;
    bool anyFlash   = false;
    QList<uint8_t> done;

    for (auto& b : m_cajas) {
        if (b.exit == BoxState::Exit::None) continue;
        b.animStep++;
        if (b.animStep >= ANIM_STEPS)
            done.append(b.id);
        else {
            anyAnim = true;
            if (b.exit == BoxState::Exit::Ejecting) anyFlash = true;
        }
    }

    for (uint8_t id : done) m_cajas.remove(id);

    if (!anyAnim)  m_animTimer->stop();
    if (!anyFlash) { m_flashTimer->stop(); m_flashState = false; }

    update();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers de posición/apariencia
// ─────────────────────────────────────────────────────────────────────────────

QColor CintaTracker::boxColor(uint8_t tipo) const
{
    switch (tipo) {
    case 1: return QColor(255, 210,  30);   // Chica   – amarillo
    case 2: return QColor(255, 130,  20);   // Mediana – naranja
    case 3: return QColor(220,  50,  50);   // Grande  – rojo
    default: return QColor(140, 140, 140);  // Desconocido – gris
    }
}

QSize CintaTracker::boxDim(uint8_t tipo) const
{
    switch (tipo) {
    case 1: return {30, 22};
    case 2: return {42, 30};
    case 3: return {56, 40};
    default: return {28, 20};
    }
}

// X central de la caja según zona y estado de animación
int CintaTracker::boxXCenter(const BoxState& b) const
{
    if (b.exit == BoxState::Exit::Ejecting) {
        // La caja está fija en su brazo; la animación la eleva
        return xArm(armFor(b.tipo));
    }
    if (b.exit == BoxState::Exit::Discarding) {
        // Cae deslizándose hacia la derecha
        int start   = zoneCenterX(4);
        int travel  = (width() - start + 40) * b.animStep / ANIM_STEPS;
        return start + travel;
    }
    switch (b.zona) {
    case 1: return zoneCenterX(1);
    case 2: return zoneCenterX(2);
    case 3: return zoneCenterX(3);
    case 4: return zoneCenterX(4);
    default: return zoneCenterX(1);
    }
}

// Y central de la caja según estado de animación
// En reposo la caja descansa con la base sobre beltTop().
int CintaTracker::boxYCenter(const BoxState& b) const
{
    int h       = boxDim(b.tipo).height();
    int restY   = beltTop() - h / 2;

    if (b.exit == BoxState::Exit::Ejecting) {
        // Sube hacia la parte superior del widget
        int travel = (beltTop() - 8) * b.animStep / ANIM_STEPS;
        return restY - travel;
    }
    if (b.exit == BoxState::Exit::Discarding) {
        // Baja por debajo de la cinta
        int travel = (height() - beltTop()) * b.animStep / ANIM_STEPS;
        return restY + travel;
    }
    return restY;
}

// Opacidad: 255 en reposo, se desvanece hacia 0 al final de la animación
int CintaTracker::boxAlpha(const BoxState& b) const
{
    if (b.exit == BoxState::Exit::None) return 255;
    return 255 - 255 * b.animStep / ANIM_STEPS;
}

bool CintaTracker::armIsActive(int armIdx) const
{
    for (const auto& b : m_cajas)
        if (b.exit == BoxState::Exit::Ejecting && armFor(b.tipo) == armIdx)
            return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────────────────────────

void CintaTracker::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawBackground(p);
    drawBelt(p);
    drawArms(p);
    drawZoneLabels(p);
    drawBoxes(p);
    drawStatistics(p);
}

void CintaTracker::drawBackground(QPainter& p)
{
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(18, 20, 28));
    bg.setColorAt(1.0, QColor(12, 14, 18));
    p.fillRect(rect(), bg);

    // Grilla sutil
    p.setPen(QPen(QColor(28, 32, 44), 1));
    for (int x = 0; x < width();  x += 24) p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 24) p.drawLine(0, y, width(), y);

    // Bandas de color por zona (incluyen toda la franja visual)
    const QColor fill[5] = {
        {},
        QColor( 45, 160,  65, 18),  // Z1 – verde
        QColor( 40, 115, 200, 18),  // Z2 – azul
        QColor(190, 155,  25, 18),  // Z3 – amarillo
        QColor(170,  38,  38, 18),  // Z4 – rojo (descarte)
    };
    const QColor border[5] = {
        {},
        QColor( 45, 160,  65, 50),
        QColor( 40, 115, 200, 50),
        QColor(190, 155,  25, 50),
        QColor(170,  38,  38, 50),
    };

    const int bandTop    = beltTop() - 70;
    const int bandBottom = beltBottom() + 55;

    for (int z = 1; z <= 4; z++) {
        p.fillRect(QRect(zoneStart(z), bandTop,
                         zoneEnd(z) - zoneStart(z), bandBottom - bandTop), fill[z]);
        if (z > 1) {
            p.setPen(QPen(border[z], 1, Qt::DashLine));
            p.drawLine(zoneStart(z), bandTop, zoneStart(z), bandBottom);
        }
    }
}

void CintaTracker::drawBelt(QPainter& p)
{
    const int bTop = beltTop();
    const int bH   = beltHeight();
    const int bBot = bTop + bH;
    const int bW   = width();

    // Superficie
    QLinearGradient belt(0, bTop, 0, bBot);
    belt.setColorAt(0.0, QColor(72, 72, 72));
    belt.setColorAt(0.2, QColor(52, 52, 52));
    belt.setColorAt(1.0, QColor(35, 35, 35));
    p.fillRect(QRect(0, bTop, bW, bH), belt);

    // Eslabones
    p.setPen(QPen(QColor(63, 63, 63), 1));
    for (int x = 0; x < bW; x += 26)
        p.drawLine(x, bTop, x, bBot);

    // Flechas de dirección
    p.setPen(QPen(QColor(85, 170, 85, 65), 1));
    p.setFont(QFont("Arial", 9));
    p.drawText(QRect(20, bTop + bH/2 - 8, bW - 40, 16),
               Qt::AlignCenter, "→  →  →  →  →  →  →  →  →  →  →  →");

    // Bordes
    p.setPen(QPen(QColor(100, 100, 100), 2));
    p.drawLine(0, bTop, bW, bTop);
    p.setPen(QPen(QColor(18, 18, 18), 2));
    p.drawLine(0, bBot, bW, bBot);

    // Rodillos
    auto drawRoller = [&](int cx) {
        QRect r(cx - 9, bTop - 5, 18, bH + 10);
        QLinearGradient rg(r.left(), 0, r.right(), 0);
        rg.setColorAt(0.0, QColor(48, 48, 48));
        rg.setColorAt(0.5, QColor(150, 150, 150));
        rg.setColorAt(1.0, QColor(38, 38, 38));
        p.setBrush(rg);
        p.setPen(QPen(QColor(22, 22, 22), 1));
        p.drawRoundedRect(r, 4, 4);
    };
    drawRoller(5);
    drawRoller(bW - 5);
}

// Dibuja los 3 brazos eyectores en los límites entre zonas (25%, 50%, 75%).
void CintaTracker::drawArms(QPainter& p)
{
    const int bTop   = beltTop();
    const int bBot   = beltBottom();
    const int armW   = 14;
    const int armH   = 50;
    const int armTop = bTop - armH - 8;

    const QColor baseCol[3] = {
        QColor(255, 210,  30),   // ARM1 – Chica   (amarillo)
        QColor(255, 130,  20),   // ARM2 – Mediana (naranja)
        QColor(220,  50,  50),   // ARM3 – Grande  (rojo)
    };
    const QString lbl[3] = { "ARM1", "ARM2", "ARM3" };

    for (int i = 0; i < 3; i++) {
        const int  cx     = xArm(i);
        const bool active = armIsActive(i);

        QColor c = active ? baseCol[i] : QColor(50, 53, 60);
        if (active && m_flashState) c = c.lighter(180);

        // Línea de límite de zona
        p.setPen(QPen(active ? c.darker(110) : QColor(55, 58, 68), 1, Qt::DashLine));
        p.drawLine(cx, armTop - 2, cx, bBot + 44);

        // Cuerpo del brazo
        QRect arm(cx - armW / 2, armTop, armW, armH);
        QLinearGradient ag(arm.left(), 0, arm.right(), 0);
        ag.setColorAt(0.0, c.darker(160));
        ag.setColorAt(0.5, c);
        ag.setColorAt(1.0, c.darker(160));
        p.setBrush(ag);
        p.setPen(QPen(c.darker(120), 1));
        p.drawRoundedRect(arm, 3, 3);

        p.setPen(active ? Qt::black : QColor(88, 88, 88));
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.drawText(arm, Qt::AlignCenter, lbl[i]);

        // Flecha de impacto (hacia arriba = hacia la rampa)
        if (active) {
            p.setPen(QPen(c.lighter(140), 2));
            int tip = bTop - 2;
            int ay  = arm.bottom() + 2;
            p.drawLine(cx, ay, cx, tip);
            p.drawLine(cx - 4, tip - 6, cx, tip);
            p.drawLine(cx + 4, tip - 6, cx, tip);
        }
    }
}

// Etiquetas de zona debajo de la cinta + nombres de brazos.
void CintaTracker::drawZoneLabels(QPainter& p)
{
    const int row0  = beltBottom() + 6;
    const int row1  = beltBottom() + 18;
    const int rowAm = beltBottom() + 32;   // etiqueta del brazo en el límite

    struct ZInfo { int z; QString name; QString sub; QColor col; } zi[4] = {
        { 1, "ZONA 1", "Sensor → ARM1", QColor( 55, 185,  75) },
        { 2, "ZONA 2", "ARM1 → ARM2",   QColor( 55, 135, 215) },
        { 3, "ZONA 3", "ARM2 → ARM3",   QColor(210, 175,  35) },
        { 4, "ZONA 4", "DESCARTE",      QColor(210,  60,  60) },
    };

    for (auto& z : zi) {
        int cx = zoneCenterX(z.z);
        p.setFont(QFont("Courier New", 8, QFont::Bold));
        p.setPen(z.col);
        p.drawText(QRect(cx - 48, row0, 96, 13), Qt::AlignCenter, z.name);
        p.setFont(QFont("Courier New", 7));
        p.setPen(z.col.darker(135));
        p.drawText(QRect(cx - 48, row1, 96, 12), Qt::AlignCenter, z.sub);
    }

    // Etiquetas de brazos en los límites
    const QColor armCol[3] = {
        QColor(255, 210,  30),
        QColor(255, 130,  20),
        QColor(220,  50,  50),
    };
    const QString armLbl[3] = { "ARM1\nCHICA", "ARM2\nMED", "ARM3\nGDE" };
    for (int i = 0; i < 3; i++) {
        p.setFont(QFont("Courier New", 7, QFont::Bold));
        p.setPen(armCol[i]);
        p.drawText(QRect(xArm(i) - 22, rowAm, 44, 22), Qt::AlignCenter, armLbl[i]);
    }
}

void CintaTracker::drawBoxes(QPainter& p)
{
    if (m_cajas.isEmpty()) return;

    // ── Paso 1: calcular offset lateral por zona (orden ascendente de ID) ────
    QMap<uint8_t, int> zoneCount;
    for (const auto& b : m_cajas)
        if (b.exit == BoxState::Exit::None) zoneCount[b.zona]++;

    QMap<uint8_t, int> zoneIdx;

    struct BoxEntry { BoxState st; int cx, cy; };
    QVector<BoxEntry> entries;
    entries.reserve(m_cajas.size());

    for (auto it = m_cajas.cbegin(); it != m_cajas.cend(); ++it) {
        const BoxState& b = it.value();
        QSize sz = boxDim(b.tipo);
        int cx = boxXCenter(b);
        int cy = boxYCenter(b);

        if (b.exit == BoxState::Exit::None) {
            int total = zoneCount[b.zona];
            int idx   = zoneIdx[b.zona]++;
            if (total > 1) {
                // Negado: idx=0 (más vieja) → derecha (adelante en cinta),
                //         idx=N (más nueva) → izquierda (recién ingresó)
                double off = -((idx - (total - 1) * 0.5) * (sz.width() + 6.0));
                cx += static_cast<int>(off);
            }
        }
        entries.append({b, cx, cy});
    }

    // ── Paso 2: dibujar en orden inverso ─────────────────────────────────────
    // QMap itera por ID ascendente → índice 0 = caja más vieja.
    // Dibujando de atrás hacia adelante, las más nuevas (mayor ID) quedan debajo
    // y las más viejas encima.
    for (int i = entries.size() - 1; i >= 0; --i) {
        const BoxState& b = entries[i].st;
        const int cx = entries[i].cx;
        const int cy = entries[i].cy;

        QSize sz  = boxDim(b.tipo);
        QColor c  = boxColor(b.tipo);
        int alpha = boxAlpha(b);

        const int bx = cx - sz.width()  / 2;
        const int by = cy - sz.height() / 2;
        QRect box(bx, by, sz.width(), sz.height());

        // Parpadeo al eyectar (primeros frames)
        bool flash = (b.exit == BoxState::Exit::Ejecting)
                     && m_flashState && b.animStep < 6;
        if (flash) c = c.lighter(170);

        c.setAlpha(alpha);

        QLinearGradient bg(bx, by, bx, by + sz.height());
        QColor top = c.lighter(130); top.setAlpha(alpha);
        QColor bot = c.darker(130);  bot.setAlpha(alpha);
        bg.setColorAt(0.0, top);
        bg.setColorAt(1.0, bot);
        p.setBrush(bg);

        QColor borderC = flash ? Qt::white : c.darker(150);
        borderC.setAlpha(alpha);
        p.setPen(QPen(borderC, flash ? 2 : 1));
        p.drawRoundedRect(box, 3, 3);

        // Marca X roja para CAJA_DESCONOCIDA (tipo 0 legacy o tipo 4 nuevo)
        const bool isUnknown = (b.tipo == Protocol::BOX_NONE ||
                                b.tipo == Protocol::BOX_UNKNOWN);
        if (isUnknown) {
            QColor xc(220, 50, 50, alpha);
            p.setPen(QPen(xc, 2));
            p.drawLine(box.topLeft()    + QPoint( 3,  3),
                       box.bottomRight() - QPoint( 3,  3));
            p.drawLine(box.topRight()   + QPoint(-3,  3),
                       box.bottomLeft() + QPoint( 3, -3));
        }

        // Etiqueta ID
        QColor lblC = isUnknown ? QColor(220, 80, 80) : Qt::black;
        lblC.setAlpha(alpha);
        p.setPen(lblC);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(box, Qt::AlignCenter,
                   isUnknown ? QString("?%1").arg(b.id) : QString("#%1").arg(b.id));

        // Halo de impacto al inicio de la eyección
        if (flash) {
            QColor glow = boxColor(b.tipo);
            glow.setAlpha(60);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(glow, 5));
            p.drawRoundedRect(box.adjusted(-4, -4, 4, 4), 5, 5);
        }
    }
}

// Panel de estadísticas en la esquina superior izquierda.
void CintaTracker::drawStatistics(QPainter& p)
{
    // Contadores eyectadas por tipo
    struct { uint8_t tipo; QString lbl; } items[3] = {
        { 1, "Chicas"   },
        { 2, "Medianas" },
        { 3, "Grandes"  },
    };

    const int x0 = 10;
    int       y  = 8;

    p.setFont(QFont("Courier New", 8, QFont::Bold));
    p.setPen(QColor(160, 160, 160));
    p.drawText(QRect(x0, y, 120, 13), Qt::AlignLeft | Qt::AlignVCenter, "EYECTADAS:");
    y += 14;

    for (auto& it : items) {
        QColor col = boxColor(it.tipo);
        int    cnt = m_ejected[it.tipo];

        // Cuadrito de color
        p.fillRect(QRect(x0, y + 1, 9, 9), col);
        p.setPen(col.darker(140));
        p.drawRect(QRect(x0, y + 1, 9, 9));

        // Texto
        p.setPen(col.lighter(130));
        p.setFont(QFont("Courier New", 8, QFont::Bold));
        p.drawText(QRect(x0 + 13, y, 90, 12), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1: %2").arg(it.lbl).arg(cnt));
        y += 13;
    }

    // Descarte
    y += 3;
    p.fillRect(QRect(x0, y + 1, 9, 9), QColor(120, 120, 120));
    p.setPen(QColor(80, 80, 80));
    p.drawRect(QRect(x0, y + 1, 9, 9));
    p.setPen(QColor(160, 80, 80));
    p.setFont(QFont("Courier New", 8, QFont::Bold));
    p.drawText(QRect(x0 + 13, y, 100, 12), Qt::AlignLeft | Qt::AlignVCenter,
               QString("Descarte:  %1").arg(m_discarded));

    // En cinta (en la esquina, top-right)
    p.setFont(QFont("Courier New", 8));
    p.setPen(QColor(110, 160, 230));
    QString inBelt = QString("En cinta: %1").arg(m_cajas.size());
    p.drawText(QRect(width() - 115, 8, 108, 13),
               Qt::AlignRight | Qt::AlignVCenter, inBelt);
}

#pragma once
#include <QWidget>
#include <QMap>
#include <QTimer>
#include <cstdint>

struct BoxState {
    uint8_t id   = 0;
    uint8_t tipo = 0;   // 0=desconocido, 1=Chica, 2=Mediana, 3=Grande
    uint8_t zona = 0;   // 1..3 = zonas de tránsito, 4 = descarte, 5 = eyectada

    enum class Exit { None, Ejecting, Discarding };
    Exit exit     = Exit::None;
    int  animStep = 0;
};

class CintaTracker : public QWidget
{
    Q_OBJECT
    static constexpr int ANIM_STEPS = 20;

public:
    explicit CintaTracker(QWidget* parent = nullptr);

    void handleTracker(uint8_t id, uint8_t tipo, uint8_t zona);
    void reset();

signals:
    void statusMessage(const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint()        const override { return {780, 310}; }
    QSize minimumSizeHint() const override { return {560, 265}; }

private:
    QMap<uint8_t, BoxState> m_cajas;
    int    m_ejected[4] = {};  // [tipo 1..3] = conteo de eyecciones exitosas
    int    m_discarded  = 0;   // cajas que cayeron al final

    bool   m_flashState = false;
    QTimer* m_flashTimer;  // 200ms – parpadeo al eyectar
    QTimer* m_animTimer;   // 40ms  – animaciones de salida

    // ── Geometría de la cinta ────────────────────────────────────────────────
    // 4 zonas iguales (25% cada una). 3 brazos en los límites entre zonas.
    //
    //  [  ZONA 1  ] ARM1 [  ZONA 2  ] ARM2 [  ZONA 3  ] ARM3 [  ZONA 4  ]
    //   0%     25%  25%   25%    50%  50%   50%    75%  75%   75%   100%
    //
    int beltTop()    const { return height() * 40 / 100; }
    int beltHeight() const { return height() * 26 / 100; }
    int beltBottom() const { return beltTop() + beltHeight(); }

    int zoneStart(int z)   const { return width() * (z - 1) * 25 / 100; }
    int zoneEnd(int z)     const { return width() *  z      * 25 / 100; }
    int zoneCenterX(int z) const { return (zoneStart(z) + zoneEnd(z)) / 2; }

    // 3 brazos: ARM1@25%, ARM2@50%, ARM3@75%
    int xArm(int i) const {
        const int pct[3] = {25, 50, 75};
        return width() * pct[i] / 100;
    }
    // Índice del brazo (0-based) que eyecta cada tipo
    int armFor(uint8_t tipo) const { return (tipo >= 1 && tipo <= 3) ? tipo - 1 : 2; }

    // ── Visuals de caja ──────────────────────────────────────────────────────
    QColor boxColor(uint8_t tipo) const;
    QSize  boxDim(uint8_t tipo)   const;
    int    boxXCenter(const BoxState& b) const;
    int    boxYCenter(const BoxState& b) const;
    int    boxAlpha(const BoxState& b)   const;

    // ── Draw ─────────────────────────────────────────────────────────────────
    void drawBackground(QPainter& p);
    void drawBelt(QPainter& p);
    void drawArms(QPainter& p);
    void drawZoneLabels(QPainter& p);
    void drawBoxes(QPainter& p);
    void drawStatistics(QPainter& p);

    bool armIsActive(int armIdx) const;
    void onAnimTick();
};

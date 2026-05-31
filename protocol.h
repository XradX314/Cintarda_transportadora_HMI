#pragma once
#include <QByteArray>
#include <QString>
#include <cstdint>

namespace Protocol {

// ── Comandos TX (HMI → Dispositivo) ─────────────────────────────────────────
// ── Respuestas/telemetría RX (Dispositivo → HMI) ────────────────────────────
enum Cmd : uint8_t {
    CMD_START_SISTEMA        = 0x01,   // TX: sin payload
    CMD_STOP_SISTEMA         = 0x02,   // TX: sin payload
    CMD_START_CIEGO          = 0x03,   // TX: sin payload
    CMD_ACK                  = 0x06,   // RX: [cmd_ack 1B]
    CMD_SET_DELAYS           = 0x10,   // TX: [idx 1B][tipo 1B][delay_ms 4B LE]
    CMD_SET_UMBRALES         = 0x11,   // TX: [chica 1B][mediana 1B][grande 1B][tol 1B]
    CMD_SET_PISO             = 0x12,   // TX: [piso_mm 1B]
    CMD_MEDIR_CALIBRACION    = 0x13,   // TX: sin payload | RX: [dist_mm 1B]
    CMD_SET_GEOMETRIA_CIEGA  = 0x14,   // TX: [largo 1B][d0 1B][d1 1B][d2 1B][offset 1B]
    CMD_TRACKER              = 0x5D,   // RX: [id 1B][tipo 1B][zona 1B]
    CMD_TELEMETRY_DIST       = 0x5F,   // RX: [dist_mm 1B]
    CMD_TELEMETRY_IR         = 0x5E,   // RX: [sensor_idx 1B][state 1B]
    CMD_HEARTBEAT            = 0xF0,   // RX: sin payload
};

enum TrackerZone : uint8_t {
    ZONA_1_SENSOR_A_ARM1 = 0x01,  // Entre sensor entrada y Brazo 1
    ZONA_2_ARM1_A_ARM2   = 0x02,  // Entre Brazo 1 y Brazo 2 (pasó ARM1)
    ZONA_3_ARM2_A_ARM3   = 0x03,  // Entre Brazo 2 y Brazo 3 (pasó ARM2)
    ZONA_4_DESCARTE      = 0x04,  // Cayó al final (no reconocida)
    ZONA_5_EYECTADA      = 0x05,  // Eyectada exitosamente por un brazo
};

enum BoxType : uint8_t {
    BOX_NONE    = 0,
    BOX_SMALL   = 1,   // Caja chica
    BOX_MEDIUM  = 2,   // Caja mediana
    BOX_LARGE   = 3,   // Caja grande
    BOX_UNKNOWN = 4,   // Caja desconocida – detectada pero fuera de rango
};

struct Frame {
    uint8_t    cmd     = 0;
    QByteArray payload;
};

// Codifica un frame UNER completo con checksum XOR
QByteArray encode(uint8_t cmd, const QByteArray& payload = {});

// Intenta extraer un frame del buffer; devuelve true si encontró uno válido.
// rawOut (opcional) recibe los bytes exactos del frame consumido.
bool tryDecode(QByteArray& buffer, Frame& out, QByteArray* rawOut = nullptr);

QString cmdName(uint8_t cmd);
QString boxName(uint8_t type);
QString zoneName(uint8_t zone);
QString toHex(const QByteArray& data);

} // namespace Protocol

#include "protocol.h"

// Estructura de frame (según protocolo.c del dispositivo):
//   UNER(4) | len(1) | ':'(1) | cmd(1) | params(len-2) | checksum(1)
//
// IMPORTANTE: len = 1(cmd) + n(params) + 1(checksum)  →  len = params + 2
// El campo len INCLUYE el byte de checksum (así lo define Encode() en el µC).
//
// frameSize total = 4 + 1 + 1 + (len-1) + 1 = 6 + len
//   UNER(4) + lenByte(1) + ':'(1) + cmd(1) + params(len-2) + chk(1) = 6+len
//
// checksum = XOR de todos los bytes del frame excepto el propio checksum;
//            la verificación pasa cuando XOR de todos los bytes (incluído chk) = 0.

QByteArray Protocol::encode(uint8_t cmd, const QByteArray& payload)
{
    QByteArray f;
    f.reserve(8 + payload.size());
    f.append("UNER");
    f.append(char(payload.size() + 2));  // len = params + cmd(1) + checksum(1)
    f.append(':');
    f.append(char(cmd));
    f.append(payload);

    uint8_t xorAcc = 0;
    for (int i = 0; i < f.size(); i++)
        xorAcc ^= (uint8_t)(unsigned char)f.at(i);
    f.append(char(xorAcc));
    return f;
}

bool Protocol::tryDecode(QByteArray& buffer, Frame& out, QByteArray* rawOut)
{
    // Buscar cabecera UNER
    while (buffer.size() >= 4) {
        if ((uint8_t)buffer[0] == 'U' && (uint8_t)buffer[1] == 'N' &&
            (uint8_t)buffer[2] == 'E' && (uint8_t)buffer[3] == 'R')
            break;
        buffer.remove(0, 1);
    }

    if (buffer.size() < 8) return false;  // mínimo: UNER+len+:+cmd+chk = 8B

    uint8_t len = (uint8_t)buffer[4];
    if (len < 2) { buffer.remove(0, 1); return false; }  // mínimo len = 2 (cmd+chk)

    int frameSize = 6 + len;  // 4(UNER) + 1(len) + 1(':') + (len-1)(cmd+params) + 1(chk)
    if (buffer.size() < frameSize) return false;

    if ((uint8_t)buffer[5] != ':') { buffer.remove(0, 1); return false; }

    // Verificar checksum: XOR de todos los bytes del frame debe ser 0x00
    uint8_t xorAcc = 0;
    for (int i = 0; i < frameSize; i++)
        xorAcc ^= (uint8_t)buffer[i];

    if (xorAcc != 0) { buffer.remove(0, 1); return false; }

    if (rawOut) *rawOut = buffer.left(frameSize);

    out.cmd     = (uint8_t)buffer[6];
    out.payload = buffer.mid(7, len - 2);  // params solamente (sin cmd ni checksum)
    buffer.remove(0, frameSize);
    return true;
}

QString Protocol::cmdName(uint8_t cmd)
{
    switch (cmd) {
    case CMD_START_SISTEMA:       return "START_SISTEMA";
    case CMD_STOP_SISTEMA:        return "STOP_SISTEMA";
    case CMD_START_CIEGO:         return "START_CIEGO";
    case CMD_ACK:                 return "ACK";
    case CMD_SET_DELAYS:          return "SET_DELAYS";
    case CMD_SET_UMBRALES:        return "SET_UMBRALES";
    case CMD_SET_PISO:            return "SET_PISO";
    case CMD_MEDIR_CALIBRACION:   return "MEDIR_CALIBRACION";
    case CMD_SET_GEOMETRIA_CIEGA: return "SET_GEOMETRIA_CIEGA";
    case CMD_TRACKER:             return "TRACKER";
    case CMD_TELEMETRY_DIST:      return "TELEMETRY_DIST";
    case CMD_TELEMETRY_IR:        return "TELEMETRY_IR";
    case CMD_HEARTBEAT:           return "HEARTBEAT";
    default: return QString("CMD_0x%1").arg(cmd, 2, 16, QChar('0')).toUpper();
    }
}

QString Protocol::boxName(uint8_t t)
{
    switch (t) {
    case BOX_NONE:    return "NONE";
    case BOX_SMALL:   return "CHICA";
    case BOX_MEDIUM:  return "MEDIANA";
    case BOX_LARGE:   return "GRANDE";
    case BOX_UNKNOWN: return "DESCONOCIDA";
    default: return "?";
    }
}

QString Protocol::zoneName(uint8_t z)
{
    switch (z) {
    case ZONA_1_SENSOR_A_ARM1: return "Z1 (Sensor→ARM1)";
    case ZONA_2_ARM1_A_ARM2:   return "Z2 (ARM1→ARM2)";
    case ZONA_3_ARM2_A_ARM3:   return "Z3 (ARM2→ARM3)";
    case ZONA_4_DESCARTE:      return "Z4 DESCARTE FINAL";
    case ZONA_5_EYECTADA:      return "Z5 EYECTADA";
    default: return QString("ZONA_0x%1").arg(z, 2, 16, QChar('0')).toUpper();
    }
}

QString Protocol::toHex(const QByteArray& data)
{
    QString s;
    for (auto b : data)
        s += QString("%1 ").arg((uint8_t)b, 2, 16, QChar('0')).toUpper();
    return s.trimmed();
}

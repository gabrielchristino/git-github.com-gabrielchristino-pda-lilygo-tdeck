#include "services.h"
#include "pda/datetime.h"
#include "pda/expression.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <esp_partition.h>
#include <driver/i2s.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <algorithm>

namespace pda { namespace hal {
static bool mounted = false;
static std::string mountMessage = "Memoria nao montada";
static Preferences configuration;
static uint32_t lastActivity = 0;
static uint8_t lightLevel = 16;
static bool screenOff = false;

static void brightness(uint8_t value) {
    if (value == lightLevel) return;
    if (!value) { digitalWrite(42, LOW); delay(3); lightLevel = 0; return; }
    if (!lightLevel) { digitalWrite(42, HIGH); delayMicroseconds(30); lightLevel = 16; }
    const uint8_t pulses = uint8_t((lightLevel - value + 16) % 16);
    for (uint8_t i = 0; i < pulses; ++i) { digitalWrite(42, LOW); delayMicroseconds(1); digitalWrite(42, HIGH); delayMicroseconds(1); }
    lightLevel = value;
}
static const char *slotPath(int slot) { return slot == 0 ? "/littlefs/pda-a.bin" : "/littlefs/pda-b.bin"; }
class FlashStorage : public StorageBackend {
public:
    bool ready() const override { return mounted; }
    bool exists(int slot) const override {
        FILE *file = fopen(slotPath(slot), "rb"); if (!file) return false; fclose(file); return true;
    }
    bool read(int slot, std::vector<uint8_t> &data) override {
        FILE *file = fopen(slotPath(slot), "rb"); if (!file) return false;
        if (fseek(file, 0, SEEK_END)) { fclose(file); return false; }
        const long size = ftell(file);
        if (size < 0 || size_t(size) > limits::snapshotBytes) { fclose(file); return false; }
        rewind(file); data.resize(size_t(size));
        const bool ok = fread(data.data(), 1, data.size(), file) == data.size(); fclose(file); return ok;
    }
    bool write(int slot, const std::vector<uint8_t> &data) override {
        if (!mounted) return false;
        FILE *file = fopen(slotPath(slot), "wb"); if (!file) return false;
        bool ok = fwrite(data.data(), 1, data.size(), file) == data.size();
        if (fflush(file) != 0) ok = false;
        if (fsync(fileno(file)) != 0) ok = false;
        if (fclose(file) != 0) ok = false;
        return ok;
    }
    bool initialize() override {
        if (mounted) LittleFS.end();
        mounted = false;
        // Only a deliberate in-app confirmation invokes this method.
        mounted = LittleFS.begin(false);
        if (!mounted) {
            if (!LittleFS.format()) { mountMessage = "Falha ao formatar area de dados"; return false; }
            mounted = LittleFS.begin(false);
        }
        if (!mounted) { mountMessage = "Falha ao montar area de dados"; return false; }
        LittleFS.remove("/pda-a.bin"); LittleFS.remove("/pda-b.bin");
        mountMessage = "Flash interna / LittleFS";
        return true;
    }
    const char *description() const override { return mountMessage.c_str(); }
};
static FlashStorage flashStorage;

void beginStorage() {
    mounted = LittleFS.begin(false);
    if (mounted) { mountMessage = "Flash interna / LittleFS"; return; }
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    bool erased = partition != nullptr;
    uint8_t buffer[1024];
    for (size_t offset = 0; erased && offset < partition->size; offset += sizeof(buffer)) {
        const size_t n = std::min(sizeof(buffer), size_t(partition->size - offset));
        if (esp_partition_read(partition, offset, buffer, n) != ESP_OK) { erased = false; break; }
        for (size_t i = 0; i < n; ++i) if (buffer[i] != 0xFF) { erased = false; break; }
        yield();
    }
    // Automatic first-use formatting is restricted to a completely blank area.
    if (erased && LittleFS.format()) mounted = LittleFS.begin(false);
    mountMessage = mounted ? "Flash interna / LittleFS" : "Area nao reconhecida. Preparar em Ajustes.";
}
static bool writeExport(const std::string &path, const uint8_t *data, size_t size) {
    File file = SD.open(path.c_str(), FILE_WRITE);
    if (!file) return false;
    const bool ok = file.write(data, size) == size; file.flush(); file.close(); return ok;
}
static bool writeText(const std::string &path, const std::string &text) {
    return writeExport(path, reinterpret_cast<const uint8_t *>(text.data()), text.size());
}
class DevicePlatform : public Platform {
public:
    int64_t utc() const override { return int64_t(time(nullptr)); }
    bool setUtc(int64_t epoch) override { timeval t; t.tv_sec = time_t(epoch); t.tv_usec = 0; return settimeofday(&t, nullptr) == 0; }
    void configure(const Preferences &p) override { configuration = p; brightness(p.brightness); lastActivity = millis(); screenOff = false; }
    void alert() override {
        // Short, bounded tone. No microphone/MCLK routing to GPIO0 (ball click).
        static bool initialized = false;
        if (!initialized) {
            i2s_config_t config = {};
            config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX);
            config.sample_rate = 16000; config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
            config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
            config.intr_alloc_flags = 0; config.dma_buf_count = 4; config.dma_buf_len = 128;
            config.use_apll = false; config.tx_desc_auto_clear = true;
            if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return;
            i2s_pin_config_t pins = {}; pins.bck_io_num = 7; pins.ws_io_num = 5; pins.data_out_num = 6;
            pins.data_in_num = I2S_PIN_NO_CHANGE; pins.mck_io_num = I2S_PIN_NO_CHANGE;
            if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) { i2s_driver_uninstall(I2S_NUM_0); return; }
            initialized = true;
        }
        int16_t samples[640];
        for (size_t i = 0; i < 640; ++i) samples[i] = (i / 8) % 2 ? 1500 : -1500;
        size_t written = 0; i2s_write(I2S_NUM_0, samples, sizeof(samples), &written, pdMS_TO_TICKS(60));
        activity();
    }
    bool exportData(const State &s, std::string &message) override {
        if (!SD.begin(39, SPI, 800000U)) { message = "Cartao SD nao encontrado."; return false; }
        SD.mkdir("/pda-export");
        // Choose a new directory, preserving all earlier exports.
        std::string root;
        for (unsigned i = 1; i < 10000; ++i) {
            root = "/pda-export/copia-" + std::to_string(i);
            if (!SD.exists(root.c_str())) break;
            root.clear();
        }
        if (root.empty() || !SD.mkdir(root.c_str())) { message = "Nao foi possivel criar pasta no SD."; return false; }
        bool ok = true;
        std::vector<uint8_t> snapshot;
        ok = encodeSnapshot(s, 1, snapshot) && writeExport(root + "/estado.pda", snapshot.data(), snapshot.size());
        for (const auto &x : s.notes) ok = writeText(root + "/nota-" + std::to_string(x.id) + ".txt", x.title + "\n\n" + x.text) && ok;
        std::string csv = "Nome;Telefone;Email;Observacoes\r\n";
        for (const auto &x : s.contacts) csv += csvField(x.name) + ";" + csvField(x.phone) + ";" + csvField(x.email) + ";" + csvField(x.detail) + "\r\n";
        ok = writeText(root + "/contatos.csv", csv) && ok;
        csv = "Titulo;Inicio;Fim;Lembrete_min;Notas\r\n";
        for (const auto &x : s.appointments) csv += csvField(x.title) + ";" + csvField(formatCivil(x.start, s.preferences.timezoneMinutes)) + ";" +
            csvField(x.end ? formatCivil(x.end, s.preferences.timezoneMinutes) : "") + ";" + std::to_string(x.reminderMinutes) + ";" + csvField(x.detail) + "\r\n";
        ok = writeText(root + "/agenda.csv", csv) && ok;
        for (const auto &x : s.sheets) {
            csv.clear();
            Evaluator evaluator(&x);
            for (int row = 0; row < limits::rows; ++row) {
                for (int col = 0; col < limits::columns; ++col) {
                    if (col) csv += ';';
                    const auto &raw = x.get(row, col);
                    const Value value = evaluator.cell(row, col);
                    csv += csvField(value.kind == ValueKind::Text ? raw : value.display());
                }
                csv += "\r\n";
            }
            ok = writeText(root + "/planilha-" + std::to_string(x.id) + ".csv", csv) && ok;
        }
        for (const auto &x : s.sketches) {
            const std::string header = "P4\n288 128\n";
            std::vector<uint8_t> pbm(header.begin(), header.end()); pbm.insert(pbm.end(), x.pixels.begin(), x.pixels.end());
            ok = writeExport(root + "/desenho-" + std::to_string(x.id) + ".pbm", pbm.data(), pbm.size()) && ok;
        }
        message = ok ? "Exportado: " + root : "Exportacao incompleta. Confira o SD.";
        return ok;
    }
};
static DevicePlatform devicePlatform;
StorageBackend &storage() { return flashStorage; }
Platform &platform() { return devicePlatform; }
void activity() { lastActivity = millis(); screenOff = false; brightness(configuration.brightness); }
bool wakeScreen() { const bool wasOff = screenOff; activity(); return wasOff; }
void powerTick() {
    const uint32_t elapsed = uint32_t(millis() - lastActivity) / 1000;
    if (configuration.offSeconds && elapsed >= configuration.offSeconds) { brightness(0); screenOff = true; }
    else if (configuration.dimSeconds && elapsed >= configuration.dimSeconds) brightness(std::min<uint8_t>(3, configuration.brightness));
}
} }

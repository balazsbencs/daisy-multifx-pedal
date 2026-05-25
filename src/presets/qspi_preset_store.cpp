#include "qspi_preset_store.h"
#include <cstring>

namespace pedal {

// File-scope 4 KB buffer reused by SaveSlot to avoid stack usage.
static uint8_t s_sector_buf[4096];

struct PresetHeader {
    uint32_t magic;
    uint16_t version;
    uint8_t  active_bank;
    uint8_t  active_slot;
    char     name[PRESET_SLOT_COUNT][12]; // 1200 bytes — fits in one 4 KB sector
};

bool MultiPresetSlot::operator!=(const MultiPresetSlot& o) const {
    return memcmp(this, &o, sizeof(*this)) != 0;
}

void QspiPresetStore::Init(daisy::QSPIHandle& qspi) {
    qspi_ = &qspi;
    ReadHeader();
    initialized_ = true;
}

void QspiPresetStore::ReadHeader() {
    const auto* h = reinterpret_cast<const PresetHeader*>(kQspiBase + kHeaderOffset);
    if (h->magic == kMagic && h->version == kVersion) {
        active_bank_ = h->active_bank;
        active_slot_ = h->active_slot;
        memcpy(names_, h->name, sizeof(names_));
    } else {
        // First boot / format mismatch — start fresh.
        active_bank_ = 0;
        active_slot_ = 0;
        memset(names_, 0, sizeof(names_));
    }
}

void QspiPresetStore::WriteHeader() {
    PresetHeader h{};
    h.magic       = kMagic;
    h.version     = kVersion;
    h.active_bank = static_cast<uint8_t>(active_bank_);
    h.active_slot = static_cast<uint8_t>(active_slot_);
    memcpy(h.name, names_, sizeof(names_));
    qspi_->EraseSector(kQspiBase + kHeaderOffset);
    qspi_->Write(kQspiBase + kHeaderOffset, sizeof(h),
                 reinterpret_cast<uint8_t*>(&h));
}

void QspiPresetStore::SetActive(int bank, int slot) {
    if (!initialized_ || !ValidIndex(bank, slot)) return;
    active_bank_ = bank;
    active_slot_ = slot;
    WriteHeader();
}

bool QspiPresetStore::LoadSlot(int bank, int slot, MultiPresetSlot& out) const {
    if (!initialized_ || !ValidIndex(bank, slot)) return false;
    const int      idx             = LinearIndex(bank, slot);
    const uint32_t sector_addr     = kQspiBase + kDataOffset
                                     + static_cast<uint32_t>(idx / kSlotsPerSector) * kSectorSize;
    const uint32_t offset_in_sector = static_cast<uint32_t>(idx % kSlotsPerSector)
                                      * sizeof(MultiPresetSlot);
    const auto* ptr = reinterpret_cast<const MultiPresetSlot*>(sector_addr + offset_in_sector);
    if (!ptr->valid) return false;
    memcpy(&out, ptr, sizeof(out));
    return true;
}

bool QspiPresetStore::SaveSlot(int bank, int slot, const MultiPresetSlot& data,
                                const char* name) {
    if (!initialized_ || !ValidIndex(bank, slot)) return false;

    const int      idx              = LinearIndex(bank, slot);
    const uint32_t sector_addr      = kQspiBase + kDataOffset
                                      + static_cast<uint32_t>(idx / kSlotsPerSector) * kSectorSize;
    const uint32_t offset_in_sector = static_cast<uint32_t>(idx % kSlotsPerSector)
                                      * sizeof(MultiPresetSlot);

    // Read-modify-write: preserve all other presets in the same 4 KB sector.
    memcpy(s_sector_buf, reinterpret_cast<const void*>(sector_addr), kSectorSize);
    auto* dst = reinterpret_cast<MultiPresetSlot*>(s_sector_buf + offset_in_sector);
    *dst        = data;
    dst->valid  = 1;

    if (name) {
        const int idx = LinearIndex(bank, slot);
        strncpy(names_[idx], name, 11);
        names_[idx][11] = '\0';
    }

    qspi_->EraseSector(sector_addr);
    qspi_->Write(sector_addr, kSectorSize, s_sector_buf);
    WriteHeader(); // persists updated names and active bank/slot
    return true;
}

bool QspiPresetStore::LoadLiveState(MultiPresetSlot& out) const {
    if (!initialized_) return false;
    const auto* ptr = reinterpret_cast<const MultiPresetSlot*>(
        kQspiBase + kLiveOffset);
    if (!ptr->valid) return false;
    memcpy(&out, ptr, sizeof(out));
    return true;
}

bool QspiPresetStore::SaveLiveState(const MultiPresetSlot& data) {
    if (!initialized_) return false;
    static MultiPresetSlot buf;
    buf       = data;
    buf.valid = 1;
    qspi_->EraseSector(kQspiBase + kLiveOffset);
    qspi_->Write(kQspiBase + kLiveOffset, sizeof(buf),
                 reinterpret_cast<uint8_t*>(&buf));
    return true;
}

const char* QspiPresetStore::GetName(int bank, int slot) const {
    if (!ValidIndex(bank, slot)) return "";
    return names_[LinearIndex(bank, slot)];
}

} // namespace pedal

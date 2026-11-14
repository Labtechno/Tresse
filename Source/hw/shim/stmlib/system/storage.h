// Tresses — stockage en flash virtuel (remplace stmlib/system/storage.h).
//
// Même algorithme que stmlib/system/page_storage.h (Emilie Gillet, licence MIT) :
// sauvegarde "parcimonieuse" par blocs versionnés, effacement des pages au
// besoin, rechargement du bloc valide le plus récent. Seule différence : les
// adresses absolues de la flash pointent vers la flash virtuelle du slot, que le
// plugin enregistre dans le projet du DAW.

#ifndef STMLIB_SYSTEM_STORAGE_H_
#define STMLIB_SYSTEM_STORAGE_H_

#include "stmlib/stmlib.h"
#include "tresses_virtual_hardware.h"

namespace stmlib {

template<uint32_t last_address = 0x8020000, uint16_t num_pages = 1>
class Storage {
 public:
  enum {
    FLASH_STORAGE_BASE = last_address - num_pages * PAGE_SIZE
  };

  template<typename T>
  static void Save(const T& data) {
    Save(data, 0);
  }

  template<typename T>
  static void Save(const T& data, uint8_t page_index) {
    Save((void*)(&data), sizeof(T), page_index);
  }

  static void Save(const void* data, size_t data_size, uint8_t page_index) {
    tresses_flash_erase_page(FLASH_STORAGE_BASE + page_index * PAGE_SIZE);
    WriteBlock(FLASH_STORAGE_BASE + page_index * PAGE_SIZE, data, data_size);
  }

  template<typename T>
  static bool Load(T* data) {
    return Load(data, 0);
  }

  template<typename T>
  static bool Load(T* data, uint8_t page_index) {
    return Load((void*)(data), sizeof(T), page_index);
  }

  static bool Load(void* data, size_t data_size, uint8_t page_index) {
    uint32_t base = FLASH_STORAGE_BASE + page_index * PAGE_SIZE;
    Read(base, data, data_size);
    uint16_t checksum = ReadHalfWord(base + data_size);
    return checksum == Checksum(data, data_size);
  }

  template<typename T>
  static void ParsimoniousSave(const T& data, uint16_t* version_token) {
    ParsimoniousSave((void*)(&data), sizeof(T), version_token);
  }

  static void ParsimoniousSave(
      const void* data,
      size_t data_size,
      uint16_t* version_token) {
    bool wrapped_around = false;

    // 2 bytes of checksum and 2 bytes of version are added to the block.
    size_t block_size = data_size + 2 + 2;
    uint32_t start = FLASH_STORAGE_BASE + block_size * *version_token;
    if (start + block_size >= last_address) {
      // Erase all pages and restart the versioning from scratch.
      *version_token = 0;
      start = FLASH_STORAGE_BASE;
      wrapped_around = true;
    }

    if (wrapped_around) {
      for (size_t i = 0; i < num_pages; ++i) {
        tresses_flash_erase_page(FLASH_STORAGE_BASE + i * PAGE_SIZE);
      }
    } else {
      // If we will write into a new page, erase it.
      uint32_t previous_page = start - 1;
      previous_page -= previous_page % PAGE_SIZE;
      uint32_t this_page = start + block_size;
      this_page -= this_page % PAGE_SIZE;
      if (this_page != previous_page) {
        tresses_flash_erase_page(this_page);
      }
    }

    WriteBlock(start, data, data_size);
    ProgramHalfWord(start + data_size + 2, *version_token);
    *version_token = *version_token + 1;
  }

  template<typename T>
  static bool ParsimoniousLoad(T* data, uint16_t* version_token) {
    return ParsimoniousLoad((void*)(data), sizeof(T), version_token);
  }

  static bool ParsimoniousLoad(
      void* data,
      size_t data_size,
      uint16_t* version_token) {
    size_t block_size = data_size + 2 + 2;

    // Try from the end of the reserved area until we find a block with
    // the right checksum and the right version index.
    for (int16_t candidate_version = (num_pages * PAGE_SIZE / block_size) - 1;
         candidate_version >= 0;
         --candidate_version) {
      uint32_t start = FLASH_STORAGE_BASE + candidate_version * block_size;

      Read(start, data, data_size);
      uint16_t expected_checksum = Checksum(data, data_size);
      uint16_t read_checksum = ReadHalfWord(start + data_size);
      uint16_t version_number = ReadHalfWord(start + data_size + 2);
      if (read_checksum == expected_checksum &&
          version_number == candidate_version) {
        *version_token = version_number + 1;
        return true;
      }
    }
    // Memory appears to be corrupted or virgin - restart from scratch.
    *version_token = 0;
    return false;
  }

 private:
  static void Read(uint32_t address, void* data, size_t size) {
    uint8_t* d = static_cast<uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
      const uint8_t* p = tresses_flash_ptr(address + static_cast<uint32_t>(i));
      d[i] = p ? *p : 0xff;
    }
  }

  static uint16_t ReadHalfWord(uint32_t address) {
    uint16_t v;
    Read(address, &v, sizeof(v));
    return v;
  }

  static void ProgramHalfWord(uint32_t address, uint16_t value) {
    tresses_flash_program(address, &value, sizeof(value));
  }

  static void WriteBlock(uint32_t start, const void* data, size_t data_size) {
    const uint32_t* words = (const uint32_t*)(data);
    size_t size = data_size;
    uint32_t address = start;
    while (size >= 4) {
      tresses_flash_program(address, words++, 4);
      address += 4;
      size -= 4;
    }
    // Write checksum.
    uint16_t checksum = Checksum(data, data_size);
    ProgramHalfWord(start + data_size, checksum);
  }

  static uint16_t Checksum(const void* data, uint16_t size) {
    uint16_t s = 0;
    const uint8_t* d = static_cast<const uint8_t*>(data);
    while (size--) {
      s += *d++;
    }
    return s;
  }
};

};  // namespace stmlib

#endif  // STMLIB_SYSTEM_STORAGE_H_

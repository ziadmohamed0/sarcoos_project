#pragma once

// =============================================================================
//  SARCUS Robot — NVS Manager
//  Non-volatile storage for WiFi credentials and robot config.
//  Follows the exact same pattern as the original Takamul project.
// =============================================================================

#include "nvs_flash.h"
#include "nvs.h"
#include <string>
#include <vector>

namespace SARCUS {

// ─── NVSHandle: RAII wrapper ──────────────────────────────────────────────────
class NVSHandle {
public:
    NVSHandle(const char* namespace_name, nvs_open_mode_t mode);
    ~NVSHandle();

    // Move only (no copy)
    NVSHandle(NVSHandle&& other) noexcept;
    NVSHandle& operator=(NVSHandle&& other) noexcept;
    NVSHandle(const NVSHandle&) = delete;
    NVSHandle& operator=(const NVSHandle&) = delete;

    bool     isValid()   const { return m_handle != 0; }
    esp_err_t setString(const char* key, const std::string& value);
    esp_err_t getString(const char* key, std::string& value);
    esp_err_t setU8    (const char* key, uint8_t value);
    esp_err_t getU8    (const char* key, uint8_t& value);
    esp_err_t commit();

private:
    nvs_handle_t m_handle = 0;
};

// ─── NVSManager: Singleton ───────────────────────────────────────────────────
class NVSManager {
public:
    static NVSManager& getInstance();
    void init();

private:
    NVSManager()  = default;
    ~NVSManager() = default;
    NVSManager(const NVSManager&) = delete;
    NVSManager& operator=(const NVSManager&) = delete;
};

} // namespace SARCUS

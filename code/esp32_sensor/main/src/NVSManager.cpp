// =============================================================================
//  SARCUS Robot — NVS Manager Implementation
//  Non-volatile storage. RAII NVSHandle pattern (same as Takamul project).
// =============================================================================

#include "inc/NVSManager.h"
#include "esp_log.h"
#include <cstring>
#include <vector>

static const char* Tag = "NVSManager";

// ─── NVSHandle ────────────────────────────────────────────────────────────────

SARCUS::NVSHandle::NVSHandle(const char* namespace_name, nvs_open_mode_t mode) {
    esp_err_t err = nvs_open(namespace_name, mode, &m_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(Tag, "NVS namespace '%s' not found (first boot / cleared storage)", namespace_name);
        m_handle = 0;
    } else if (err != ESP_OK) {
        ESP_LOGE(Tag, "nvs_open('%s') failed: %s", namespace_name, esp_err_to_name(err));
        m_handle = 0;
    }
}

SARCUS::NVSHandle::~NVSHandle() {
    if (m_handle != 0) {
        nvs_close(m_handle);
        m_handle = 0;
    }
}

SARCUS::NVSHandle::NVSHandle(NVSHandle&& other) noexcept
    : m_handle(other.m_handle) {
    other.m_handle = 0;
}

SARCUS::NVSHandle& SARCUS::NVSHandle::operator=(NVSHandle&& other) noexcept {
    if (this != &other) {
        if (m_handle != 0) nvs_close(m_handle);
        m_handle       = other.m_handle;
        other.m_handle = 0;
    }
    return *this;
}

esp_err_t SARCUS::NVSHandle::setString(const char* key, const std::string& value) {
    if (!m_handle) return ESP_FAIL;
    return nvs_set_str(m_handle, key, value.c_str());
}

esp_err_t SARCUS::NVSHandle::getString(const char* key, std::string& value) {
    if (!m_handle) return ESP_FAIL;
    size_t    required = 0;
    esp_err_t err = nvs_get_str(m_handle, key, nullptr, &required);
    if (err != ESP_OK) return err;

    std::vector<char> buf(required);
    err = nvs_get_str(m_handle, key, buf.data(), &required);
    if (err == ESP_OK) value.assign(buf.data());
    return err;
}

esp_err_t SARCUS::NVSHandle::setU8(const char* key, uint8_t value) {
    if (!m_handle) return ESP_FAIL;
    return nvs_set_u8(m_handle, key, value);
}

esp_err_t SARCUS::NVSHandle::getU8(const char* key, uint8_t& value) {
    if (!m_handle) return ESP_FAIL;
    return nvs_get_u8(m_handle, key, &value);
}

esp_err_t SARCUS::NVSHandle::commit() {
    if (!m_handle) return ESP_FAIL;
    return nvs_commit(m_handle);
}

// ─── NVSManager ───────────────────────────────────────────────────────────────

SARCUS::NVSManager& SARCUS::NVSManager::getInstance() {
    static NVSManager instance;
    return instance;
}

void SARCUS::NVSManager::init() {
    ESP_LOGI(Tag, "[ENTRY] init()");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(Tag, "NVS needs erase — erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(Tag, "NVS Flash initialized OK");
    ESP_LOGI(Tag, "[EXIT]  init()");
}

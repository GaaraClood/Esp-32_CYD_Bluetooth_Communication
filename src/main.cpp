#define HMIN 600
#define HMAX 3500
#define VMIN 550
#define VMAX 3400
#define XYSWAP 1

#include <lvgl.h>
#include <TFT_Touch.h>
#include <BluetoothSerial.h>
#include <vector>

#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
#endif

#define TFT_HOR_RES   320
#define TFT_VER_RES   240
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

uint32_t draw_buf[DRAW_BUF_SIZE / 4];
TFT_Touch touch = TFT_Touch(XPT2046_CS, XPT2046_CLK, XPT2046_MOSI, XPT2046_MISO);
BluetoothSerial SerialBT;

// LVGL objeleri
lv_obj_t *screen1, *screenPassword;
lv_obj_t *deviceList = NULL;
lv_obj_t *passwordInput = NULL;
lv_obj_t *keyboard = NULL;
lv_obj_t *statusLabel = NULL;

// Değişkenler
bool btConnected = false;
bool isScanning = false;
String selectedDevice = "";
std::vector<String> discoveredDevices;
std::vector<String> discoveredAddresses;

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
    lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    bool touched = touch.Pressed();
    if(!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.X();
        data->point.y = touch.Y();
    }
}

static uint32_t my_tick(void) {
    return millis();
}

void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if(event == ESP_SPP_SRV_OPEN_EVT) {
        btConnected = true;
        Serial.println("BT baglandi!");
    } else if(event == ESP_SPP_CLOSE_EVT) {
        btConnected = false;
        Serial.println("BT kesildi!");
    }
}

// Cihaz listesini güncelle
void updateDeviceList() {
    if(deviceList == NULL) return;
    lv_obj_clean(deviceList);

    if(discoveredDevices.size() == 0) {
        lv_obj_t *label = lv_label_create(deviceList);
        lv_label_set_text(label, "Cihaz bulunamadi");
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF6B6B), 0);
        return;
    }

    for(size_t i = 0; i < discoveredDevices.size(); i++) {
        lv_obj_t *btn = lv_button_create(deviceList);
        lv_obj_set_size(btn, 280, 35);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2C3E50), 0);
        lv_obj_set_style_radius(btn, 6, 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, discoveredDevices[i].c_str());
        lv_obj_center(label);

        lv_obj_set_user_data(btn, (void*)i);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            lv_obj_t *target = lv_event_get_target(e);
            size_t idx = (size_t)lv_obj_get_user_data(target);
            selectedDevice = discoveredDevices[idx];
            Serial.println("Secilen: " + selectedDevice);

            lv_screen_load(screenPassword);
        }, LV_EVENT_CLICKED, NULL);
    }
}

// Klasik Bluetooth cihazlarını tara
void scanBTDevices() {
    if(isScanning) return;
    isScanning = true;

    if(statusLabel) {
        lv_label_set_text(statusLabel, "TARANIYOR...");
        lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xFFFF00), 0);
    }

    discoveredDevices.clear();
    discoveredAddresses.clear();

    Serial.println("Bluetooth cihazlari taraniyor...");
    int16_t num = SerialBT.discover([](const char* name, const uint8_t* address) {
        char addrStr[18];
        sprintf(addrStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            address[0], address[1], address[2],
            address[3], address[4], address[5]);

        String deviceName = name ? String(name) : String("Bilinmeyen");
        discoveredDevices.push_back(deviceName + " [" + addrStr + "]");
        discoveredAddresses.push_back(addrStr);

        Serial.printf("Bulundu: %s\n", (deviceName + " [" + addrStr + "]").c_str());
    });

    Serial.printf("Tarama bitti! %d cihaz bulundu\n", num);

    char buf[32];
    sprintf(buf, "%d cihaz bulundu", (int)discoveredDevices.size());
    lv_label_set_text(statusLabel, buf);
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x00FF00), 0);

    updateDeviceList();
    isScanning = false;
}

// Bağlan butonu callback
void connectToBT(lv_event_t *e) {
    Serial.println("Baglaniyor: " + selectedDevice);

    // Seçilen adresi al
    int start = selectedDevice.indexOf('[') + 1;
    int end = selectedDevice.indexOf(']');
    String mac = selectedDevice.substring(start, end);
    mac.replace(":", "");

    uint8_t addr[6];
    sscanf(mac.c_str(), "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
        &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);

    if(SerialBT.connect(addr)) {
        Serial.println("Baglanti basarili!");
        lv_obj_t *msg = lv_msgbox_create(NULL);
        lv_msgbox_add_title(msg, "BASARILI");
        lv_msgbox_add_text(msg, "Baglanti kuruldu!");
        lv_msgbox_add_close_button(msg);
    } else {
        Serial.println("Baglanti basarisiz!");
        lv_obj_t *msg = lv_msgbox_create(NULL);
        lv_msgbox_add_title(msg, "HATA");
        lv_msgbox_add_text(msg, "Baglanti basarisiz!\nCihaz kapali olabilir.");
        lv_msgbox_add_close_button(msg);
    }
}

// EKRAN 1: Cihaz tarama
void createScreen1() {
    screen1 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0x1a1a2e), 0);

    lv_obj_t *title = lv_label_create(screen1);
    lv_label_set_text(title, "BLUETOOTH TARAMA");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    statusLabel = lv_label_create(screen1);
    lv_label_set_text(statusLabel, "HAZIR");
    lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0x00D9FF), 0);

    lv_obj_t *btnScan = lv_button_create(screen1);
    lv_obj_set_size(btnScan, 120, 35);
    lv_obj_align(btnScan, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(btnScan, lv_color_hex(0x00D9FF), 0);
    lv_obj_t *label = lv_label_create(btnScan);
    lv_label_set_text(label, "TARA");
    lv_obj_center(label);
    lv_obj_add_event_cb(btnScan, [](lv_event_t *e) {
        scanBTDevices();
    }, LV_EVENT_CLICKED, NULL);

    deviceList = lv_list_create(screen1);
    lv_obj_set_size(deviceList, 300, 140);
    lv_obj_align(deviceList, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(deviceList, lv_color_hex(0x16213e), 0);
}

// EKRAN 2: Şifre ekranı
void createPasswordScreen() {
    screenPassword = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screenPassword, lv_color_hex(0x1a1a2e), 0);

    lv_obj_t *title = lv_label_create(screenPassword);
    lv_label_set_text(title, "Cihaz Baglanti");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    passwordInput = lv_textarea_create(screenPassword);
    lv_obj_set_size(passwordInput, 200, 35);
    lv_obj_align(passwordInput, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_placeholder_text(passwordInput, "Sifre (opsiyonel)");
    lv_textarea_set_password_mode(passwordInput, true);

    lv_obj_t *btnConnect = lv_button_create(screenPassword);
    lv_obj_set_size(btnConnect, 110, 32);
    lv_obj_align(btnConnect, LV_ALIGN_BOTTOM_LEFT, 15, -5);
    lv_obj_set_style_bg_color(btnConnect, lv_color_hex(0x00FF00), 0);
    lv_obj_t *label = lv_label_create(btnConnect);
    lv_label_set_text(label, "BAGLAN");
    lv_obj_center(label);
    lv_obj_add_event_cb(btnConnect, connectToBT, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btnBack = lv_button_create(screenPassword);
    lv_obj_set_size(btnBack, 90, 32);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_RIGHT, -15, -5);
    lv_obj_set_style_bg_color(btnBack, lv_color_hex(0xFF6B6B), 0);
    lv_obj_t *label2 = lv_label_create(btnBack);
    lv_label_set_text(label2, "< GERI");
    lv_obj_center(label2);
    lv_obj_add_event_cb(btnBack, [](lv_event_t *e) {
        lv_screen_load(screen1);
    }, LV_EVENT_CLICKED, NULL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 Bluetooth Scanner ===");

    if(!SerialBT.begin("ESP32_Master", true)) {
        Serial.println("Bluetooth baslatilamadi!");
    } else {
        Serial.println("Bluetooth Master baslatildi");
        SerialBT.register_callback(btCallback);
    }

    touch.setCal(HMIN, HMAX, VMIN, VMAX, TFT_HOR_RES, TFT_VER_RES, XYSWAP);
    touch.setRotation(3);

    lv_init();
    lv_tick_set_cb(my_tick);

    lv_display_t *disp;
#if LV_USE_TFT_ESPI
    disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, TFT_ROTATION);
#else
    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    createScreen1();
    createPasswordScreen();
    lv_screen_load(screen1);

    Serial.println("=== Hazir! ===");
}

void loop() {
    lv_timer_handler();
    delay(5);

    if(SerialBT.available()) {
        char c = SerialBT.read();
        Serial.print("Gelen: ");
        Serial.println(c);
    }
}

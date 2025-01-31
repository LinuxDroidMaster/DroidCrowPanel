#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include <ESPping.h>
#include "ui.h"
#include "gfx_conf.h"

// Configuración Wi-Fi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// URL de la API de TimeAPI.io para Madrid
const char* timeApiUrl = "https://timeapi.io/api/time/current/zone?timeZone=Europe%2FMadrid";

// Intervalo de actualización de los suscriptores (30 segundos)
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 120000;

// Nombres de dominio de los dispositivos
const char* indiedroidNovaDomain = "droidmaster-nova";
const char* orangePi5Domain = "droidmaster-opi5";
const char* orangePiZero3Domain = "droidmaster-opiz3";

// Dirección IP del Raspberry Pi
const char* raspberryPiIp = "192.168.1.1";


// API de YouTube
const char* api_key = "YOUR_API_KEY"; // Reemplaza con tu clave de API
const char* channel_id = "YOUR_CHANNEL_ID"; // ID del canal
const char* api_url = "https://www.googleapis.com/youtube/v3/channels?part=statistics&id=";

static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 10];
static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 10];
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}


// Función para verificar si un dispositivo es accesible usando ESPPing
bool isDeviceAccessible(const char* domainOrIp) {
    // Hacer un ping a la dirección IP o dominio
    if (Ping.ping(domainOrIp)) {  // Si el ping es exitoso
        return true; // El dispositivo está accesible
    }
    return false;
}

// Función para actualizar el estado de los checkboxes
void actualizarEstadoDispositivos() {
    // Verificar si "droidmaster-nova" es accesible
    if (isDeviceAccessible(indiedroidNovaDomain)) {
        lv_obj_add_state(ui_checkboxindiedroidnova, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_checkboxindiedroidnova, LV_STATE_CHECKED);
    }

    // Verificar si "orange-pi5" es accesible
    if (isDeviceAccessible(orangePi5Domain)) {
        lv_obj_add_state(ui_checkboxorangepi5, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_checkboxorangepi5, LV_STATE_CHECKED);
    }

    // Verificar si "droidmaster-opiz3" es accesible
    if (isDeviceAccessible(orangePiZero3Domain)) {
        lv_obj_add_state(ui_checkboxpizero1, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_checkboxpizero1, LV_STATE_CHECKED);
    }

    // Verificar si "192.168.31.11" (Raspberry Pi) es accesible
    if (isDeviceAccessible(raspberryPiIp)) {
        lv_obj_add_state(ui_checkboxraspi, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_checkboxraspi, LV_STATE_CHECKED);
    }
}


void setup() {
    Serial.begin(9600);
    Serial.println("LVGL Widgets Demo");

    tft.begin();
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    delay(200);

    lv_init();
    delay(100);

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, screenWidth * screenHeight / 10);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.full_refresh = 1;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    tft.fillScreen(TFT_BLACK);
    ui_init();

    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    obtenerSuscriptores();
    actualizarEstadoDispositivos();  // Actualizar el estado de los dispositivos al inicio
}

void loop() {
    lv_timer_handler();
    delay(5);

    // Verificar si ha pasado el tiempo de actualización
    if (millis() - lastUpdateTime >= updateInterval) {
        obtenerSuscriptores();  // Actualizar los suscriptores y la hora
        actualizarEstadoDispositivos();  // Actualizar el estado de los dispositivos
        lastUpdateTime = millis();  // Resetear el temporizador
    }
}

void obtenerSuscriptores() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(api_url) + channel_id + "&key=" + api_key;
        http.begin(url);
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            Serial.println("HTTP Response:");
            Serial.println(payload);

            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                int subscriberCount = doc["items"][0]["statistics"]["subscriberCount"];
                Serial.print("Subscriber Count: ");
                Serial.println(subscriberCount);

                // Convertir el número de suscriptores a texto
                String subscriberCountText = String(subscriberCount);

                // Actualizar el texto de la etiqueta ui_labelsubscount con el número de suscriptores
                lv_label_set_text(ui_labelsubscount, subscriberCountText.c_str());

                // Obtener la fecha y hora actual de Madrid
                String fechaYHoraMadrid = obtenerHoraActual();

                // Actualizar la etiqueta de última actualización con la fecha y hora actuales
                String lastUpdatedText = "Last updated: " + fechaYHoraMadrid;
                lv_label_set_text(ui_labellastupdated, lastUpdatedText.c_str());
            } else {
                Serial.println("JSON Parsing Failed for Subscribers");
            }
        } else {
            Serial.println("HTTP Request Failed for Subscribers");
        }
        http.end();
    } else {
        Serial.println("Wi-Fi Not Connected");
    }
}

String obtenerHoraActual() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(timeApiUrl);
        int httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                const char* date = doc["date"];
                const char* time = doc["time"];
                Serial.print("Fecha actual en Madrid (original): ");
                Serial.println(date);
                Serial.print("Hora actual en Madrid: ");
                Serial.println(time);

                // Dividir la fecha en componentes mm/dd/aaaa
                String dateStr = String(date);
                int firstSlash = dateStr.indexOf('/');
                int secondSlash = dateStr.indexOf('/', firstSlash + 1);

                String month = dateStr.substring(0, firstSlash);
                String day = dateStr.substring(firstSlash + 1, secondSlash);
                String year = dateStr.substring(secondSlash + 1);

                // Reformatear a dd/mm/aaaa
                String formattedDate = day + "/" + month + "/" + year;
                Serial.print("Fecha actual en Madrid (reformateada): ");
                Serial.println(formattedDate);

                // Concatenar fecha reformateada y hora
                String fechaYHora = formattedDate + " " + String(time);
                return fechaYHora;
            } else {
                Serial.println("Error al analizar el JSON");
            }
        } else {
            Serial.println("Error en la solicitud HTTP");
        }
        http.end();
    } else {
        Serial.println("No conectado al Wi-Fi");
    }
    return "Error al obtener hora";
}


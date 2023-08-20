#include "esp_err.h"
#include "esp_log.h"

#include "parameters.h"

static const char *TAG = "IXTLI_CONF";
const char *g_params_names[] = {
    "wifi_ssid",
    "wifi_pass",
    "cam_jpeg_quality",
    "cam_frame_size",
    };

IxtliPersistentSettings global_params = {"fake_ap", "dummy", 1, 1};

int dloadPersistentSettings(const char* filename) {

    return 0;
}

int loadPersistentSettings(const char* filename) {
    IxtliParamEnum next = WF_SSID;
    char line[CONF_LINE_SIZE];

    FILE *conf_file = fopen(filename, "r");
    while (fgets(line, CONF_LINE_SIZE, conf_file)) {
        line[strcspn(line, "\n")] = 0;
        const char* val = strrchr(line, '=') + 1; // +1 to remove the '=' char
        switch (next) {
        case WF_SSID:
            strcpy(global_params.wifi_ssid, val);
            next = WF_PASS;
            break;
        case WF_PASS:
            strcpy(global_params.wifi_pass, val);
            next = CAM_JPEG_QUALITY;
            break;
        case CAM_JPEG_QUALITY:
            global_params.cam_jpeg_quality = atoi(val);
            next = CAM_FRAME_SIZE;
            break;
        case CAM_FRAME_SIZE:
            global_params.cam_frame_size = atoi(val);
            next = PARAM_END;
            break;
        case PARAM_END:
        default:
            goto free_file;
            break;
        }
    }

free_file:
    fclose(conf_file);
    return (int)next;
}

int pushPersistentSettings(const char* filename) {
    IxtliParamEnum next = WF_SSID;
    FILE *conf_file = fopen(filename, "w");
    if (!conf_file)
        return ESP_FAIL;
    fseek(conf_file, 0, SEEK_SET);

    while (next != PARAM_END) {
        switch (next) {
        case WF_SSID:
            fprintf(conf_file, "%s=%s\n", g_params_names[next], global_params.wifi_ssid);
            next = WF_PASS;
            break;
        case WF_PASS:
            fprintf(conf_file, "%s=%s\n", g_params_names[next], global_params.wifi_pass);
            next = CAM_JPEG_QUALITY;
            break;
        case CAM_JPEG_QUALITY:
            fprintf(conf_file, "%s=%d\n", g_params_names[next], global_params.cam_jpeg_quality);
            next = CAM_FRAME_SIZE;
            break;
        case CAM_FRAME_SIZE:
            fprintf(conf_file, "%s=%d\n", g_params_names[next], global_params.cam_frame_size);
            next = PARAM_END;
            break;
        case PARAM_END:
        default:
            goto free_file;
        }
    }

free_file:
    fclose(conf_file);
    return (int)next;
}
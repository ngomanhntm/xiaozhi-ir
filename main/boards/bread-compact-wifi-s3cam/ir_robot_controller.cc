/**
 * IR Robot Dog Controller - MCP Tool
 *
 * Tích hợp phát lệnh IR hồng ngoại vào hệ thống MCP của Xiaozhi ESP32.
 * Model AI có thể gọi tool "self.robot.move" để điều khiển robot dog
 * bằng giọng nói (tiến, lùi, trái, phải, dừng).
 *
 * Giao thức Robot Dog:
 *   Cấu trúc 1 frame: [Header] + [8 data bits] + [Stop bit]
 *     Header : L=6215µs (carrier ON), H=514µs (carrier OFF)
 *     Bit "0": L=1651µs, H=612µs
 *     Bit "1": L=663µs,  H=1590µs
 *   1 lần bấm = 9 frame liên tiếp cách nhau ~120ms
 *   Chỉ frame đầu tiên chứa mã lệnh, 8 frame sau là repeat (toàn bit 0)
 *
 * Mã lệnh (8-bit, MSB first):
 *   Tiến  : 0b00010000 (0x10)
 *   Lùi   : 0b00001010 (0x0A)
 *   Trái  : 0b00001101 (0x0D)
 *   Phải  : 0b00001001 (0x09)
 */

#include "ir_robot_controller.h"
#include "mcp_server.h"
#include "application.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <cstring>
#include <string>
#include <stdexcept>

#define TAG "IrRobotCtrl"

#ifdef IR_RX_GPIO
typedef struct {
    uint32_t duration;
    int level;
} pulse_t;

static QueueHandle_t s_rx_queue = nullptr;

static void IRAM_ATTR ir_rx_gpio_isr_handler(void* arg)
{
    static uint64_t last_time = 0;
    uint64_t now = esp_timer_get_time();
    int gpio_num = (int)arg;
    int level = gpio_get_level((gpio_num_t)gpio_num);
    
    pulse_t pulse;
    pulse.level = level;
    if (last_time != 0) {
        pulse.duration = (uint32_t)(now - last_time);
        xQueueSendFromISR(s_rx_queue, &pulse, NULL);
    }
    last_time = now;
}

// ---------------------------------------------------------------
// Kỳ vọng thời gian nhận (đo thực tế với remote gốc)
// ---------------------------------------------------------------
#define RX_HDR_L   6215
#define RX_HDR_H    514
#define RX_BIT0_L  1651
#define RX_BIT0_H   612
#define RX_BIT1_L   663
#define RX_BIT1_H  1590

static void print_rx_result(pulse_t* buf, int count, int frame_num)
{
    ESP_LOGI(TAG, "--- Frame #%d | So xung: %d ---", frame_num, count);

    if (count < 17) {
        ESP_LOGI(TAG, "  !!! It xung qua (%d) - Tin hieu bi cat hoac nhieu !!!", count);
        for (int i = 0; i < count && i < 20; i++) {
            ESP_LOGI(TAG, "  [%2d] %8u us  level=%d", i, buf[i].duration, buf[i].level);
        }
        return;
    }

    static const uint32_t exp_l[] = { RX_HDR_L, RX_BIT0_L, RX_BIT0_L, RX_BIT0_L, RX_BIT0_L,
                                       RX_BIT0_L, RX_BIT0_L, RX_BIT0_L, RX_BIT0_L };
    static const uint32_t exp_h[] = { RX_HDR_H, RX_BIT0_H, RX_BIT0_H, RX_BIT0_H, RX_BIT0_H,
                                       RX_BIT0_H, RX_BIT0_H, RX_BIT0_H, 0 };

    ESP_LOGI(TAG, "  [i]  L_thu  L_exp  DeltaL  | H_thu  H_exp  DeltaH");
    ESP_LOGI(TAG, "  ----+------+------+--------+------+------+--------");

    int sym = 0;
    for (int i = 0; i < count - 1 && sym < 9; i += 2, sym++) {
        uint32_t l = buf[i].duration;
        uint32_t h = (i + 1 < count) ? buf[i + 1].duration : 0;
        int32_t dl = (int32_t)l - (int32_t)exp_l[sym];
        int32_t dh = (int32_t)h - (int32_t)exp_h[sym];
        const char* mark = (sym >= 1 && l < 1000 && h > 1000) ? " !1" : "   ";
        ESP_LOGI(TAG, "  [%d]  %5u %5u  %+5d%s | %5u %5u  %+5d%s",
                 sym, l, exp_l[sym], dl, mark, h, exp_h[sym], dh, mark);
    }

    int code = 0;
    for (int i = 2, bit_idx = 0; i < count && bit_idx < 8; i += 2, bit_idx++) {
        if (buf[i].duration < 1000) {
            code |= (1 << (7 - bit_idx));
        }
    }
    ESP_LOGI(TAG, "  ==> Decoded: 0x%02X (%s)", code,
             code == 0x10 ? "TIEN (Forward)"  :
             code == 0x0A ? "LUI (Backward)"  :
             code == 0x0D ? "TRAI (Left)"     :
             code == 0x09 ? "PHAI (Right)"    :
             code == 0x06 ? "MO NHAC (Music)"          :
             code == 0x07 ? "TIEN BUOC (Step Fwd)"     :
             code == 0x11 ? "TRAI BUOC (Step Left)"    :
             code == 0x12 ? "LUI BUOC (Step Backward)"  :
             code == 0x08 ? "PHAI BUOC (Step Right)"   :
             code == 0x0B ? "TOGGLE (Ngoi<->Dung)"    :
             code == 0x13 ? "DUOI CHAN (Stretch)"       :
             code == 0x0F ? "DUNG LAI IR (Halt)"       :
             code == 0x00 ? "REPEAT"                   : "KHONG XAC DINH");
}

static void ir_rx_task(void* pvParameters)
{
    const int MAX_PULSES = 100;
    pulse_t packet[MAX_PULSES];
    int pulse_count = 0;
    int frame_num = 0;
    pulse_t pulse;
    
    ESP_LOGI(TAG, "Nhiem vu ir_rx_task da bat dau chay.");
    
    while (true) {
        if (xQueueReceive(s_rx_queue, &pulse, pdMS_TO_TICKS(100)) == pdTRUE) {
            // In log cho moi xung nhan duoc tu ISR de chan doan phan cung
            ESP_LOGD(TAG, "[ISR RX] Xung: dur=%u us, level=%d", pulse.duration, pulse.level);
            
            if (pulse.duration > 50000) { // > 50ms (loc gap giua cac frame ~120ms)
                ESP_LOGD(TAG, "[ISR RX] Gap phat hien: %u us, reset pulse_count", pulse.duration);
                pulse_count = 0;
                continue;
            }
            if (pulse_count < MAX_PULSES) {
                packet[pulse_count++] = pulse;
            }
        } else {
            if (pulse_count > 0) {
                print_rx_result(packet, pulse_count, ++frame_num);
                pulse_count = 0;
            }
        }
    }
}
#endif

// ---------------------------------------------------------------
// Task in dòng phân cách log khi nhấn Space
// ---------------------------------------------------------------
static void separator_task(void* pvParameters)
{
    static int sep_num = 0;
    if (!uart_is_driver_installed(UART_NUM_0)) {
        uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    }
    uint8_t ch;
    while (true) {
        if (uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(100)) > 0) {
            if (ch == ' ') {
                printf("\n");
                ESP_LOGI(TAG, "\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550 [SEPARATOR #%d] \u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550", ++sep_num);
                printf("\n");
            }
        }
    }
}

// ---------------------------------------------------------------
// Timing phát (đã bù trừ độ trễ quang học)
// ---------------------------------------------------------------
#define TX_HDR_L   6245   // Header: carrier ON  ~6215µs
#define TX_HDR_H    460   // Header: carrier OFF ~514µs
#define TX_BIT0_L  1690   // Bit "0": L ~1651µs
#define TX_BIT0_H   570   // Bit "0": H ~612µs
#define TX_BIT1_L   625   // Bit "1": L ~663µs
#define TX_BIT1_H  1640   // Bit "1": H ~1590µs
#define TX_STOP_L  1690   // Stop: L ~1651µs
#define TX_STOP_H  2000   // Stop: trailing space

// ---------------------------------------------------------------
// Mã lệnh
// ---------------------------------------------------------------
typedef enum {
    CMD_FORWARD  = 0b00010000,  // 0x10 - Tiến
    CMD_BACKWARD = 0b00001010,  // 0x0A - Lùi
    CMD_LEFT     = 0b00001101,  // 0x0D - Trái
    CMD_RIGHT    = 0b00001001,  // 0x09 - Phải
    CMD_MUSIC         = 0b00000110,  // 0x06 - Mở nhạc
    CMD_STEP_FORWARD  = 0b00000111,  // 0x07 - Tiến bước (chân+bánh)
    CMD_STEP_LEFT     = 0b00010001,  // 0x11 - Trái từng bước
    CMD_STEP_BACKWARD = 0b00010010,  // 0x12 - Lùi từng bước
    CMD_STEP_RIGHT    = 0b00001000,  // 0x08 - Phải từng bước
    CMD_TOGGLE       = 0b00001011,  // 0x0B - Chuyển trạng thái ngồi/đứng
    CMD_STRETCH      = 0b00010011,  // 0x13 - Duỗi chân
    CMD_HALT         = 0b00001111,  // 0x0F - Dừng lại (phát IR)
} dog_cmd_t;

// ---------------------------------------------------------------
// Biến toàn cục RMT (chỉ khởi tạo một lần)
// ---------------------------------------------------------------
static rmt_channel_handle_t s_tx_channel   = nullptr;
static rmt_encoder_handle_t s_copy_encoder = nullptr;
static bool                 s_initialized  = false;

// Biến trạng thái phát IR liên tục không khựng
static TaskHandle_t s_ir_continuous_task_handle = nullptr;
static dog_cmd_t s_current_continuous_cmd = CMD_HALT;
static volatile bool s_is_continuous_active = false;
static uint32_t s_continuous_duration_ms = 2000;

// Biến trạng thái nhảy múa biểu diễn
static TaskHandle_t s_ir_dance_task_handle = nullptr;
static volatile bool s_is_dance_active = false;

// Hàm dừng mọi task IR chạy ngầm đang hoạt động
static void stop_all_active_ir_tasks()
{
    if (s_is_continuous_active) {
        s_is_continuous_active = false;
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    if (s_is_dance_active) {
        s_is_dance_active = false;
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ---------------------------------------------------------------
// Xây dựng mảng xung phát cho 1 frame (Header + 8 data bits)
// ---------------------------------------------------------------
static void build_tx_frame(rmt_symbol_word_t* symbols, dog_cmd_t cmd, bool is_first_frame)
{
    // [0] Header
    symbols[0].duration0 = TX_HDR_L;
    symbols[0].level0    = 1;
    symbols[0].duration1 = TX_HDR_H;
    symbols[0].level1    = 0;

    // [1]-[8] Data bits (8 bit, MSB first)
    for (int i = 0; i < 8; i++) {
        bool bit_val = is_first_frame && ((cmd >> (7 - i)) & 1);
        if (bit_val) {
            symbols[1 + i].duration0 = TX_BIT1_L;
            symbols[1 + i].level0    = 1;
            symbols[1 + i].duration1 = TX_BIT1_H;
            symbols[1 + i].level1    = 0;
        } else {
            symbols[1 + i].duration0 = TX_BIT0_L;
            symbols[1 + i].level0    = 1;
            symbols[1 + i].duration1 = TX_BIT0_H;
            symbols[1 + i].level1    = 0;
        }
    }
}

// ---------------------------------------------------------------
// Task phát lệnh IR liên tục không khựng (sử dụng repeat frame)
// ---------------------------------------------------------------
static void ir_continuous_tx_task(void* pvParameters)
{
    rmt_symbol_word_t frame[9];
    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = 0;

    uint64_t start_time = esp_timer_get_time() / 1000;

    ESP_LOGI(TAG, "Continuous IR task started (sending CMD repeatedly): cmd=0x%02X, duration=%u ms", 
             (uint8_t)s_current_continuous_cmd, s_continuous_duration_ms);

    // 1. Phát frame lệnh thực đầu tiên để bắt đầu di chuyển
    build_tx_frame(frame, s_current_continuous_cmd, true);
    esp_err_t err = rmt_transmit(s_tx_channel, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
    if (err == ESP_OK) {
        rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. Liên tục phát các frame lệnh thực cách nhau ~125ms (25ms truyền + 100ms delay)
    while (s_is_continuous_active) {
        uint64_t current_time = esp_timer_get_time() / 1000;
        if ((current_time - start_time) >= s_continuous_duration_ms) {
            ESP_LOGI(TAG, "Continuous duration reached (%u ms). Exiting loop.", s_continuous_duration_ms);
            break;
        }

        build_tx_frame(frame, s_current_continuous_cmd, true);
        err = rmt_transmit(s_tx_channel, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
        if (err == ESP_OK) {
            rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 3. Khi dừng di chuyển, phát lệnh HALT (0x0F) để robot dừng lập tức
    ESP_LOGI(TAG, "Stopping continuous move: sending HALT command.");
    build_tx_frame(frame, CMD_HALT, true);
    err = rmt_transmit(s_tx_channel, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
    if (err == ESP_OK) {
        rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
    }

    s_is_continuous_active = false;
    s_ir_continuous_task_handle = nullptr;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------
// Phát 1 lệnh hoàn chỉnh (9 frame)
// ---------------------------------------------------------------
static void send_ir_command(dog_cmd_t cmd)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "IR chưa được khởi tạo!");
        return;
    }

    rmt_symbol_word_t frame[9];
    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = 0;

    const char* cmd_str = "UNKNOWN";
    switch (cmd) {
        case CMD_FORWARD:  cmd_str = "TIEN (Forward)";  break;
        case CMD_BACKWARD: cmd_str = "LUI (Backward)";  break;
        case CMD_LEFT:     cmd_str = "TRAI (Left)";     break;
        case CMD_RIGHT:    cmd_str = "PHAI (Right)";    break;
        case CMD_MUSIC:         cmd_str = "MO NHAC (Music)";        break;
        case CMD_STEP_FORWARD:  cmd_str = "TIEN BUOC (Step Fwd)";  break;
        case CMD_STEP_LEFT:     cmd_str = "TRAI BUOC (Step Left)"; break;
        case CMD_STEP_BACKWARD: cmd_str = "LUI BUOC (Step Back)";  break;
        case CMD_STEP_RIGHT:    cmd_str = "PHAI BUOC (Step Right)";break;
        case CMD_TOGGLE:        cmd_str = "TOGGLE (Ngoi<->Dung)"; break;
        case CMD_STRETCH:       cmd_str = "DUOI CHAN (Stretch)";   break;
        case CMD_HALT:          cmd_str = "DUNG LAI IR (Halt)";   break;
    }
    ESP_LOGI(TAG, ">>> Phat lenh IR: %s (0x%02X)", cmd_str, (uint8_t)cmd);

    for (int n = 1; n <= 9; n++) {
        build_tx_frame(frame, cmd, (n == 1));
        esp_err_t err = rmt_transmit(s_tx_channel, s_copy_encoder,
                                     frame, sizeof(frame), &tx_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
            return;
        }
        err = rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(err));
            return;
        }
        if (n < 9) {
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }

    ESP_LOGI(TAG, "<<< Hoan thanh: %s", cmd_str);
}

// ---------------------------------------------------------------
// Task FreeRTOS chạy chuỗi nhảy múa (dance routine) của Robot Dog
// ---------------------------------------------------------------
static void ir_dance_tx_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Dance routine started.");
    
    // Helper lambda để gửi lệnh nhấp nhả và chờ một khoảng thời gian
    auto run_cmd_click = [](dog_cmd_t cmd, uint32_t delay_after_ms) {
        if (!s_is_dance_active) return;
        send_ir_command(cmd);
        vTaskDelay(pdMS_TO_TICKS(delay_after_ms));
    };

    // Helper lambda để gửi lệnh ấn giữ (continuous) trong một khoảng thời gian
    auto run_cmd_hold = [](dog_cmd_t cmd, uint32_t hold_duration_ms, uint32_t delay_after_ms) {
        if (!s_is_dance_active) return;
        rmt_symbol_word_t frame[9];
        rmt_transmit_config_t tx_cfg = {};
        tx_cfg.loop_count = 0;
        
        uint64_t start_time = esp_timer_get_time() / 1000;
        
        // 1. Phát frame lệnh thực đầu tiên để bắt đầu di chuyển
        build_tx_frame(frame, cmd, true);
        esp_err_t err = rmt_transmit(s_tx_channel, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
        if (err == ESP_OK) {
            rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        // 2. Liên tục phát các frame lệnh thực cách nhau ~125ms (25ms truyền + 100ms delay)
        while (s_is_dance_active) {
            uint64_t current_time = esp_timer_get_time() / 1000;
            if ((current_time - start_time) >= hold_duration_ms) {
                break;
            }
            build_tx_frame(frame, cmd, true);
            err = rmt_transmit(s_tx_channel, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
            if (err == ESP_OK) {
                rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // 3. Phát lệnh HALT (0x0F) để robot dừng ngay lập tức
        build_tx_frame(frame, CMD_HALT, true);
        err = rmt_transmit(s_tx_channel, s_copy_encoder, frame, sizeof(frame), &tx_cfg);
        if (err == ESP_OK) {
            rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(delay_after_ms));
    };

    // --- Chuỗi các lệnh biểu diễn (dance routine) ---
    
    // 1. Phát nhạc - trễ 5.0s để nhạc chạy một lát
    run_cmd_click(CMD_MUSIC, 5000);

    // 2. Đứng lên / ngồi xuống chào mừng - trễ 2s
    run_cmd_click(CMD_TOGGLE, 2000);

    // 3. Duỗi chân khởi động - trễ 2.5s
    run_cmd_click(CMD_STRETCH, 2500);

    // 4. Tiến lên liên tục 3 giây - trễ 200ms
    run_cmd_hold(CMD_FORWARD, 3000, 200);

    // 5. Rẽ trái liên tục xoay vòng 3 giây - trễ 200ms
    run_cmd_hold(CMD_LEFT, 3000, 200);

    // 6. Đi lùi liên tục 3 giây - trễ 200ms
    run_cmd_hold(CMD_BACKWARD, 3000, 200);

    // 7. Rẽ phải liên tục xoay vòng 3 giây - trễ 200ms
    run_cmd_hold(CMD_RIGHT, 3000, 200);

    // 8. Đứng lên / ngồi xuống lần nữa - trễ 2s
    run_cmd_click(CMD_TOGGLE, 2000);

    // 9. Lệnh dừng hoàn toàn
    run_cmd_click(CMD_HALT, 500);

    ESP_LOGI(TAG, "Dance routine finished.");
    s_is_dance_active = false;
    s_ir_dance_task_handle = nullptr;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------
// Khởi tạo RMT TX và đăng ký MCP tools
// ---------------------------------------------------------------
void InitializeIrRobotController(int ir_tx_gpio)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "IR da duoc khoi tao truoc do, bo qua.");
        return;
    }

    ESP_LOGI(TAG, "Khoi tao IR Robot Controller tren GPIO%d...", ir_tx_gpio);

    // Cấu hình kênh RMT TX
    rmt_tx_channel_config_t tx_cfg = {};
    tx_cfg.gpio_num          = (gpio_num_t)ir_tx_gpio;
    tx_cfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    tx_cfg.resolution_hz     = 1000000;   // 1µs mỗi tick
    tx_cfg.mem_block_symbols = 64;
    tx_cfg.trans_queue_depth = 4;
    tx_cfg.flags.invert_out  = false;
    tx_cfg.flags.with_dma    = false;

    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &s_tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel that bai: %s", esp_err_to_name(err));
        return;
    }

    // Sóng mang 38kHz, duty 33%
    rmt_carrier_config_t carrier_cfg = {};
    carrier_cfg.frequency_hz              = 38000;
    carrier_cfg.duty_cycle                = 0.33f;
    carrier_cfg.flags.polarity_active_low = false;

    err = rmt_apply_carrier(s_tx_channel, &carrier_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_apply_carrier that bai: %s", esp_err_to_name(err));
        return;
    }

    err = rmt_enable(s_tx_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_enable that bai: %s", esp_err_to_name(err));
        return;
    }

    // Tạo copy encoder
    rmt_copy_encoder_config_t enc_cfg = {};
    err = rmt_new_copy_encoder(&enc_cfg, &s_copy_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_copy_encoder that bai: %s", esp_err_to_name(err));
        return;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "IR TX khoi tao thanh cong: GPIO%d, 38kHz, duty=33%%", ir_tx_gpio);

    // ---------------------------------------------------------------
    // Đăng ký MCP tool cho model AI
    // ---------------------------------------------------------------
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool(
        "self.robot.move",
        "Điều khiển robot dog bằng lệnh IR hồng ngoại. "
        "Dùng khi người dùng nói các lệnh như: 'đi thẳng', 'tiến lên', "
        "'lùi lại', 'quay trái', 'quay phải', 'mở nhạc', "
        "'đi bộ', 'trạng thái 1', 'trạng thái 2', 'trạng thái 3', 'trạng thái 4', 'ngồi/đứng', 'duỗi chân', 'dừng lại'.\n"
        "Tham số `command`:\n"
        "  - `forward`       : Tiến (bánh xe)\n"
        "  - `backward`      : Lùi (bánh xe)\n"
        "  - `left`          : Quay trái (bánh xe)\n"
        "  - `right`         : Quay phải (bánh xe)\n"
        "  - `music`         : Mở nhạc / bật nhạc\n"
        "  - `trang_thai_1`  : Trạng thái 1 — Tiến bước (chân + bánh)\n"
        "  - `trang_thai_2`  : Trạng thái 2 — Quay trái từng bước\n"
        "  - `trang_thai_3`  : Trạng thái 3 — Lùi từng bước\n"
        "  - `trang_thai_4`  : Trạng thái 4 — Quay phải từng bước\n"
        "  - `toggle`        : Chuyển trạng thái ngồi/đứng (toggle)\n"
        "  - `stretch`       : Duỗi chân\n"
        "  - `halt`          : Dừng lại (phát lệnh IR)\n"
        "Tham số `repeat` (mặc định 1, chỉ dùng khi duration_ms=0): số lần lặp lại lệnh (1-5).\n"
        "Tham số `duration_ms` (mặc định 0): thời gian di chuyển liên tục bằng mili-giây (100-10000). Đặt > 0 để đi mượt mà không khựng.",
        PropertyList({
            Property("command",     kPropertyTypeString,  std::string("forward")),
            Property("repeat",      kPropertyTypeInteger, 1, 1, 5),
            Property("duration_ms", kPropertyTypeInteger, 0, 0, 10000)
        }),
        [](const PropertyList& props) -> ReturnValue {
            std::string cmd_str = props["command"].value<std::string>();
            int repeat          = props["repeat"].value<int>();
            int duration_ms     = props["duration_ms"].value<int>();

            dog_cmd_t cmd;

            if      (cmd_str == "forward")  cmd = CMD_FORWARD;
            else if (cmd_str == "backward") cmd = CMD_BACKWARD;
            else if (cmd_str == "left")     cmd = CMD_LEFT;
            else if (cmd_str == "right")    cmd = CMD_RIGHT;
            else if (cmd_str == "music")    cmd = CMD_MUSIC;
            else if (cmd_str == "trang_thai_1") cmd = CMD_STEP_FORWARD;
            else if (cmd_str == "trang_thai_2") cmd = CMD_STEP_LEFT;
            else if (cmd_str == "trang_thai_3") cmd = CMD_STEP_BACKWARD;
            else if (cmd_str == "trang_thai_4") cmd = CMD_STEP_RIGHT;
            else if (cmd_str == "toggle")       cmd = CMD_TOGGLE;
            else if (cmd_str == "stretch")      cmd = CMD_STRETCH;
            else if (cmd_str == "halt")         cmd = CMD_HALT;
            else {
                return std::string("Loi: lenh khong hop le '") + cmd_str +
                       "'. Su dung: forward, backward, left, right, music, trang_thai_1, trang_thai_2, trang_thai_3, trang_thai_4, toggle, stretch, halt.";
            }

            // Dừng mọi task IR chạy ngầm đang hoạt động (cả di chuyển liên tục và nhảy múa)
            stop_all_active_ir_tasks();

            // Nếu sử dụng thời gian di chuyển liên tục
            if (duration_ms > 0) {
                s_current_continuous_cmd = cmd;
                s_continuous_duration_ms = duration_ms;
                s_is_continuous_active = true;
                xTaskCreate(ir_continuous_tx_task, "ir_continuous", 2048, NULL, 10, &s_ir_continuous_task_handle);

                return std::string("Da kich hoat phat IR lien tuc: ") + cmd_str +
                       " trong " + std::to_string(duration_ms) + " ms";
            }

            // Chế độ phát lặp lại thông thường (giật cục)
            for (int i = 0; i < repeat; i++) {
                send_ir_command(cmd);
                if (i < repeat - 1) {
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
            }
            return std::string("Da phat lenh IR: ") + cmd_str +
                   " x" + std::to_string(repeat);
        }
    );

    ESP_LOGI(TAG, "Dang ky MCP tool 'self.robot.move' thanh cong.");

    mcp.AddTool(
        "self.robot.perform",
        "Thực hiện chuỗi hành động biểu diễn nhảy múa tự động của robot dog. "
        "Dùng khi người dùng yêu cầu robot biểu diễn, nhảy múa, diễn xiếc hoặc hỏi robot có thể làm được những gì.",
        PropertyList(),
        [](const PropertyList& props) -> ReturnValue {
            // Dừng các task IR đang chạy ngầm trước khi bắt đầu biểu diễn mới
            stop_all_active_ir_tasks();

            s_is_dance_active = true;
            xTaskCreate(ir_dance_tx_task, "ir_dance", 4096, NULL, 10, &s_ir_dance_task_handle);

            return std::string("Da bat dau chuoi bieu dien nhay mua cua robot.");
        }
    );

    ESP_LOGI(TAG, "Dang ky MCP tool 'self.robot.perform' thanh cong.");

    xTaskCreate(separator_task, "sep_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "[TIP] Nhan phim SPACE trong Serial Monitor de in duong phan cach log.");

#ifdef IR_RX_GPIO
    ESP_LOGI(TAG, "Kiem tra IR_RX_GPIO: %d", IR_RX_GPIO);
    if (IR_RX_GPIO != GPIO_NUM_NC) {
        s_rx_queue = xQueueCreate(200, sizeof(pulse_t));
        if (s_rx_queue != nullptr) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_ANYEDGE;
            io_conf.pin_bit_mask = (1ULL << IR_RX_GPIO);
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            esp_err_t conf_err = gpio_config(&io_conf);
            ESP_LOGI(TAG, "gpio_config cho GPIO%d tra ve: %s", IR_RX_GPIO, esp_err_to_name(conf_err));

            esp_err_t isr_service_err = gpio_install_isr_service(0);
            ESP_LOGI(TAG, "gpio_install_isr_service tra ve: %s", esp_err_to_name(isr_service_err));

            esp_err_t handler_err = gpio_isr_handler_add(IR_RX_GPIO, ir_rx_gpio_isr_handler, (void*)IR_RX_GPIO);
            ESP_LOGI(TAG, "gpio_isr_handler_add cho GPIO%d tra ve: %s", IR_RX_GPIO, esp_err_to_name(handler_err));

            BaseType_t task_created = xTaskCreate(ir_rx_task, "ir_rx_task", 4096, NULL, 10, NULL);
            ESP_LOGI(TAG, "xTaskCreate ir_rx_task tra ve: %d", (int)task_created);
            ESP_LOGI(TAG, "IR RX debug receiver khoi tao hoan tat tren GPIO%d", IR_RX_GPIO);
        } else {
            ESP_LOGE(TAG, "Khong the tao s_rx_queue!");
        }
    } else {
        ESP_LOGI(TAG, "IR_RX_GPIO la GPIO_NUM_NC, khong khoi tao RX.");
    }
#else
    ESP_LOGI(TAG, "IR_RX_GPIO chua duoc define!");
#endif
}

# Hướng Dẫn Thêm Chủ Đề & Bộ Biểu Cảm (Theme & Emojis) Tùy Chỉnh Cho Xiaozhi

Tài liệu này hướng dẫn chi tiết quy trình từng bước dành cho lập trình viên để tạo ra một Chủ đề giao diện (Theme) màn hình mới cùng với Bộ sưu tập 21 hình ảnh biểu cảm (Emoji) trong suốt độc quyền của riêng mình trên thiết bị **Xiaozhi ESP32**.

---

## 🎯 1. Chuẩn Bị Tài Nguyên Hình Ảnh

Xiaozhi hỗ trợ hiển thị tối đa 21 trạng thái biểu cảm chuẩn. Bạn cần chuẩn bị đúng 21 tệp ảnh định dạng **PNG** với tên viết thường không dấu như sau:
`angry`, `confident`, `confused`, `cool`, `crying`, `delicious`, `embarrassed`, `funny`, `happy`, `kissy`, `laughing`, `loving`, `neutral`, `relaxed`, `sad`, `shocked`, `silly`, `sleepy`, `surprised`, `thinking`, `winking`.

### ⚠️ Quy tắc tối ưu ảnh:
*   **Tách nền (Trong suốt):** Nên dùng công cụ tách nền (như `rembg`) để loại bỏ nền trắng/màu của ảnh, giúp biểu cảm nổi bật trên nền LCD đen của thiết bị.
*   **Độ phân giải chuẩn:** Ảnh gốc của bạn có thể ở bất kỳ độ phân giải nào (512x512, 1024x1024), nhưng script build sẽ tự động crop và resize về độ phân giải hiển thị chuẩn là **`200x200` pixels** để đảm bảo ảnh nằm chính giữa màn hình, sắc nét và không bị méo.

---

## 🛠️ 2. Đăng Ký Theme Mới Trong Mã Nguồn C++

Mở tệp: **[main/display/lcd_display.cc](file:///d:/project/xiaozhi-esp32-main/main/display/lcd_display.cc)**

Tìm đến hàm `InitializeLcdThemes()` và định nghĩa màu sắc bong bóng chat, phông chữ cho giao diện mới, sau đó đăng ký vào hệ thống:

```cpp
// Khai báo theme mới (ví dụ đặt tên là: "my_theme")
auto my_theme = new LvglTheme("my_theme");
my_theme->set_background_color(lv_color_hex(0x000000));
my_theme->set_text_color(lv_color_hex(0xFFFFFF));
my_theme->set_chat_background_color(lv_color_hex(0x1F1F1F));
my_theme->set_user_bubble_color(lv_color_hex(0x00FF00));      // Bong bóng chat người dùng
my_theme->set_assistant_bubble_color(lv_color_hex(0x222222)); // Bong bóng chat AI
my_theme->set_system_bubble_color(lv_color_hex(0x000000));
my_theme->set_system_text_color(lv_color_hex(0xFFFFFF));
my_theme->set_border_color(lv_color_hex(0xFFFFFF));
my_theme->set_low_battery_color(lv_color_hex(0xFF0000));
my_theme->set_text_font(text_font);
my_theme->set_icon_font(icon_font);
my_theme->set_large_icon_font(large_icon_font);

// Đăng ký theme vào bộ quản lý LvglThemeManager
auto& theme_manager = LvglThemeManager::GetInstance();
theme_manager.RegisterTheme("my_theme", my_theme);
```

---

## 📦 3. Cấu Hình Nạp Bộ Emoji Vào Theme

Mở tệp: **[main/assets.cc](file:///d:/project/xiaozhi-esp32-main/main/assets.cc)**

1.  Tại vị trí load fonts và gán theme mặc định, khai báo font chữ và bộ ảnh fallback mặc định cho theme mới:
    ```cpp
    auto my_theme = theme_manager.GetTheme("my_theme");
    if (my_theme != nullptr) {
        my_theme->set_text_font(text_font);
        if (my_theme->emoji_collection() == nullptr) {
            my_theme->set_emoji_collection(custom_emoji_collection); // Dùng mặc định nếu lỗi load assets
        }
    }
    ```

2.  Viết code giải mã JSON index của tệp `assets.bin` để nạp các asset ảnh của bộ emoji độc quyền:
    ```cpp
    cJSON* my_theme_emoji_collection = cJSON_GetObjectItem(root, "my_theme_emoji_collection");
    if (cJSON_IsArray(my_theme_emoji_collection)) {
        auto custom_emoji = std::make_shared<EmojiCollection>();
        int count = cJSON_GetArraySize(my_theme_emoji_collection);
        for (int i = 0; i < count; i++) {
            cJSON* emoji = cJSON_GetArrayItem(my_theme_emoji_collection, i);
            if (cJSON_IsObject(emoji)) {
                cJSON* name = cJSON_GetObjectItem(emoji, "name");
                cJSON* file = cJSON_GetObjectItem(emoji, "file");
                if (cJSON_IsString(name) && cJSON_IsString(file)) {
                    if (assets->GetAssetData(file->valuestring, ptr, size)) {
                        custom_emoji->AddEmoji(name->valuestring, new LvglRawImage(ptr, size));
                    }
                }
            }
        }
        if (my_theme != nullptr) {
            my_theme->set_emoji_collection(custom_emoji);
        }
    }
    ```

---

## 🐍 4. Cập Nhật Script Đóng Gói Tài Nguyên (Python)

Mở tệp: **[scripts/build_default_assets.py](file:///d:/project/xiaozhi-esp32-main/scripts/build_default_assets.py)**

1.  Đăng ký thêm tham số dòng lệnh nhận đường dẫn thư mục ảnh mới:
    ```python
    parser.add_argument('--my_theme_emoji_collection', help='Đường dẫn tới thư mục ảnh my_theme')
    ```
2.  Trong hàm xử lý chính, thêm logic nén và xuất metadata JSON tương ứng:
    ```python
    my_theme_emoji_collection = None
    if my_theme_emoji_collection_path:
        my_theme_emoji_collection = process_custom_emoji_collection(
            my_theme_emoji_collection_path, 
            assets_dir, 
            "my_theme", 
            target_size
        )
    ```

---

## 🛠️ 5. Liên Kết Đường Dẫn Trong Hệ Thống Build CMake

Mở tệp: **[main/CMakeLists.txt](file:///d:/project/xiaozhi-esp32-main/main/CMakeLists.txt)**

Thêm logic tự động kiểm tra thư mục ảnh trên máy chủ biên dịch để truyền đối số vào script Python:
```cmake
if(EXISTS "C:/Users/ngoma/Downloads/my_theme")
    list(APPEND BUILD_ARGS "--my_theme_emoji_collection" "C:/Users/ngoma/Downloads/my_theme")
endif()
```

---

## 🗣️ 6. Đăng Ký Tên Theme Cho Mô Hình AI

Mở tệp: **[main/mcp_server.cc](file:///d:/project/xiaozhi-esp32-main/main/mcp_server.cc)**

Tìm đến phần đăng ký công cụ `self.screen.set_theme` và cập nhật danh sách các theme được hỗ trợ trong chuỗi mô tả (description) để mô hình ngôn ngữ lớn (LLM) biết thiết bị có giao diện này:
```cpp
// Thêm "my_theme" vào danh sách mô tả để AI nhận diện đổi theme bằng giọng nói
```

---

## 🚀 7. Biên Dịch Và Đóng Gói Lại

Mỗi khi thay đổi hoặc thêm ảnh biểu cảm mới, bạn cần xóa file build assets cũ để CMake chạy lại script Python đóng gói:

```powershell
# Xóa file assets cũ
Remove-Item -Path d:\project\xiaozhi-esp32-main\build\generated_assets.bin -ErrorAction SilentlyContinue

# Biên dịch và nạp chương trình lên ESP32
idf.py build
idf.py flash
```

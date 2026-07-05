# Chatbot Trợ Lý Ảo Sử Dụng Giao Thức MCP

(Tiếng Việt | [English](README_en.md) | [中文](README_zh.md) | [日本語](README_ja.md))

## Giới thiệu

👉 [Video demo: Cho AI xem camera vs phản ứng của AI khi phát hiện chủ nhân 3 ngày chưa gội đầu【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [Video hướng dẫn chế tạo bạn gái ảo AI cho người mới bắt đầu【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

Là một thiết bị tương tác giọng nói đầu cuối, chatbot AI XiaoZhi (Tiểu Trí) tận dụng sức mạnh trí tuệ nhân tạo của các mô hình ngôn ngữ lớn như Qwen / DeepSeek, đồng thời đạt được khả năng điều khiển đa thiết bị thông qua giao thức MCP (Model Context Protocol).

<img src="docs/mcp-based-graph.jpg" alt="Điều khiển mọi thứ qua MCP" width="320">

## Lưu ý về Phiên bản

Phiên bản v2 hiện tại không tương thích với bảng phân vùng (partition table) của phiên bản v1, do đó không thể nâng cấp từ v1 lên v2 thông qua OTA. Để biết chi tiết về bảng phân vùng, vui lòng xem [partitions/v2/README.md](partitions/v2/README.md).

Tất cả các phần cứng đang chạy v1 đều có thể nâng cấp lên v2 bằng cách nạp (flash) firmware thủ công.

Phiên bản ổn định của v1 là 1.9.2. Bạn có thể chuyển sang v1 bằng lệnh `git checkout v1`. Nhánh v1 sẽ được duy trì hỗ trợ cho đến tháng 2 năm 2026.

### Các tính năng đã triển khai

- Kết nối Wi-Fi / ML307 Cat.1 4G
- Đánh thức bằng giọng nói ngoại tuyến [ESP-SR](https://github.com/espressif/esp-sr)
- Hỗ trợ hai giao thức truyền thông ([Websocket](docs/websocket.md) hoặc MQTT + UDP)
- Sử dụng bộ mã hóa âm thanh OPUS (OPUS audio codec)
- Tương tác giọng nói dựa trên kiến trúc luồng (streaming) ASR + LLM + TTS
- Nhận diện người nói, xác định ai đang nói [3D Speaker](https://github.com/modelscope/3D-Speaker)
- Màn hình OLED / LCD, hỗ trợ hiển thị biểu cảm (emoji) sinh động
- Hiển thị phần trăm pin và quản lý nguồn điện tiêu thụ
- Hỗ trợ đa ngôn ngữ (Tiếng Trung, Tiếng Anh, Tiếng Nhật)
- Hỗ trợ các nền tảng chip ESP32-C3, ESP32-S3, ESP32-P4 của Espressif
- Giao thức MCP phía thiết bị để điều khiển phần cứng (Loa, LED, Động cơ Servo, GPIO, v.v.)
- Giao thức MCP phía máy chủ để mở rộng năng lượng của mô hình AI (điều khiển nhà thông minh, vận hành máy tính để bàn PC, tìm kiếm kiến thức, gửi email, v.v.)
- Cho phép tùy chỉnh từ khóa đánh thức, phông chữ, biểu cảm và hình nền chat thông qua công cụ chỉnh sửa trực tuyến trên web ([Custom Assets Generator](https://github.com/78/xiaozhi-assets-generator))
- **Bộ điều khiển Robot đồ chơi hồng ngoại tùy chỉnh**: Tích hợp bộ phát hồng ngoại (RMT 38kHz) và mắt thu (VS1838B) để điều khiển robot cún di chuyển bằng giọng nói thông qua AI (xem chi tiết tại [README_IR_CONTROL.md](README_IR_CONTROL.md))

---

## Phần cứng

### Thực hành tự làm (DIY) trên Breadboard

Xem tài liệu hướng dẫn chi tiết trên Feishu:

👉 ["Bách khoa toàn thư về Chatbot AI XiaoZhi"](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

Demo lắp ráp trên breadboard:

![Breadboard Demo](docs/v1/wiring2.jpg)

### Hỗ trợ hơn 70 phần cứng nguồn mở (Danh sách một phần)

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="LiChuang ESP32-S3 Development Board">Bảng mạch phát triển LiChuang ESP32-S3</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="Espressif ESP32-S3-BOX3">Espressif ESP32-S3-BOX3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">M5Stack AtomS3R + Echo Base</a>
- <a href="https://gf.bilibili.com/item/detail/1108782064" target="_blank" title="Magic Button 2.4">Magic Button 2.4</a>
- <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">Waveshare ESP32-S3-Touch-AMOLED-1.8</a>
- <a href="https://github.com/Xinyuan-LilyGO/T-Circle-S3" target="_blank" title="LILYGO T-Circle-S3">LILYGO T-Circle-S3</a>
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="XiaGe Mini C3">XiaGe Mini C3</a>
- <a href="https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y" target="_blank" title="Movecall CuiCan ESP32S3">Mặt dây chuyền CuiCan AI</a>
- <a href="https://github.com/WMnologo/xingzhi-ai" target="_blank" title="WMnologo-Xingzhi-1.54">WMnologo-Xingzhi-1.54TFT</a>
- <a href="https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html" target="_blank" title="SenseCAP Watcher">SenseCAP Watcher</a>
- <a href="https://www.bilibili.com/video/BV1BHJtz6E2S/" target="_blank" title="ESP-HI Low Cost Robot Dog">Robot Dog giá rẻ ESP-HI</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

---

## Phần mềm

### Nạp Firmware nhanh

Đối với người mới bắt đầu, khuyến nghị sử dụng các bản build firmware được biên dịch sẵn để nạp trực tiếp mà không cần thiết lập môi trường phát triển code.

Mặc định, firmware sẽ kết nối đến máy chủ chính thức [xiaozhi.me](https://xiaozhi.me). Người dùng cá nhân có thể đăng ký tài khoản để sử dụng miễn phí mô hình AI Qwen thời gian thực.

👉 [Hướng dẫn nạp firmware cho người mới bắt đầu (Tiếng Trung)](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### Môi trường phát triển

- Phần mềm soạn thảo: Cursor hoặc VSCode
- Cài đặt extension **ESP-IDF**, chọn phiên bản SDK 5.4 trở lên
- Hệ điều hành khuyến nghị: Linux biên dịch sẽ nhanh hơn và ít gặp lỗi driver hơn Windows
- Dự án này tuân thủ phong cách lập trình C++ của Google (Google C++ code style), vui lòng đảm bảo tuân thủ khi gửi các thay đổi mã nguồn.

### Tài liệu dành cho lập trình viên

- [Hướng dẫn thiết lập Board tùy chỉnh](docs/custom-board.md) - Học cách khai báo chân phần cứng cho XiaoZhi AI
- [Hướng dẫn thêm chủ đề & biểu cảm tùy chỉnh](docs/custom-emoji-theme.md) - Học cách thiết kế giao diện LCD và nạp bộ emoji độc quyền
- [Hướng dẫn điều khiển IoT bằng giao thức MCP](docs/mcp-usage.md) - Cách điều khiển thiết bị ngoại vi qua giao thức MCP
- [Luồng tương tác của giao thức MCP](docs/mcp-protocol.md) - Triển khai giao thức MCP phía thiết bị (ESP32)
- [Tài liệu giao thức truyền thông hỗn hợp MQTT + UDP](docs/mqtt-udp.md)
- [Tài liệu chi tiết về giao thức truyền thông WebSocket](docs/websocket.md)
- [Hướng dẫn điều khiển Robot bằng hồng ngoại (IR)](README_IR_CONTROL.md) - Chi tiết sơ đồ đấu nối và giao thức phát hồng ngoại 38kHz điều khiển cún robot

---

## Cấu hình Mô hình Lớn (LLM)

Nếu bạn đã sở hữu thiết bị chatbot AI XiaoZhi và kết nối thành công với máy chủ chính thức, bạn có thể đăng nhập vào trang điều khiển [xiaozhi.me](https://xiaozhi.me) để quản lý cấu hình hệ thống.

👉 [Video hướng dẫn vận hành trang cấu hình (Giao diện cũ)](https://www.bilibili.com/video/BV1jUCUY2EKM/)

---

## Các dự án nguồn mở liên quan

Để tự triển khai server xử lý AI trên máy tính cá nhân của riêng bạn, vui lòng tham khảo các dự án nguồn mở sau:

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) Server viết bằng Python
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) Server viết bằng Java
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) Server viết bằng Golang
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) Server viết bằng Golang

Các dự án client khác sử dụng chung giao thức truyền thông của XiaoZhi:

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) Client chạy trên Python
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) Client chạy trên Android
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) Client chạy trên Linux của đội ngũ 100ask
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) Firmware chip Bluetooth của hãng Tứ Xuyên
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) Firmware QuecPython của hãng Quectel

Bộ công cụ tạo file Assets tùy chỉnh:

- [78/xiaozhi-assets-generator](https://github.com/78/xiaozhi-assets-generator) Bộ tạo từ khóa đánh thức, phông chữ, biểu cảm và hình nền tùy chỉnh

---

## Về dự án này

Đây là một dự án nguồn mở viết cho ESP32, được phát hành dưới giấy phép **MIT license**, cho phép bất kỳ ai cũng có thể sử dụng hoàn toàn miễn phí, bao gồm cả mục đích thương mại.

Chúng tôi hy vọng dự án này sẽ giúp ích cho mọi người trong việc tìm hiểu phát triển phần cứng AI và áp dụng nhanh chóng các mô hình ngôn ngữ lớn đang phát triển như vũ bão vào các thiết bị vật lý thực tế.

Nếu có bất kỳ ý tưởng hoặc đề xuất đóng góp nào, vui lòng mở Issues hoặc tham gia nhóm [Discord](https://discord.gg/C759fGMBcZ) hoặc QQ group: 994694848
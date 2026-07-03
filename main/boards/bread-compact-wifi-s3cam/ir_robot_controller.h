#pragma once

/**
 * IR Robot Dog Controller - MCP Tool
 *
 * Phát lệnh IR hồng ngoại điều khiển robot dog qua giọng nói.
 * Giao thức: 7-bit command + 9 lần lặp lại, 38kHz carrier.
 *
 * Kết nối phần cứng:
 *   - IR TX LED: GPIO_NUM_3
 *   - Carrier: 38kHz, duty 33%
 */

#ifndef IR_ROBOT_CONTROLLER_H
#define IR_ROBOT_CONTROLLER_H

/**
 * Khởi tạo IR Robot Controller và đăng ký các MCP tool:
 *   - self.robot.move  (forward / backward / left / right / stop)
 *
 * Gọi hàm này trong constructor của Board sau khi khởi tạo các thành phần khác.
 *
 * @param ir_tx_gpio  Số GPIO của đèn LED IR phát (mặc định: GPIO_NUM_3)
 */
void InitializeIrRobotController(int ir_tx_gpio = 3);

#endif // IR_ROBOT_CONTROLLER_H

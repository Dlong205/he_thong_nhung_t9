// SV1 - I2C1 master polling, register-level. PCLK1=36MHz -> 400kHz Fast.
#include "i2c_reg.h"
#include "stm32f1xx_hal.h"
#include "usart_reg.h"

#define I2C_TIMEOUT 20000u

volatile uint8_t i2c_fail_at = 0;
volatile uint16_t i2c_fail_idx = 0;
volatile uint32_t i2c_sr1_last = 0;
volatile uint32_t i2c_sr2_last = 0;

static void snap_regs(uint8_t code) {
  i2c_fail_at = code;
  i2c_sr1_last = I2C1->SR1;
  i2c_sr2_last = I2C1->SR2;
}

// --- Xử lý lỗi I2C theo RM0008 (bài học bring-up thật) ---
// F1: cờ AF/BERR/ARLO/OVR được xóa bằng cách GHI 0 vào bit đó (HAL làm y hệt:
// SR1 = ~(FLAG & 0xFFFF)). Không xóa thì sau 1 lần NACK mọi phiên sau đều kẹt.
static void i2c_clear_errs(void) {
  I2C1->SR1 = (uint32_t)~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR);
}

static bool bus_lines_idle(void) {
  return (GPIOB->IDR & (1u << 6)) && (GPIOB->IDR & (1u << 7)); // SCL + SDA đều cao
}

// BUSY kẹt nhưng dây đã rảnh cao: reset state machine bằng PE=0→1 (thanh ghi
// CCR/TRISE/CR2 giữ nguyên). Nếu dây thấp thật thì để recover() bit-bang lo.
static void i2c_fix_busy_stuck(void) {
  if ((I2C1->SR2 & I2C_SR2_BUSY) && bus_lines_idle()) {
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR1 |= I2C_CR1_PE;
  }
}
static bool wait_flag(volatile uint32_t *reg, uint32_t flag, bool set) {
  for (uint32_t i = 0; i < I2C_TIMEOUT; i++) {
    bool has = ((*reg) & flag) != 0;
    if (has == set) return true;
  }
  return false;
}

void i2c1_init_400k(void) {
  // 1. Clock: GPIOB + AFIO (APB2), I2C1 (APB1)
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
  RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
  // 2. PB6/PB7: Alternate Open-Drain 50MHz (MODE=11 CNF=11)
  GPIOB->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 | GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
  GPIOB->CRL |= GPIO_CRL_MODE6 | GPIO_CRL_CNF6 | GPIO_CRL_MODE7 | GPIO_CRL_CNF7;
  GPIOB->ODR |= GPIO_ODR_ODR6 | GPIO_ODR_ODR7; // thả bus cao (pull-up ngoài 4k7)
  // 3. I2C timing: PE=0 trước khi cấu hình
  I2C1->CR1 &= ~I2C_CR1_PE;
  // CR2.FREQ = PCLK1 MHz
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  uint32_t freq = pclk1 / 1000000u; // 36
  I2C1->CR2 = (I2C1->CR2 & ~I2C_CR2_FREQ) | (freq & 0x3F);
  // CCR Fast-mode DUTY=0: CCR = PCLK1/(3*Fscl) = 36e6/1.2e6 = 30
  I2C1->CCR = I2C_CCR_FS | 30u;
  // TRISE Fast (300ns): FREQ*300ns + 1 = 36*0.3+1 ≈ 12
  I2C1->TRISE = 12u;
  // 4. Enable: PE + ACK
  I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
}

static void hw_reset(void) {
  // Reset cứng peripheral khi SB không set (trạng thái rối nội): RSTR + cấu hình lại.
  I2C1->CR1 &= ~I2C_CR1_PE;
  RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
  RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
  uint32_t freq = HAL_RCC_GetPCLK1Freq() / 1000000u;
  I2C1->CR2 = (I2C1->CR2 & ~I2C_CR2_FREQ) | (freq & 0x3F);
  I2C1->CCR = I2C_CCR_FS | 30u;
  I2C1->TRISE = 12u;
  I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;
}

static bool start_addr(uint8_t dev7, bool read) {
  i2c_clear_errs();        // dọn NACK/BERR cũ trước mỗi phiên
  i2c_fix_busy_stuck();    // gỡ BUSY giả nếu state machine kẹt
  I2C1->CR1 |= I2C_CR1_START;                       // phát START
  if (!wait_flag(&I2C1->SR1, I2C_SR1_SB, true)) {
    // Chụp trạng thái THẬT trước khi recover để debug (không bị reset xóa)
    i2c_sr1_last = I2C1->SR1;
    i2c_sr2_last = I2C1->SR2;
    // Dây bị giữ thấp → bit-bang cứu; nếu dây rảnh → reset cứng (throttle 20ms)
    static uint32_t last_rst = 0;
    uint32_t now = HAL_GetTick();
    if (!bus_lines_idle()) {
      i2c1_recover();
    } else if (now - last_rst > 20) {
      last_rst = now;
      hw_reset();
    }
    return false;
  }
  (void)I2C1->SR1;
  I2C1->DR = (uint8_t)((dev7 << 1) | (read ? 1u : 0u)); // ADDR+R/W
  if (!wait_flag(&I2C1->SR1, I2C_SR1_ADDR, true)) return false;
  (void)I2C1->SR1; (void)I2C1->SR2;                 // xóa ADDR
  return true;
}

bool i2c1_write_reg(uint8_t dev7, uint8_t reg, uint8_t val) {
  if (!start_addr(dev7, false)) { i2c_fail_at = 11; goto fail; }
  if (!wait_flag(&I2C1->SR1, I2C_SR1_TXE, true)) { i2c_fail_at = 13; goto fail; }
  I2C1->DR = reg;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_TXE, true)) { i2c_fail_at = 14; goto fail; }
  I2C1->DR = val;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_BTF, true)) { i2c_fail_at = 15; goto fail; }
  I2C1->CR1 |= I2C_CR1_STOP;
  return true;
fail:
  I2C1->CR1 |= I2C_CR1_STOP;
  return false;
}

bool i2c1_read_reg(uint8_t dev7, uint8_t reg, uint8_t *val) {
  if (!start_addr(dev7, false)) { i2c_fail_at = 21; goto fail; }
  if (!wait_flag(&I2C1->SR1, I2C_SR1_TXE, true)) { i2c_fail_at = 22; goto fail; }
  I2C1->DR = reg;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_TXE, true)) { i2c_fail_at = 23; goto fail; }
  // Repeated START rồi đọc 1 byte: NACK + STOP trước khi đọc
  I2C1->CR1 |= I2C_CR1_START;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_SB, true)) { i2c_fail_at = 24; goto fail; }
  (void)I2C1->SR1;
  I2C1->DR = (uint8_t)((dev7 << 1) | 1u);
  if (!wait_flag(&I2C1->SR1, I2C_SR1_ADDR, true)) { i2c_fail_at = 25; goto fail; }
  I2C1->CR1 &= ~I2C_CR1_ACK;                        // NACK byte cuối
  (void)I2C1->SR1; (void)I2C1->SR2;
  I2C1->CR1 |= I2C_CR1_STOP;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_RXNE, true)) { i2c_fail_at = 26; goto fail; }
  *val = (uint8_t)I2C1->DR;
  I2C1->CR1 |= I2C_CR1_ACK;
  return true;
fail:
  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->CR1 |= I2C_CR1_ACK;
  return false;
}

bool i2c1_burst_read(uint8_t dev7, uint8_t start_reg, uint8_t *buf, uint16_t len) {
  if (len == 0 || buf == 0) return false;
  i2c_fail_idx = 0xFFFF;
  if (!start_addr(dev7, false)) { snap_regs(31); goto fail; }
  if (!wait_flag(&I2C1->SR1, I2C_SR1_TXE, true)) { snap_regs(32); goto fail; }
  I2C1->DR = start_reg;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_BTF, true)) { snap_regs(33); goto fail; }
  // Repeated START + ADDR+R
  I2C1->CR1 |= I2C_CR1_START;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_SB, true)) { snap_regs(34); goto fail; }
  (void)I2C1->SR1;
  I2C1->DR = (uint8_t)((dev7 << 1) | 1u);
  if (!wait_flag(&I2C1->SR1, I2C_SR1_ADDR, true)) { snap_regs(35); goto fail; }
  if (len == 1) {
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR1; (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;
    if (!wait_flag(&I2C1->SR1, I2C_SR1_RXNE, true)) { snap_regs(36); goto fail; }
    buf[0] = (uint8_t)I2C1->DR;
  } else if (len == 2) {
    // Theo HAL F1 (không dùng POS): chờ BTF, STOP, đọc 2 byte
    (void)I2C1->SR1; (void)I2C1->SR2;
    if (!wait_flag(&I2C1->SR1, I2C_SR1_BTF, true)) { snap_regs(37); goto fail; }
    __disable_irq();
    I2C1->CR1 |= I2C_CR1_STOP;
    buf[0] = (uint8_t)I2C1->DR;
    __enable_irq();
    buf[1] = (uint8_t)I2C1->DR;
  } else {
    (void)I2C1->SR1; (void)I2C1->SR2;               // xóa ADDR, ACK các byte đầu
    I2C1->CR1 |= I2C_CR1_ACK;
    for (uint16_t i = 0; i < len; i++) {
      if (i == len - 3) {
        // 3 byte cuối theo EV6_3 (RM0008 Fig.276 / HAL F1): BẮT BUỘC
        // chờ BTF lần 2 TRƯỚC khi STOP, nếu STOP sớm bus sẽ kẹt.
        if (!wait_flag(&I2C1->SR1, I2C_SR1_BTF, true)) { i2c_fail_idx = i; snap_regs(39); goto fail; }
        I2C1->CR1 &= ~I2C_CR1_ACK;
        __disable_irq();
        buf[i] = (uint8_t)I2C1->DR;                 // byte N-3
        if (!wait_flag(&I2C1->SR1, I2C_SR1_BTF, true)) {
          __enable_irq(); i2c_fail_idx = i + 1; snap_regs(40); goto fail;
        }
        I2C1->CR1 |= I2C_CR1_STOP;                  // STOP đúng thời điểm
        buf[i + 1] = (uint8_t)I2C1->DR;             // byte N-2
        __enable_irq();
        buf[i + 2] = (uint8_t)I2C1->DR;             // byte N-1
        break;
      }
      if (!wait_flag(&I2C1->SR1, I2C_SR1_RXNE, true)) { i2c_fail_idx = i; snap_regs(38); goto fail; }
      buf[i] = (uint8_t)I2C1->DR;
    }
    I2C1->CR1 |= I2C_CR1_ACK;
  }
  return true;
fail:
  I2C1->CR1 |= I2C_CR1_STOP;
  I2C1->CR1 &= ~I2C_CR1_POS;
  I2C1->CR1 |= I2C_CR1_ACK;
  return false;
}

bool i2c1_write_buf(uint8_t dev7, const uint8_t *buf, uint16_t len) {  if (len == 0 || buf == 0) return false;
  if (!start_addr(dev7, false)) goto fail;
  for (uint16_t i = 0; i < len; i++) {
    if (!wait_flag(&I2C1->SR1, I2C_SR1_TXE, true)) goto fail;
    I2C1->DR = buf[i];
  }
  if (!wait_flag(&I2C1->SR1, I2C_SR1_BTF, true)) goto fail;
  I2C1->CR1 |= I2C_CR1_STOP;
  return true;
fail:
  I2C1->CR1 |= I2C_CR1_STOP;
  return false;
}

bool i2c1_probe(uint8_t dev7) {
  i2c_clear_errs();
  i2c_fix_busy_stuck();
  I2C1->CR1 |= I2C_CR1_START;
  if (!wait_flag(&I2C1->SR1, I2C_SR1_SB, true)) { I2C1->CR1 |= I2C_CR1_STOP; return false; }
  (void)I2C1->SR1;
  I2C1->DR = (uint8_t)(dev7 << 1); // +W
  bool ack = wait_flag(&I2C1->SR1, I2C_SR1_ADDR, true);
  (void)I2C1->SR1; (void)I2C1->SR2;
  I2C1->CR1 |= I2C_CR1_STOP;
  // Đợi bus rảnh trước lần probe sau (tránh dính BUSY khi slave giữ clock)
  for (uint32_t i = 0; i < 5000; i++) {
    if (!(I2C1->SR2 & I2C_SR2_BUSY)) break;
  }
  return ack;
}

void i2c1_recover(void) {
  // Chuẩn I2C bus-clear đầy đủ: 18 xung SCL + STOP để slave kẹt giữa byte
  // (MPU6500 hay latch khi MCU reset giữa transaction) nhả về idle.
  I2C1->CR1 &= ~I2C_CR1_PE;
  // PB6 SCL + PB7 SDA: open-drain output 50MHz (MODE=11 CNF=01)
  GPIOB->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 | GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
  GPIOB->CRL |= (GPIO_CRL_MODE6 | GPIO_CRL_CNF6_0) | (GPIO_CRL_MODE7 | GPIO_CRL_CNF7_0);
  GPIOB->ODR |= (1u << 6) | (1u << 7); // thả 2 dây cao trước
  for (volatile int d = 0; d < 1000; d++);
  // ≥9 xung SCL (làm 18) cho slave nhả SDA
  for (int i = 0; i < 18; i++) {
    GPIOB->ODR &= ~(1u << 6); // SCL low
    for (volatile int d = 0; d < 200; d++);
    GPIOB->ODR |= (1u << 6);  // SCL high
    for (volatile int d = 0; d < 200; d++);
  }
  // STOP: SDA thấp -> SDA cao trong khi SCL cao (đưa slave về idle state)
  GPIOB->ODR &= ~(1u << 7);
  for (volatile int d = 0; d < 200; d++);
  GPIOB->ODR |= (1u << 7);
  for (volatile int d = 0; d < 200; d++);
  // Trả pin về AF-OD + cấu hình lại peripheral
  i2c1_init_400k();
}

void i2c1_dump_regs(void) {  usart1_printf("RCC_APB1ENR=0x%08lX I2C1_CR1=0x%08lX CR2=0x%08lX CCR=0x%08lX TRISE=0x%08lX\r\n",
                (unsigned long)RCC->APB1ENR,
                (unsigned long)I2C1->CR1, (unsigned long)I2C1->CR2,
                (unsigned long)I2C1->CCR, (unsigned long)I2C1->TRISE);
}

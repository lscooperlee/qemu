
/**
 * uart:                Base = 0xFF000000
 * RX/TX Reg:           offset = 0x0
 *                      Value = RX/TX data, 8bits
 * Baudrate Reg:        Offset = 0x4
 *                      Value = Baudrate
 * RX ready Reg:        Offset = 0x8
 *                      Value = 1 if a character received
 * TX ready Reg:        Offset = 0xc
 *                      Value = 1 if is ready for trasmit
 **/

#define LARM_UART_REG_BASE              0xFF000000
#define LARM_UART_OFFSET_RTX            0x0
#define LARM_UART_OFFSET_BAUDRATE       0x4 
#define LARM_UART_OFFSET_RX_READY       0x8 
#define LARM_UART_OFFSET_TX_READY       0xc 

#define LARM_UART_REG_RTX               (LARM_UART_REG_BASE + LARM_UART_OFFSET_RTX)
#define LARM_UART_REG_BAUDRATE          (LARM_UART_REG_BASE + LARM_UART_OFFSET_BAUDRATE) 
#define LARM_UART_REG_RX_READY          (LARM_UART_REG_BASE + LARM_UART_OFFSET_RX_READY) 
#define LARM_UART_REG_TX_READY          (LARM_UART_REG_BASE + LARM_UART_OFFSET_TX_READY) 


/**
 * timer:               Base = 0xFF010000
 * Timer enable Reg:    offset = 0x0
 *                      Value = 1 for start timer, 0 for stop
 * Freqency Reg:        Offset = 0x4
 *                      Value = frequency in HZ
 * Timer counter Reg:   offset = 0x8
 *                      Value = courrent counter
*/

#define LARM_TIMER_REG_BASE              0xFF010000
#define LARM_TIMER_OFFSET_ENABLE         0x0
#define LARM_TIMER_OFFSET_FREQ           0x4 
#define LARM_TIMER_OFFSET_COUNTER        0x8

#define LARM_TIMER_REG_ENABLE            (LARM_TIMER_REG_BASE + LARM_TIMER_OFFSET_ENABLE)
#define LARM_TIMER_REG_FREQ              (LARM_TIMER_REG_BASE + LARM_TIMER_OFFSET_FREQ) 
#define LARM_TIMER_REG_COUNTER           (LARM_TIMER_REG_BASE + LARM_TIMER_OFFSET_COUNTER) 


/**
 * interrupt:           Base = 0xFF0F0000
 * Irq Source Reg:      irqSource = 0x0
 *                      Value = Irq Source, write 1 to clear
 * Irq Enable Reg:      irqEnable = 0x4
 *                      Value = Irq Enable, write 1 to enable
 **/

#define LARM_INTERRUPT_REG_BASE              0xFF0F0000
#define LARM_INTERRUPT_OFFSET_SOURCE         0x0
#define LARM_INTERRUPT_OFFSET_ENABLE         0x4

#define LARM_INTERRUPT_REG_SOURCE            (LARM_INTERRUPT_REG_BASE + LARM_INTERRUPT_OFFSET_SOURCE)
#define LARM_INTERRUPT_REG_ENABLE            (LARM_INTERRUPT_REG_BASE + LARM_INTERRUPT_OFFSET_ENABLE)

#define LARM_INTERRUPT_UART                  (1 << 0)
#define LARM_INTERRUPT_TIMER                 (1 << 1)


static inline unsigned int larm_read32(unsigned long addr) {
    return *(unsigned int *)addr;
}

static inline void larm_write32(unsigned int value, unsigned long addr) {
    *(unsigned int *)addr = value;
}

static inline void larm_setbit32(unsigned int value, unsigned long addr) {
    unsigned int tmp = larm_read32(addr);
    larm_write32(tmp|value, addr);
}

static inline void larm_clearbit32(unsigned int value, unsigned long addr) {
    unsigned int tmp = larm_read32(addr);
    larm_write32(tmp&~value, addr);
}

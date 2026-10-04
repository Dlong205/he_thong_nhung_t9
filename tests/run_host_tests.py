"""Host syntax + pure-calculation checks only. NOT an STM32/I2C simulator.
Requires Python 3 + gcc. Fake CMSIS names permit compilation; bit values
are placeholders and no peripheral transactions are exercised.
"""
import re, subprocess, tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = '\n'.join((root / x).read_text() for x in ['main.c','mpu6500_i2c.c','mpu6500_i2c.h'])
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    header = '#ifndef TEST_CMSIS\n#define TEST_CMSIS\n#include <stdint.h>\n'
    for obj in ['TIM2','I2C1','GPIOB','RCC','AFIO','EXTI']:
        fields = sorted(set(re.findall(obj+r'->(\w+)',src)))
        header += 'typedef struct {' + ''.join('volatile uint32_t '+f+('[4]' if f=='EXTICR' else '')+';' for f in fields) + '} '+obj+'_stub;\n'
        header += 'extern '+obj+'_stub *'+obj+';\n'
    for macro in sorted(set(re.findall(r'\b(?:TIM|RCC|AFIO|EXTI|I2C)_[A-Z0-9_]+\b',src))):
        if macro != 'I2C_TIMEOUT_MAX': header += '#define '+macro+' 1u\n'
    header += '''extern uint32_t SystemCoreClock;
void SystemCoreClockUpdate(void);
#define EXTI0_IRQn 0
#define TIM2_IRQn 1
static inline uint32_t __get_PRIMASK(void) {return 0;}
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t m) {(void)m;}
static inline void NVIC_SetPriority(int i,int p) {(void)i;(void)p;}
static inline void NVIC_EnableIRQ(int i) {(void)i;}
#endif
'''
    (tmp/'stm32f10x.h').write_text(header)
    flags=['gcc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(tmp),'-I'+str(root)]
    subprocess.run(flags+['-fsyntax-only',str(root/'main.c'),str(root/'mpu6500_i2c.c')],check=True)
    subprocess.run(flags+['-ffunction-sections','-fdata-sections',str(root/'mpu6500_i2c.c'),str(root/'tests/test_math.c'),'-Wl,--gc-sections','-lm','-o',str(tmp/'tests')],check=True)
    subprocess.run([str(tmp/'tests')],check=True)

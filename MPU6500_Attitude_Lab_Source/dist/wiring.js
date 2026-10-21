(() => {
  'use strict';
  const key='mpu6500-wiring-i2c2-v1';
  const boxes=Array.from(document.querySelectorAll('#hardware-checklist input[data-check]'));
  try { const saved=JSON.parse(localStorage.getItem(key)||'{}');boxes.forEach(b=>b.checked=saved?.[b.dataset.check]===true); } catch {}
  function update(){document.getElementById('wiring-progress').textContent=`${boxes.filter(b=>b.checked).length} / ${boxes.length}`;try {localStorage.setItem(key,JSON.stringify(Object.fromEntries(boxes.map(b=>[b.dataset.check,b.checked]))));}catch{}}
  boxes.forEach(b=>b.addEventListener('change',update));
  document.getElementById('reset-wiring-checks').addEventListener('click',()=>{boxes.forEach(b=>b.checked=false);update();});
  document.getElementById('copy-wiring').addEventListener('click',async()=>{
    const rows=Array.from(document.querySelectorAll('#mpu-wiring tbody tr')).map(r=>Array.from(r.cells).map(c=>c.textContent.trim()));
    const peripherals=Array.from(document.querySelectorAll('#peripheral-wiring tbody tr')).map(r=>Array.from(r.cells).map(c=>c.textContent.trim()));
    const text='MPU6500 → STM32F103C8T6 (I2C2)\n'+rows.map(r=>`${r[0]} → ${r[1]} (${r[2]})`).join('\n')+'\n\nNgoại vi khác\n'+peripherals.map(r=>`${r[0]}: ${r[1]}`).join('\n')+'\n\nOLED + QMC: I2C1 PB6/PB7; MPU: I2C2 PB10/PB11. Hai bus riêng, tất cả chung GND.';
    const status=document.getElementById('wiring-copy-status');
    try{await navigator.clipboard.writeText(text);status.textContent='Đã sao chép chân MPU, hai bus I2C, UART và servo.';}catch{status.textContent='Không thể truy cập clipboard. Bạn có thể chọn và sao chép nội dung bảng phía trên.';}
  });
  update();
})();

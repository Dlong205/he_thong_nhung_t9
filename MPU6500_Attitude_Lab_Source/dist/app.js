(() => {
  "use strict";

  const $ = (id) => document.getElementById(id);
  const els = {
    aircraft: $("aircraft"), roll: $("hud-roll"), pitch: $("hud-pitch"), yaw: $("hud-yaw"),
    metricAcc: $("metric-acc"), metricGyro: $("metric-gyro"), metricCf: $("metric-cf"), metricKalman: $("metric-kalman"),
    packetRate: $("packet-rate"), packetCount: $("packet-count"), errorCount: $("error-count"), latency: $("latency-value"),
    connect: $("connect-button"), disconnect: $("disconnect-button"), demo: $("demo-button"), export: $("export-button"), zero: $("zero-button"),
    baud: $("baud-rate"), source: $("orientation-source"), swap: $("swap-axes"), invertRoll: $("invert-roll"), invertPitch: $("invert-pitch"),
    connectionPill: $("connection-pill"), securePill: $("secure-pill"), modeCard: $("mode-card"), modeName: $("mode-name"), modeDescription: $("mode-description"),
    rxDot: $("rx-dot"), sensorState: $("sensor-state"), uartRate: $("uart-rate"), toast: $("toast"), frameRate: $("frame-rate"),
    canvas: $("filter-chart"),
    sensors: { ax: $("ax"), ay: $("ay"), az: $("az"), gx: $("gx"), gy: $("gy"), gz: $("gz") }
  };

  const state = {
    port: null, reader: null, connected: false, demoRunning: true, receiveBuffer: "",
    packetCount: 0, errorCount: 0, packetsThisSecond: 0, measuredRate: 50,
    lastPacketAt: 0, zeroRoll: 0, zeroPitch: 0, zeroYaw: 0, start: performance.now(),
    selectedSource: "kalman", log: [], maxLog: 100000, maxChart: 3000, samples: [], elapsed: 0, previousTimestamp: null, dropped: 0, segments: 1,
    chartZoomIndex: 3, chartFollowLive: true, chartViewEnd: null,
    rawFilter: {
      lastTimestamp: null,
      gyroRoll: 0, gyroPitch: 0, cfRoll: 0, cfPitch: 0,
      roll: { angle: 0, bias: 0, p00: 0, p01: 0, p10: 0, p11: 0 },
      pitch: { angle: 0, bias: 0, p00: 0, p01: 0, p10: 0, p11: 0 }
    },
    latest: {
      timestamp: 0, acc: { roll: 0, pitch: 0 }, gyro: { roll: 0, pitch: 0 },
      cf: { roll: 0, pitch: 0 }, kalman: { roll: 0, pitch: 0 }, yaw: 0,
      ax: 0, ay: 0, az: 1, gx: 0, gy: 0, gz: 0
    },
    chartVisible: {acc:true,gyro:true,cf:true,kalman:true,yaw_raw:true,yaw_tilt:true,yaw_filtered:true},
    yawFilter: {value:null,lastElapsed:null}
  };

  const COLORS = { acc: "#ff5f68", gyro: "#c590ff", cf: "#f8d36a", kalman: "#54e6c5" };
  const CHART_WINDOWS = [1, 2, 3, 6, 10, 20, 30];
  let toastTimer;

  function toast(message, isError = false) {
    clearTimeout(toastTimer);
    els.toast.textContent = message;
    els.toast.className = `toast show${isError ? " error" : ""}`;
    toastTimer = setTimeout(() => els.toast.className = "toast", 3600);
  }

  function setConnectionUI(isConnected) {
    state.connected=isConnected;
    els.connect.disabled=isConnected;els.disconnect.disabled=!isConnected;els.baud.disabled=isConnected;
    $("protocol-mode").disabled=isConnected;
    els.connectionPill.className=`pill ${isConnected?'online':'offline'}`;
    els.connectionPill.textContent=isConnected?'COM đã mở':'Chưa kết nối';
    els.modeCard.className=`mode-card ${isConnected?'live':'demo'}`;
    els.modeName.textContent=isConnected?'Chờ gói hợp lệ':state.demoRunning?'Mô phỏng':'Đã dừng';
    els.modeDescription.textContent=isConnected?'Đang chờ dữ liệu từ firmware.':state.demoRunning?'Dữ liệu giả để xem giao diện; không phải kết quả đo.':'Giữ mẫu cuối; không có dữ liệu mới.';
    els.sensorState.textContent=isConnected?'WAIT':state.demoRunning?'DEMO':'STOP';
    updateCommandButtons();checkEnvironment();
  }

  function checkEnvironment() {
    if (!("serial" in navigator)) {
      els.securePill.textContent = "Trình duyệt chưa hỗ trợ Web Serial";
      els.securePill.className = "pill warning";
      els.connect.disabled = true;
      return;
    }
    if (!window.isSecureContext) {
      els.securePill.textContent = "Cần HTTPS hoặc localhost";
      els.securePill.className = "pill warning";
      els.connect.disabled = true;
      return;
    }
    els.securePill.textContent = "Web Serial sẵn sàng";
    els.securePill.className = "pill online";
  }

  let readTask=null, writeQueue=Promise.resolve(), closing=false;

  async function connectSerial() {
    if (!navigator.serial || closing) return toast('Cần Chrome/Edge hoặc Firefox 151+ bản desktop có Web Serial.',true);
    let port;
    try {
      els.connect.disabled=true;
      port=await navigator.serial.requestPort();
      await port.open({baudRate:Number(els.baud.value),dataBits:8,stopBits:1,parity:'none',flowControl:'none',bufferSize:65536});
      state.port=port;state.reader=port.readable.getReader();state.demoRunning=false;
      resetSession();streamPaused=false;peripheral={oled:null,mag:null,magFresh:null,magCalibrated:null,lastMagReportAt:0};$('seq-state').textContent='Chưa xác nhận';
      els.demo.textContent='Bật mô phỏng';els.uartRate.textContent=els.baud.value;
      setConnectionUI(true);readTask=readSerialLoop(state.reader);
      toast('Đã mở COM. Chờ khung dữ liệu hợp lệ từ STM32.');
    } catch(error) {
      if(port){try{await port.close();}catch{}}
      state.port=null;
      if(error.name!=='NotFoundError')toast(`Không thể kết nối: ${error.message}`,true);
      setConnectionUI(false);
    }
  }
  async function readSerialLoop(reader) {
    const textDecoder=new TextDecoder();
    try {
      while(state.connected){
        const {value,done}=await reader.read();if(done)break;if(!value)continue;
        if($('protocol-mode').value==='binary') decoder.push(value);
        else {
          state.receiveBuffer+=textDecoder.decode(value,{stream:true});
          const lines=state.receiveBuffer.split(/\r?\n/);state.receiveBuffer=lines.pop()||'';
          if(state.receiveBuffer.length>4096){state.receiveBuffer='';state.errorCount++;}
          lines.forEach(line=>{if(line.trim().startsWith('#'))firmwareText(line);else parseLine(line);});
        }
        updateDecoderStats();
      }
    }catch(error){if(state.connected)toast(`Serial bị ngắt: ${error.message}`,true);}
    finally {
      try{reader.releaseLock();}catch{}if(state.reader===reader)state.reader=null;
      if(state.connected){state.connected=false;setTimeout(()=>disconnectSerial(false),0);}
    }
  }
  async function disconnectSerial(showMessage=true) {
    if(closing)return;closing=true;state.connected=false;cancelCapture('Đã ngắt COM; phép đo bị hủy.');
    const port=state.port;
    try{if(state.reader)await state.reader.cancel();}catch{}
    if(readTask){try{await readTask;}catch{}readTask=null;}
    try{await writeQueue;}catch{}
    if(port){try{await port.close();}catch(error){logMessage('WEB',`Đóng cổng: ${error.message}`);}}
    state.port=null;state.demoRunning=false;streamPaused=false;closing=false;
    els.demo.textContent='Bật mô phỏng';setConnectionUI(false);
    if(showMessage)toast('Đã ngắt COM. Giữ mẫu cuối, không tự chuyển sang dữ liệu giả.');
  }

  function finite(value, fallback = 0) {
    const n = Number(value);
    return Number.isFinite(n) ? n : fallback;
  }

  function normalizePacket(raw) {
    const p = raw || {};
    const pair = (prefix, rollFallback = 0, pitchFallback = 0) => ({
      roll: finite(p[`${prefix}_roll`] ?? p[`roll_${prefix}`] ?? p[prefix]?.roll, rollFallback),
      pitch: finite(p[`${prefix}_pitch`] ?? p[`pitch_${prefix}`] ?? p[prefix]?.pitch, pitchFallback)
    });
    const kalman = pair("kalman", finite(p.roll), finite(p.pitch));
    return {
      timestamp: finite(p.timestamp ?? p.time ?? performance.now()),
      acc: pair("acc", kalman.roll, kalman.pitch),
      gyro: pair("gyro", kalman.roll, kalman.pitch),
      cf: pair("cf", kalman.roll, kalman.pitch),
      kalman,
      protocol: p.protocol || 'text', yaw: finite(p.yaw),
      yaw_raw: finite(p.yaw_raw ?? p.yaw), yaw_tilt: finite(p.yaw_tilt ?? p.yaw),
      ax: finite(p.ax), ay: finite(p.ay), az: finite(p.az, 1),
      gx: finite(p.gx), gy: finite(p.gy), gz: finite(p.gz)
    };
  }

  function kalman1D(filter, measuredAngle, measuredRate, dt) {
    const qAngle = 0.001, qBias = 0.003, rMeasure = 0.03;
    filter.angle += dt * (measuredRate - filter.bias);
    filter.p00 += dt * (dt * filter.p11 - filter.p01 - filter.p10 + qAngle);
    filter.p01 -= dt * filter.p11;
    filter.p10 -= dt * filter.p11;
    filter.p11 += qBias * dt;
    const innovation = measuredAngle - filter.angle;
    const s = filter.p00 + rMeasure;
    const k0 = filter.p00 / s, k1 = filter.p10 / s;
    filter.angle += k0 * innovation;
    filter.bias += k1 * innovation;
    const p00 = filter.p00, p01 = filter.p01;
    filter.p00 -= k0 * p00;
    filter.p01 -= k0 * p01;
    filter.p10 -= k1 * p00;
    filter.p11 -= k1 * p01;
    return filter.angle;
  }

  function rawToAttitude(values) {
    const [timestamp, ax, ay, az, gx, gy, gz] = values.map(Number);
    if (![timestamp, ax, ay, az, gx, gy, gz].every(Number.isFinite)) throw new Error("RAW không hợp lệ");
    const f = state.rawFilter;
    let dt = f.lastTimestamp == null ? 0.002 : (timestamp - f.lastTimestamp) / 1000;
    if (!(dt > 0 && dt < 0.1)) dt = 0.002;
    f.lastTimestamp = timestamp;
    const rollAcc = Math.atan2(ay, az) * 180 / Math.PI;
    const pitchAcc = Math.atan2(-ax, Math.sqrt(ay * ay + az * az)) * 180 / Math.PI;
    f.gyroRoll += gx * dt; f.gyroPitch += gy * dt;
    f.cfRoll = 0.98 * (f.cfRoll + gx * dt) + 0.02 * rollAcc;
    f.cfPitch = 0.98 * (f.cfPitch + gy * dt) + 0.02 * pitchAcc;
    return normalizePacket({
      timestamp, roll_acc: rollAcc, pitch_acc: pitchAcc,
      roll_gyro: f.gyroRoll, pitch_gyro: f.gyroPitch,
      roll_cf: f.cfRoll, pitch_cf: f.cfPitch,
      roll_kalman: kalman1D(f.roll, rollAcc, gx, dt),
      pitch_kalman: kalman1D(f.pitch, pitchAcc, gy, dt),
      yaw: 0, ax, ay, az, gx, gy, gz
    });
  }

  function parseLine(line) {
    const clean = line.trim();
    if (!clean) return;
    if(clean.startsWith("#")){firmwareText(clean);return;}
    try {
      let packet;
      if (clean.startsWith("{")) {
        packet = normalizePacket(JSON.parse(clean));
      } else {
        const parts = clean.split(",").map(v => v.trim());
        if (parts[0].toUpperCase() === "ATT" && parts.length >= 10) {
          if(parts.slice(1).some(v=>v===''||!Number.isFinite(Number(v))))throw Error("ATT không hợp lệ");
          packet = normalizePacket({
            timestamp: parts[1], roll_acc: parts[2], pitch_acc: parts[3], roll_gyro: parts[4], pitch_gyro: parts[5],
            roll_cf: parts[6], pitch_cf: parts[7], roll_kalman: parts[8], pitch_kalman: parts[9], yaw: parts[10],
            ax: parts[11], ay: parts[12], az: parts[13], gx: parts[14], gy: parts[15], gz: parts[16]
          });
        } else if (parts[0].toUpperCase() === "RAW" && parts.length >= 8) {
          packet = rawToAttitude(parts.slice(1, 8));
        } else if (parts.length >= 2 && parts.every(v => Number.isFinite(Number(v)))) {
          packet = normalizePacket({ roll: parts[0], pitch: parts[1], yaw: parts[2] || 0 });
        } else {
          throw new Error("Sai định dạng");
        }
      }
      packet.protocol = clean.startsWith("RAW") ? "raw-preview" : "text";
      ingest(packet, true);
    } catch (_) {
      state.errorCount++;
      els.errorCount.textContent = state.errorCount.toLocaleString("vi-VN");
    }
  }

  function ingest(packet,isReal) {
    const now=performance.now();
    packet.real=isReal;if(!isReal)packet.protocol="demo";
    packet.magFresh=isReal?peripheral.magFresh===true&&now-peripheral.lastMagReportAt<1800:true;
    if(state.previousTimestamp!==null){
      let delta=packet.timestamp-state.previousTimestamp;
      if(delta<0 && state.previousTimestamp>0xf0000000 && packet.timestamp<0x0fffffff)delta+=4294967296;
      if(delta<0 || delta>60000){state.segments++;state.samples=[];cancelCapture('Timestamp reset hoặc khoảng trống quá lớn. Hãy đo lại.');delta=0;}
      state.elapsed+=delta/1000;
    }
    state.previousTimestamp=packet.timestamp;packet.elapsed=state.elapsed;
    updateYawFilter(packet);
    const age=state.lastPacketAt?now-state.lastPacketAt:0;
    state.lastPacketAt=now;state.latest=packet;state.packetCount++;state.packetsThisSecond++;
    els.latency.textContent=age?`${age.toFixed(1)} ms`:'—';els.packetCount.textContent=state.packetCount.toLocaleString('vi-VN');
    state.samples.push(packet);if(state.samples.length>state.maxChart)state.samples.shift();
    const row={reference_measured:capture&&isReal?capture.reference:'',reference_axis:capture&&isReal?capture.axis:'',trial_id:capture&&isReal?capture.id:'',source:isReal?'hardware':'demo',protocol:packet.protocol||'demo',segment:state.segments,timestamp:packet.timestamp,elapsed_s:state.elapsed,
      roll_acc:packet.acc.roll,pitch_acc:packet.acc.pitch,roll_gyro:packet.gyro.roll,pitch_gyro:packet.gyro.pitch,roll_cf:packet.cf.roll,pitch_cf:packet.cf.pitch,roll_kalman:packet.kalman.roll,pitch_kalman:packet.kalman.pitch,
      roll_fixed:packet.fixed?.roll??'',pitch_fixed:packet.fixed?.pitch??'',ref_command:packet.ref??'',yaw_raw:packet.yaw_raw??'',yaw_tilt:packet.yaw_tilt??'',yaw_filtered_web:packet.yaw_filtered??'',yaw_filter_mode:$('yaw-filter').value,mag_fresh:isReal?(peripheral.magFresh===true?'1':'0'):'demo',mag_calibrated:isReal?(peripheral.magCalibrated===true?'1':'0'):'demo',exec_us:packet.exec_us??'',dt_us:packet.dt_us??'',ax:packet.ax,ay:packet.ay,az:packet.az,gx:packet.gx,gy:packet.gy,gz:packet.gz};
    state.log.push(row);if(state.log.length>state.maxLog){state.log.shift();state.dropped++;}
    if(isReal && packet.protocol==='binary'){
      updateStats(execStats,packet.exec_us);updateStats(dtStats,packet.dt_us);
      maxDelta=Math.max(maxDelta,Math.abs(packet.kalman.roll-packet.fixed.roll),Math.abs(packet.kalman.pitch-packet.fixed.pitch));
      if(packet.dt_us>2000)overBudget++;
    }
    if(capture&&isReal){
      if(age>1000){cancelCapture('Mất dữ liệu hơn 1 giây; phép đo bị hủy.');}
      else if(packet.protocol!=='binary'){cancelCapture('Phép đo yêu cầu dữ liệu K03 binary.');}
      else if(now-capture.start<=5000){capture.rows.push(row);}
    }
    els.rxDot.classList.add('active');setTimeout(()=>els.rxDot.classList.remove('active'),65);
    updateNumbers();
  }

  const wrap180 = angle => ((angle + 180) % 360 + 360) % 360 - 180;
  const YAW_TAU={off:0,fast:.2,balanced:.6,smooth:1.2};
  function updateYawFilter(packet) {
    const f=state.yawFilter;
    const raw=$('yaw-source').value==='raw'?packet.yaw_raw:packet.yaw_tilt;
    if(!packet.magFresh||!Number.isFinite(raw)){
      f.value=null;f.lastElapsed=null;packet.yaw_filtered=null;return;
    }
    const gap=f.lastElapsed===null?Infinity:packet.elapsed-f.lastElapsed;
    const tau=YAW_TAU[$('yaw-filter').value]??0;
    // Shortest circular distance keeps 179° -> -179° as a 2° change.
    f.value=f.value===null||gap<=0||gap>.5||tau===0
      ? wrap180(raw)
      : wrap180(f.value+(1-Math.exp(-gap/tau))*wrap180(raw-f.value));
    f.lastElapsed=packet.elapsed;
    packet.yaw_filtered=f.value;
  }
  function yawAvailable() {
    if (!state.connected) return state.demoRunning;
    return state.latest.protocol==='binary' && peripheral.mag===true &&
      peripheral.magFresh===true && performance.now()-peripheral.lastMagReportAt<1800 &&
      performance.now()-state.lastPacketAt<1000;
  }
  function yawSourceValue() {
    if($('yaw-filter').value!=='off'&&Number.isFinite(state.latest.yaw_filtered))return state.latest.yaw_filtered;
    return $('yaw-source').value==='raw' ? state.latest.yaw_raw : state.latest.yaw_tilt;
  }
  function displayAngles() {
    const src = state.latest[state.selectedSource] || state.latest.kalman;
    let roll = src.roll - state.zeroRoll;
    let pitch = src.pitch - state.zeroPitch;
    if (els.swap.checked) [roll, pitch] = [pitch, roll];
    if (els.invertRoll.checked) roll *= -1;
    if (els.invertPitch.checked) pitch *= -1;
    const active=$('yaw-source').value!=='off'&&yawAvailable();
    const y=active?wrap180(yawSourceValue()-state.zeroYaw)*($('invert-yaw').checked?-1:1):0;
    return {roll,pitch,yaw:y,yawActive:active};
  }

  function updateNumbers() {
    const { roll, pitch, yaw, yawActive } = displayAngles();
    els.roll.textContent = `${roll.toFixed(2)}°`;
    els.pitch.textContent = `${pitch.toFixed(2)}°`;
    els.yaw.textContent = $('yaw-source').value==='off'?'OFF':yawActive?`${yaw.toFixed(2)}°`:'—';
    $('yaw-relative-value').textContent=yawActive?`${yaw.toFixed(2)}°`:'—';
    $('yaw-needle').style.transform=`rotate(${yaw}deg)`;
    const axis=$("chart-axis").value;
    for(const [el,key] of [[els.metricAcc,'acc'],[els.metricGyro,'gyro'],[els.metricCf,'cf'],[els.metricKalman,'kalman']]){
      const value=state.latest[key]?.[axis];el.textContent=Number.isFinite(value)?`${value.toFixed(2)}°`:'—';
    }
    Object.keys(els.sensors).forEach(key => els.sensors[key].textContent = state.latest[key].toFixed(3));
  }

  function updateAircraft() {
    const { roll, pitch, yaw } = displayAngles();
    window.aircraftView?.setAttitude(roll, pitch, yaw);
  }

  function demoTick() {
    if (!state.demoRunning || state.connected) return;
    const t = (performance.now() - state.start) / 1000;
    const trueRoll = 22 * Math.sin(t * .72) + 7 * Math.sin(t * 1.8);
    const truePitch = 10 * Math.sin(t * .43 + .8);
    const noise = () => (Math.random() - .5) * 2.4;
    const drift = t * .085;
    const accRoll = trueRoll + noise();
    const accPitch = truePitch + noise() * .65;
    const gyroRoll = trueRoll + drift;
    const gyroPitch = truePitch - drift * .45;
    const cfRoll = trueRoll + noise() * .13 + drift * .04;
    const cfPitch = truePitch + noise() * .1;
    const kalmanRoll = trueRoll + noise() * .05;
    const kalmanPitch = truePitch + noise() * .04;
    const demoYaw = 4 * Math.sin(t * .15);
    const rr = trueRoll * Math.PI / 180, pr = truePitch * Math.PI / 180;
    ingest(normalizePacket({
      timestamp: Math.round(t * 1000), roll_acc: accRoll, pitch_acc: accPitch,
      roll_gyro: gyroRoll, pitch_gyro: gyroPitch, roll_cf: cfRoll, pitch_cf: cfPitch,
      roll_kalman: kalmanRoll, pitch_kalman: kalmanPitch,
      yaw: demoYaw, yaw_raw: demoYaw + .16 * trueRoll,
      yaw_tilt: demoYaw,
      ax: -Math.sin(pr), ay: Math.sin(rr) * Math.cos(pr), az: Math.cos(rr) * Math.cos(pr),
      gx: 15.8 * Math.cos(t * .72), gy: 4.3 * Math.cos(t * .43 + .8), gz: .2 * Math.sin(t)
    }), false);
  }

  function resizeCanvas() {
    const rect = els.canvas.getBoundingClientRect();
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    els.canvas.width = Math.max(1, Math.round(rect.width * dpr));
    els.canvas.height = Math.max(1, Math.round(rect.height * dpr));
    els.canvas.dataset.dpr = String(dpr);
  }

  function chartRange() {
    const span = CHART_WINDOWS[state.chartZoomIndex];
    const latestEnd = Math.max(span, state.elapsed);
    const oldest = state.samples[0]?.elapsed ?? 0;
    const earliestEnd = Math.min(latestEnd, oldest + span);
    const end = state.chartFollowLive ? latestEnd : Math.max(earliestEnd, Math.min(latestEnd, state.chartViewEnd ?? latestEnd));
    return {start:end-span,end,span,earliestEnd,latestEnd};
  }

  function updateChartToolbar() {
    const span=CHART_WINDOWS[state.chartZoomIndex];
    $('chart-zoom-label').textContent=`${span} giây`;
    $('chart-zoom-in').disabled=state.chartZoomIndex===0;
    $('chart-zoom-out').disabled=state.chartZoomIndex===CHART_WINDOWS.length-1;
    $('chart-live').disabled=state.chartFollowLive;
    $('chart-window').textContent=`Cửa sổ ${span} giây · ${state.chartFollowLive?'đang theo dữ liệu mới':'đang xem đoạn trước'} · kéo để tua, cuộn chuột để phóng to`;
  }

  function setChartZoom(nextIndex) {
    const index=Math.max(0,Math.min(CHART_WINDOWS.length-1,nextIndex));
    if(index===state.chartZoomIndex)return;
    const oldRange=chartRange();
    state.chartZoomIndex=index;
    if(!state.chartFollowLive)state.chartViewEnd=(oldRange.start+oldRange.end+CHART_WINDOWS[index])/2;
    updateChartToolbar();
  }

  function drawChart() {
    const canvas=els.canvas,ctx=canvas.getContext('2d'),dpr=Number(canvas.dataset.dpr||1),w=canvas.width/dpr,h=canvas.height/dpr;
    ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
    const axis=$('chart-axis').value,{start,end,span}=chartRange(),rows=state.samples.filter(p=>p.elapsed>=start&&p.elapsed<=end);
    const series=(axis==='yaw'
      ? [['yaw_raw','#70b7ff'],['yaw_tilt','#54e6c5'],['yaw_filtered','#ff8d5c']]
      : Object.entries(COLORS).map(([key,color])=>[key,color]))
      .filter(([key])=>state.chartVisible[key]);
    const points=series.map(([key,color])=>{
      let previous=null,unwrapped=0;
      return {key,color,data:rows.map(p=>{
        const raw=axis==='yaw'?p[key]:p[key]?.[axis];
        if(axis==='yaw'&&!p.magFresh){previous=null;return null;}
        if(!Number.isFinite(raw)){previous=null;return null;}
        if(axis==='yaw'){unwrapped=previous===null?raw:unwrapped+wrap180(raw-previous);previous=raw;}
        return {time:p.elapsed,value:axis==='yaw'?unwrapped:raw};
      })};
    });
    const nums=points.flatMap(s=>s.data.filter(Boolean).map(p=>p.value));
    let low=Math.floor((Math.min(-10,...nums)-3)/10)*10,high=Math.ceil((Math.max(10,...nums)+3)/10)*10;
    const pad={l:44,r:14,t:14,b:26},cw=w-pad.l-pad.r,ch=h-pad.t-pad.b;
    ctx.font='9px ui-monospace,monospace';ctx.fillStyle='#8eaaa7';ctx.lineWidth=1;ctx.strokeStyle='rgba(160,222,216,.09)';
    for(let i=0;i<=4;i++){let y=pad.t+ch*i/4;ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(w-pad.r,y);ctx.stroke();ctx.fillText(`${(high-(high-low)*i/4).toFixed(0)}°`,4,y+3);}
    for(let i=0;i<=6;i++){let x=pad.l+cw*i/6;ctx.beginPath();ctx.moveTo(x,pad.t);ctx.lineTo(x,h-pad.b);ctx.stroke();ctx.fillText(`${(start+span*i/6).toFixed(1)}s`,x-10,h-7);}
    for(const s of points){ctx.beginPath();ctx.strokeStyle=s.color;ctx.lineWidth=s.key==='kalman'||s.key==='yaw_filtered'?2.4:s.key==='yaw_tilt'?1.8:1.2;let prev=null;
      for(const point of s.data){if(!point){prev=null;continue;}const x=pad.l+(point.time-start)/span*cw,y=pad.t+(high-point.value)/(high-low)*ch;
        if(prev===null||point.time-prev>.25)ctx.moveTo(x,y);else ctx.lineTo(x,y);prev=point.time;
      }ctx.stroke();}
    if(!series.length){ctx.fillStyle='#8eaaa7';ctx.textAlign='center';ctx.font='12px ui-sans-serif, sans-serif';ctx.fillText('Chọn ít nhất một đường để xem',w/2,h/2);ctx.textAlign='left';}
  }

  function exportCsv() {
    if (!state.log.length) return toast("Chưa có dữ liệu để xuất.", true);
    const headers = Object.keys(state.log[0]);
    const csv = [headers.join(","), ...state.log.map(row => headers.map(key => row[key]).join(","))].join("\n");
    const url = URL.createObjectURL(new Blob([csv], { type: "text/csv;charset=utf-8" }));
    const a = document.createElement("a"); a.href = url; a.download = `mpu6500-attitude-${new Date().toISOString().replace(/[:.]/g, "-")}.csv`; a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    toast(`Đã xuất ${state.log.length.toLocaleString("vi-VN")} mẫu CSV.`);
  }

  let lastFrame = performance.now(), frameCounter = 0, fpsStart = lastFrame;
  function animate(now) {
    if(now-lastFrame<33){requestAnimationFrame(animate);return;}
    updateAircraft(); drawChart();
    frameCounter++;
    if (now - fpsStart >= 1000) {
      els.frameRate.textContent = `${Math.round(frameCounter * 1000 / (now - fpsStart))} FPS`;
      frameCounter = 0; fpsStart = now;
    }
    lastFrame = now;
    requestAnimationFrame(animate);
  }

  let streamPaused=false, peripheral={oled:null,mag:null,magFresh:null,magCalibrated:null,lastMagReportAt:0}, capture=null, measurementRows=[],consoleLines=[],maxDelta=0,overBudget=0;
  let execStats={n:0,mean:0,m2:0,min:Infinity,max:-Infinity},dtStats={...execStats};
  const decoder=new K03.Decoder(p=>ingest(p,true),firmwareText);
  function updateStats(s,x){s.n++;const d=x-s.mean;s.mean+=d/s.n;s.m2+=d*(x-s.mean);s.min=Math.min(s.min,x);s.max=Math.max(s.max,x);}
  function logMessage(kind,text){consoleLines.push(`[${new Date().toLocaleTimeString('vi-VN')}] ${kind} ${text}`);if(consoleLines.length>150)consoleLines.shift();const el=$('firmware-console');el.textContent=consoleLines.join('\n');el.scrollTop=el.scrollHeight;}
  function firmwareText(text){
    logMessage('RX',text);
    let m=text.match(/OLED present=(\d) MAG present=(\d)/);if(m){peripheral.oled=m[1]==='1';peripheral.mag=m[2]==='1';}
    if(/# OLED OK/.test(text))peripheral.oled=true;if(/# OLED ABSENT/.test(text))peripheral.oled=false;
    if(/# QMC5883P OK/.test(text))peripheral.mag=true;if(/# QMC5883P ABSENT/.test(text))peripheral.mag=false;
    m=text.match(/# mag present=(\d) fresh=(\d) age_ms=(\d+) cal=(\d) samples=(\d+) errors=(\d+)/);
    if(m){peripheral.mag=m[1]==='1';peripheral.magFresh=m[2]==='1';peripheral.magCalibrated=m[4]==='1';peripheral.lastMagReportAt=performance.now();}
    if(/# MAGCAL start/.test(text))peripheral.magCalibrated=false;
    if(/# MAGCAL OK/.test(text))peripheral.magCalibrated=true;
    if(/# MAGCAL FAIL/.test(text))peripheral.magCalibrated=false;
    m=text.match(/# BIN ([01])/);if(m){streamPaused=m[1]==='0';$('command-status').textContent=`Firmware xác nhận stream ${streamPaused?'tắt':'bật'}.`;if(streamPaused)cancelCapture('Stream đã tắt; phép đo bị hủy.');}
    m=text.match(/seq=(\w+)/)||text.match(/# SEQ (STATIC|STEP|DRIFT)/);if(m)$('seq-state').textContent=m[1];
    if(/# SEQ.*(?:done|stop)/.test(text))$('seq-state').textContent='MANUAL';
    if(/# MAGCAL/.test(text))$('command-status').textContent=text.replace(/^#\s*/,'');
  }
  function updateDecoderStats(){const st=decoder.stats;$('crc-errors').textContent=st.crc;$('decoder-errors').textContent=`Độ dài: ${st.length} · không hữu hạn: ${st.nonfinite} · byte bỏ: ${st.discarded}`;els.errorCount.textContent=String(state.errorCount+st.crc+st.length+st.nonfinite);}
  function updateCommandButtons(){document.querySelectorAll('[data-hardware]').forEach(b=>b.disabled=!state.connected||$('protocol-mode').value!=='binary'||closing);}
  function sendCommand(command){
    if(!state.connected||!state.port?.writable||$('protocol-mode').value!=='binary')return toast('Kết nối K03 binary trước khi gửi lệnh.',true);
    if(capture && command!=='OLED')cancelCapture('Đã gửi lệnh điều khiển; hủy phép đo tĩnh đang chạy.');
    const port=state.port;
    writeQueue=writeQueue.catch(()=>{}).then(async()=>{
      if(!state.connected||state.port!==port)throw Error('Cổng COM đã đóng');
      const writer=port.writable.getWriter();try{await writer.write(new TextEncoder().encode(command+'\r\n'));}finally{writer.releaseLock();}
      logMessage('TX',command);$('command-status').textContent=`Đã gửi ${command}. Kiểm tra phản hồi firmware hoặc góc lệnh nhận lại.`;
    }).catch(error=>{logMessage('LỖI',error.message);toast(`Gửi lệnh thất bại: ${error.message}`,true);});
  }
  function cancelCapture(reason){if(!capture)return;capture=null;$('measurement-status').textContent=reason;$('capture-static').textContent='Thu 5 giây đứng yên';}
  function resetSession(){
    cancelCapture('Đã mở phiên mới.');decoder.reset();state.log=[];state.samples=[];state.elapsed=0;state.previousTimestamp=null;state.dropped=0;state.segments=1;
    state.chartFollowLive=true;state.chartViewEnd=null;updateChartToolbar();
    state.packetCount=0;state.errorCount=0;state.packetsThisSecond=0;state.measuredRate=0;state.lastPacketAt=0;state.receiveBuffer='';
    state.zeroRoll=state.zeroPitch=state.zeroYaw=0;state.yawFilter={value:null,lastElapsed:null};maxDelta=0;overBudget=0;
    if(state.selectedSource==="fixed"){state.selectedSource="kalman";els.source.value="kalman";}
    execStats={n:0,mean:0,m2:0,min:Infinity,max:-Infinity};dtStats={...execStats};
    state.rawFilter={lastTimestamp:null,gyroRoll:0,gyroPitch:0,cfRoll:0,cfPitch:0,roll:{angle:0,bias:0,p00:0,p01:0,p10:0,p11:0},pitch:{angle:0,bias:0,p00:0,p01:0,p10:0,p11:0}};
    state.latest={timestamp:0,acc:{roll:0,pitch:0},gyro:{roll:0,pitch:0},cf:{roll:0,pitch:0},kalman:{roll:0,pitch:0},yaw:0,ax:0,ay:0,az:1,gx:0,gy:0,gz:0};
    updateNumbers();updateDecoderStats();els.packetCount.textContent='0';
  }
  function exportRows(rows,name){
    if(!rows.length)return toast('Chưa có kết quả để xuất.',true);
    const keys=Object.keys(rows[0]),esc=v=>'"'+String(v??'').replace(/"/g,'""')+'"';
    const csv=[keys.map(esc).join(','),...rows.map(r=>keys.map(k=>esc(r[k])).join(','))].join('\r\n');
    const url=URL.createObjectURL(new Blob(['\ufeff'+csv],{type:'text/csv;charset=utf-8'}));const a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  }
  function beginCapture(){
    const v=$('reference-angle').value.trim(),angle=Number(v),now=performance.now();
    if(!v||!Number.isFinite(angle)||Math.abs(angle)>180)return toast('Nhập góc thật đo bằng thước/encoder trong khoảng ±180°.',true);
    if(!state.connected||state.latest.protocol!=='binary'||!state.lastPacketAt||now-state.lastPacketAt>1000||streamPaused)return toast('Cần dữ liệu K03 binary thật, đang cập nhật.',true);
    capture={start:now,axis:$('measure-axis').value,reference:angle,rows:[],id:new Date().toISOString()};
    $('measurement-status').textContent='Đang thu 5 giây — giữ mạch đứng yên';$('capture-static').textContent='Đang thu…';
  }
  function finishCapture(){
    const c=capture;if(!c)return;capture=null;
    if(c.rows.length<2 || (c.rows.at(-1).elapsed_s-c.rows[0].elapsed_s)<4){$('measurement-status').textContent='Không đủ khoảng dữ liệu liên tục. Hãy đo lại.';return;}
    const body=$('measurement-results');body.replaceChildren();
    for(const key of ['acc','gyro','cf','kalman']){
      const stats=K03.metrics(c.rows.map(r=>r[`${c.axis}_${key}`]),c.reference);
      const row={trial_id:c.id,axis:c.axis,reference_measured_deg:c.reference,method:key,baud:els.baud.value,protocol:'K03 binary',first_ts_ms:c.rows[0].timestamp,last_ts_ms:c.rows.at(-1).timestamp,...stats};measurementRows.push(row);
      const tr=document.createElement('tr');[key.toUpperCase(),stats.n,...['bias','mae','rmse','std'].map(k=>stats[k].toFixed(4))].forEach(v=>{const td=document.createElement('td');td.textContent=v;tr.appendChild(td);});body.appendChild(tr);
    }
    $('measurement-status').textContent=`${c.axis.toUpperCase()} · chuẩn ${c.reference}° · ${c.rows.length} mẫu`;
    $('capture-static').textContent='Thu 5 giây đứng yên';
  }
  function labMonitor(){
    const p=state.latest,now=performance.now(),fresh=!!state.lastPacketAt&&now-state.lastPacketAt<1000;
    const hardware=state.connected&&p.real&&fresh,binary=hardware&&p.protocol==='binary';
    let health=state.connected?(streamPaused?'STREAM ĐÃ TẮT':!state.lastPacketAt?'CHỜ DỮ LIỆU':fresh?'LIVE':'MẤT DỮ LIỆU > 1 s'):state.demoRunning?'DEMO':'ĐÃ DỪNG';
    $('data-health').textContent=health;$('data-health').className='tiny-badge'+(state.connected&&!fresh?' stale':'');
    if(state.connected){els.modeName.textContent=health;els.modeDescription.textContent=hardware?(p.protocol==='raw-preview'?'RAW: góc xem trước tính trên PC.':'Góc nhận từ firmware STM32.'):streamPaused?'Bấm Bật stream để tiếp tục nhận.':'Kiểm tra nguồn, baud, giao thức và dây UART.';els.sensorState.textContent=health;}
    const fmt=(x,n=1)=>Number.isFinite(x)?x.toFixed(n):'—';
    $('exec-time').textContent=binary?`${p.exec_us} µs`:'—';$('sample-dt').textContent=binary?`${p.dt_us} µs`:'—';
    $('exec-range').textContent=execStats.n?`TB ${fmt(execStats.mean)} · min ${execStats.min} · max ${execStats.max} µs`:'Chưa có mẫu thật';
    $('dt-range').textContent=dtStats.n?`STD ${fmt(Math.sqrt(dtStats.n>1?dtStats.m2/(dtStats.n-1):0))} µs · ${overBudget}/${dtStats.n} mẫu log >2 ms`:'Chưa có mẫu thật';
    $('fixed-delta').textContent=binary?`${fmt(p.kalman.roll-p.fixed.roll,4)} / ${fmt(p.kalman.pitch-p.fixed.pitch,4)}°`:'—';
    $('fixed-max').textContent=`Max |Δ| phiên: ${fmt(maxDelta,4)}° · fixed một phần`;
    $('servo-feedback').textContent=binary?`${p.ref}°`:'—';
    $('oled-roll').textContent=binary?fmt(p.kalman.roll,2):'—';
    $('oled-pitch').textContent=binary?fmt(p.kalman.pitch,2):'—';
    const yawLive=yawAvailable();
    $('oled-yaw').textContent=binary&&yawLive?fmt(p.yaw_tilt,2):'—';
    $('yaw-tilt-value').textContent=yawLive?`${fmt(p.yaw_tilt,2)}°`:'—';
    $('yaw-raw-value').textContent=yawLive?`${fmt(p.yaw_raw,2)}°`:'—';
    $('yaw-filtered-value').textContent=yawLive&&Number.isFinite(p.yaw_filtered)?`${fmt(p.yaw_filtered,2)}°`:'—';
    $('yaw-health').textContent=state.demoRunning&&!state.connected?'Mô phỏng':!state.connected?'Chưa kết nối':peripheral.mag===false?'Không thấy QMC':!peripheral.lastMagReportAt?'Chờ báo cáo từ kế':yawLive?'Mẫu mới':'Mẫu cũ / mất từ kế';
    $('yaw-calibration').textContent=state.demoRunning&&!state.connected?'Mô phỏng':peripheral.magCalibrated===true?'Đã hiệu chuẩn':peripheral.magCalibrated===false?'Chưa hiệu chuẩn':'Chưa xác nhận';
    $('oled-ref').textContent=binary?fmt(p.ref,2):'—';
    const status=x=>x===null?'?':x?'OK':'không có';$('peripheral-state').textContent=`OLED ${status(peripheral.oled)} · MAG ${status(peripheral.mag)}`;
    $('session-info').textContent=`${state.connected?'Phần cứng':state.demoRunning?'Mô phỏng':'Phiên đã dừng'} · ${state.log.length.toLocaleString('vi-VN')} mẫu lưu · ${state.elapsed.toFixed(1)} s · giới hạn 100.000 mẫu${state.dropped?' · đã bỏ '+state.dropped+' mẫu đầu':''}`;
    $('capture-static').disabled=!binary||!!capture||streamPaused;
    const fixedOption=els.source.querySelector('[value="fixed"]');if(fixedOption)fixedOption.disabled=!p.fixed;
    if(capture){if(!hardware||streamPaused)cancelCapture('Dữ liệu bị gián đoạn; phép đo bị hủy.');else if(now-capture.start>=5000)finishCapture();else $('measurement-status').textContent=`Đang thu ${(5-(now-capture.start)/1000).toFixed(1)} s · ${capture.rows.length} mẫu`;}
    updateDecoderStats();updateCommandButtons();
  }
  $('send-ref').addEventListener('click',()=>{const val=$('servo-angle').value.trim(),angle=Number(val);if(!val||!Number.isFinite(angle)||angle<-90||angle>90||!Number.isInteger(angle))return toast('Nhập góc nguyên từ −90° đến +90°.',true);sendCommand(`REF ${angle}`);});
  document.querySelectorAll('[data-command]').forEach(b=>b.addEventListener('click',()=>sendCommand(b.dataset.command)));
  $('clear-session').addEventListener('click',()=>{resetSession();toast('Đã xóa dữ liệu phiên cũ và bắt đầu phiên mới.');});
  $('download-session').addEventListener('click',exportCsv);
  $('capture-static').addEventListener('click',beginCapture);
  $('export-measurements').addEventListener('click',()=>exportRows(measurementRows,`k03-static-${Date.now()}.csv`));
  $('clear-console').addEventListener('click',()=>{consoleLines=[];$('firmware-console').textContent='Đã xóa nhật ký.';});
  function updateChartControls(){const axis=$('chart-axis').value,yaw=axis==='yaw';
    $('chart-title').textContent=yaw?'Yaw thô, bù nghiêng và lọc web':`So sánh ${axis==='roll'?'Roll':'Pitch'} theo thời gian`;
    const entries=yaw?[['yaw_raw','Yaw thô','leg-yaw-raw'],['yaw_tilt','Yaw bù nghiêng','leg-kalman'],['yaw_filtered','Yaw lọc web','leg-yaw-filtered']]
      :[['acc','Acc','leg-acc'],['gyro','Gyro','leg-gyro'],['cf','CF','leg-cf'],['kalman','Kalman','leg-kalman']];
    for(const [i,label] of [...document.querySelectorAll('.legend label')].entries()){
      const entry=entries[i];label.hidden=!entry;if(!entry)continue;
      const [key,name,klass]=entry;label.className=klass;label.querySelector('input').dataset.chartSeries=key;
      label.querySelector('input').checked=state.chartVisible[key];label.querySelector('span').textContent=name;
    }
    updateNumbers();
  }
  $('chart-axis').addEventListener('change',updateChartControls);
  document.querySelectorAll('[data-chart-series]').forEach(box=>box.addEventListener('change',()=>{state.chartVisible[box.dataset.chartSeries]=box.checked;}));
  $('chart-zoom-in').addEventListener('click',()=>setChartZoom(state.chartZoomIndex-1));
  $('chart-zoom-out').addEventListener('click',()=>setChartZoom(state.chartZoomIndex+1));
  $('chart-live').addEventListener('click',()=>{state.chartFollowLive=true;state.chartViewEnd=null;updateChartToolbar();});
  $('chart-expand').addEventListener('click',()=>{
    const expanded=$('chart-panel').classList.toggle('expanded');
    document.body.classList.toggle('chart-expanded',expanded);
    $('chart-expand').textContent=expanded?'✕ Thu gọn':'⛶ Mở rộng';
    $('chart-expand').setAttribute('aria-pressed',String(expanded));
    requestAnimationFrame(resizeCanvas);
  });
  document.addEventListener('keydown',event=>{
    if(event.key==='Escape'&&$('chart-panel').classList.contains('expanded'))$('chart-expand').click();
  });
  els.canvas.addEventListener('wheel',event=>{
    event.preventDefault();
    setChartZoom(state.chartZoomIndex+(event.deltaY<0?-1:1));
  },{passive:false});
  let chartDrag=null;
  els.canvas.addEventListener('pointerdown',event=>{
    if(!state.samples.length)return;
    chartDrag={x:event.clientX,end:chartRange().end};
    els.canvas.setPointerCapture(event.pointerId);
    els.canvas.classList.add('dragging');
  });
  els.canvas.addEventListener('pointermove',event=>{
    if(!chartDrag)return;
    const range=chartRange();
    const plotWidth=Math.max(1,els.canvas.clientWidth-58);
    state.chartFollowLive=false;
    state.chartViewEnd=Math.max(range.earliestEnd,Math.min(range.latestEnd,chartDrag.end-(event.clientX-chartDrag.x)/plotWidth*range.span));
    updateChartToolbar();
  });
  const stopChartDrag=()=>{chartDrag=null;els.canvas.classList.remove('dragging');};
  els.canvas.addEventListener('pointerup',stopChartDrag);
  els.canvas.addEventListener('pointercancel',stopChartDrag);
  function resetYawView(){state.zeroYaw=0;state.yawFilter={value:null,lastElapsed:null};state.samples.forEach(p=>{p.yaw_filtered=null;});state.latest.yaw_filtered=null;updateNumbers();}
  $('yaw-source').addEventListener('change',resetYawView);
  $('yaw-filter').addEventListener('change',resetYawView);
  $('invert-yaw').addEventListener('change',updateNumbers);
  $('zero-yaw').addEventListener('click',()=>{if(!yawAvailable())return toast('Cần mẫu Yaw mới từ QMC5883P để đặt mốc.',true);state.zeroYaw=yawSourceValue();updateNumbers();toast('Đã đặt Yaw hiện tại làm mốc 0°.');});
  $('protocol-mode').addEventListener('change',()=>{resetSession();updateCommandButtons();});
  setInterval(labMonitor,200);

  els.connect.addEventListener("click", connectSerial);
  els.disconnect.addEventListener("click", () => disconnectSerial(true));
  els.demo.addEventListener("click", () => {
    if (state.connected) return toast("Hãy ngắt STM32 trước khi bật mô phỏng.", true);
    state.demoRunning = !state.demoRunning;
    if(state.demoRunning)resetSession();
    els.demo.textContent = state.demoRunning ? "Tạm dừng mô phỏng" : "Tiếp tục mô phỏng";
    setConnectionUI(false);
  });
  els.export.addEventListener("click", exportCsv);
  els.zero.addEventListener("click", () => {
    const src = state.latest[state.selectedSource] || state.latest.kalman;
    state.zeroRoll = src.roll; state.zeroPitch = src.pitch; if(yawAvailable())state.zeroYaw=yawSourceValue();
    toast("Đã đặt tư thế hiện tại làm mốc 0°.");
  });
  els.source.addEventListener("change", () => { state.selectedSource = els.source.value; state.zeroRoll = 0; state.zeroPitch = 0; });
  els.baud.addEventListener("change", () => els.uartRate.textContent = els.baud.value);
  window.addEventListener("resize", resizeCanvas);
  if('ResizeObserver' in window)new ResizeObserver(resizeCanvas).observe($('chart-panel').querySelector('.chart-wrap'));
  if ("serial" in navigator) navigator.serial.addEventListener("disconnect", event => {
    if(event.target===state.port)disconnectSerial(false);
  });

  setInterval(demoTick, 20);
  setInterval(() => {
    state.measuredRate = state.packetsThisSecond;
    els.packetRate.textContent = `${state.measuredRate} Hz`;
    state.packetsThisSecond = 0;
  }, 1000);

  updateChartControls();updateChartToolbar();setConnectionUI(false); resizeCanvas(); requestAnimationFrame(animate);
})();

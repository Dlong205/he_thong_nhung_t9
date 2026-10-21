/* Local, dependency-free 3D mesh viewer. Camera controls never alter sensor data. */
(() => {
  'use strict';
  const host = document.getElementById('aircraft');
  const canvas = document.createElement('canvas');
  canvas.setAttribute('aria-label', 'Máy bay 3D. Kéo để đổi góc nhìn; dùng các nút để chọn góc nhìn.');
  host.appendChild(canvas);
  const ctx = canvas.getContext('2d');
  if (!ctx) { host.textContent = 'Trình duyệt không hỗ trợ hiển thị mô hình.'; return; }
  const faces = [], rad = Math.PI / 180;
  const sub = (a,b) => a.map((v,i)=>v-b[i]);
  const dot = (a,b) => a.reduce((s,v,i)=>s+v*b[i],0);
  const cross = (a,b) => [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
  const unit = a => { const n=Math.hypot(...a)||1; return a.map(v=>v/n); };
  const paint = {
    hull:[126,148,159], upper:[173,190,195], lower:[79,101,113],
    panel:[106,128,139], dark:[28,43,53], glass:[26,91,121],
    glassEdge:[82,159,179], rim:[65,86,100], accent:[222,111,70],
    engine:[35,48,58], flame:[121,178,191], light:[225,233,225]
  };
  function face(v,c,shine=0) { faces.push({v,c,shine}); }
  function loft(rings, color, segments=28, shine=.2) {
    const rr=rings.map(([z,rx,ry,cy=0,cx=0])=>Array.from({length:segments},(_,i)=>{
      const a=i/segments*Math.PI*2; return [cx+Math.cos(a)*rx,cy+Math.sin(a)*ry,z];
    }));
    for(let j=0;j<rr.length-1;j++) for(let i=0;i<segments;i++) {
      const n=(i+1)%segments; face([rr[j][i],rr[j][n],rr[j+1][n],rr[j+1][i]],color,shine);
    }
    face([...rr[0]].reverse(),color,shine); face(rr[rr.length-1],color,shine);
  }
  function slab(points, thickness, color) {
    const a=points.map(p=>[p[0],p[1]+thickness/2,p[2]]), b=points.map(p=>[p[0],p[1]-thickness/2,p[2]]);
    face(a,color,.18);face([...b].reverse(),paint.dark,.1);
    for(let i=0;i<a.length;i++){const n=(i+1)%a.length;face([a[i],b[i],b[n],a[n]],paint.rim,.15);}
  }
  // Stealth-fighter silhouette. Nose points toward -Z; +Y is up.
  loft([[-3.65,.015,.015,-.04],[-3.1,.17,.12,-.04],[-2.25,.36,.23,-.025],
    [-1.25,.56,.32,0],[-.2,.66,.35,.02],[1.1,.64,.32,.01],
    [2.25,.53,.27,-.03],[2.8,.4,.19,-.05]],paint.hull,20,.3);
  // Faceted nose, cockpit collar and deep blue canopy.
  slab([[-.17,.115,-3.5],[.17,.115,-3.5],[.32,.235,-2.52],[-.32,.235,-2.52]],.016,paint.upper);
  loft([[-2.47,.015,.015,.21],[-2.16,.2,.15,.32],[-1.72,.28,.24,.43],
    [-1.12,.27,.24,.45],[-.72,.05,.045,.34]],paint.dark,20,.22);
  loft([[-2.38,.01,.01,.24],[-2.12,.18,.14,.35],[-1.7,.255,.225,.46],
    [-1.16,.245,.22,.47],[-.78,.025,.025,.36]],paint.glass,20,1);
  slab([[-.22,.475,-1.65],[.22,.475,-1.65],[.2,.46,-1.61],[-.2,.46,-1.61]],.008,paint.glassEdge);
  // A broad shoulder blends the fuselage into the swept wings.
  slab([[-.7,.17,-1.18],[.7,.17,-1.18],[1.05,.16,.15],
    [.7,.18,1.85],[-.7,.18,1.85],[-1.05,.16,.15]],.12,paint.upper);
  for(const s of [-1,1]) {
    const mirror=p=>p.map(([x,y,z])=>[s*x,y,z]);
    // Large diamond wings, separate control surfaces and small wingtip lights.
    slab(mirror([[.62,.10,-1.33],[1.12,.07,-1.22],[3.45,-.06,.38],
      [3.55,-.06,1.02],[2.36,.01,.93],[.74,.13,1.45]]),.12,paint.upper);
    slab(mirror([[1.67,.13,.52],[3.32,.00,.58],[3.46,.00,.98],
      [2.36,.08,.87]]),.012,paint.panel);
    slab(mirror([[2.06,.14,.84],[3.37,.01,.93],[3.42,.01,.99],
      [2.02,.14,.91]]),.008,paint.dark);
    slab(mirror([[.72,.22,-.65],[1.08,.18,-.58],[2.55,.08,.4],
      [2.39,.08,.45]]),.012,paint.rim);
    slab(mirror([[2.79,.02,.48],[3.40,-.05,.48],[3.45,-.05,.56],
      [2.90,.02,.57]]),.008,paint.accent);
    // Stealth tailplanes lie behind the main wing.
    slab(mirror([[.43,.04,1.64],[.91,.04,1.59],[2.1,-.02,2.39],
      [2.13,-.02,2.75],[.52,.06,2.49]]),.095,paint.hull);
    slab(mirror([[1.15,.11,2.31],[2.04,.03,2.45],[2.07,.03,2.67],
      [1.14,.11,2.53]]),.008,paint.panel);
    // Angled twin vertical stabilizers, with orange squadron markings.
    const fin=mirror([[.47,.24,1.18],[.81,1.12,1.92],
      [.89,1.15,2.55],[.56,.25,2.48]]);
    face(fin,paint.upper,.3);
    face(fin.map(([x,y,z])=>[x+s*.08,y,z]).reverse(),paint.lower,.2);
    for(let i=0;i<4;i++){const n=(i+1)%4;face([fin[i],fin[n],
      [fin[n][0]+s*.08,fin[n][1],fin[n][2]],
      [fin[i][0]+s*.08,fin[i][1],fin[i][2]]],paint.rim);}
    face(mirror([[.78,.99,1.97],[.81,1.11,2.07],
      [.88,1.14,2.47],[.84,.99,2.44]]),paint.accent,.15);
    // Black intake mouth and two separated engine tunnels.
    slab(mirror([[.48,.03,-.82],[.95,.03,-.8],[1.04,.02,-.15],
      [.49,.02,-.12]]),.12,paint.dark);
    loft([[-.54,.22,.17,-.20,s*.50],[.0,.3,.25,-.19,s*.53],
      [1.65,.31,.24,-.16,s*.53],[2.45,.25,.21,-.14,s*.53]],paint.lower,16);
    loft([[2.39,.27,.22,-.14,s*.53],[2.61,.23,.19,-.14,s*.53]],paint.rim,16,.4);
    loft([[2.62,.19,.15,-.14,s*.53],[2.65,.19,.15,-.14,s*.53]],paint.engine,16,.2);
    loft([[2.66,.105,.08,-.14,s*.53],[2.67,.105,.08,-.14,s*.53]],paint.flame,16,.7);
    loft([[.48,.045,.045,-.015,s*3.51],[.56,.035,.035,-.015,s*3.51]],
      s<0?[240,77,76]:[78,231,190],10,.8);
  }
  // A slim dorsal spine gives the body a continuous line behind the canopy.
  slab([[-.08,.385,-.72],[.08,.385,-.72],[.11,.34,1.5],
    [-.11,.34,1.5]],.014,paint.panel);
  let width=1,height=1, yawCam=.53,elev=.57,distance=11.5;
  let angles={roll:0,pitch:0,yaw:0};
  function rotate(p) {
    let [x,y,z]=p;const r=-angles.roll*rad,t=angles.pitch*rad,h=angles.yaw*rad;
    [x,y]=[x*Math.cos(r)-y*Math.sin(r),x*Math.sin(r)+y*Math.cos(r)];
    [y,z]=[y*Math.cos(t)-z*Math.sin(t),y*Math.sin(t)+z*Math.cos(t)];
    return [x*Math.cos(h)+z*Math.sin(h),y,-x*Math.sin(h)+z*Math.cos(h)];
  }
  function draw() {
    ctx.clearRect(0,0,width,height);
    const eye=[Math.sin(yawCam)*Math.cos(elev)*distance,Math.sin(elev)*distance,-Math.cos(yawCam)*Math.cos(elev)*distance];
    const f=unit(eye.map(v=>-v)),right=unit(cross(f,[0,1,0])),up=cross(right,f);
    const focal=Math.min(width*.95,height*1.2);
    function project(p){const a=sub(p,eye),z=dot(a,f);return [width/2+dot(a,right)*focal/z,height*.52-dot(a,up)*focal/z,z];}
    function path(points){ctx.beginPath();points.forEach((p,i)=>i?ctx.lineTo(p[0],p[1]):ctx.moveTo(p[0],p[1]));ctx.closePath();}
    // Ground reference stays still while aircraft follows the live orientation.
    ctx.lineWidth=.7;
    for(let n=-6;n<=6;n++) {
      for(const ends of [[[n,-1.35,-6],[n,-1.35,6]],[[-6,-1.35,n],[6,-1.35,n]]]) {
        const a=project(ends[0]),b=project(ends[1]);if(a[2]<1||b[2]<1)continue;
        ctx.strokeStyle=n===0?'rgba(98,222,198,.22)':'rgba(98,222,198,.07)';ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(b[0],b[1]);ctx.stroke();
      }
    }
    for(let i=12;i>0;i--){const k=i/12;path(Array.from({length:48},(_,j)=>project([Math.cos(j/48*Math.PI*2)*3*k,-1.33,Math.sin(j/48*Math.PI*2)*2*k])));ctx.fillStyle='rgba(0,0,0,.025)';ctx.fill();}
    // Fixed world axes: X spans the wings, Y points up, -Z points forward.
    const origin=[0,-1.31,0];
    function worldAxis(end,color,label){
      const a=project(origin),b=project(end);
      if(a[2]<=.1||b[2]<=.1)return;
      const dx=b[0]-a[0],dy=b[1]-a[1],len=Math.hypot(dx,dy)||1;
      const ux=dx/len,uy=dy/len;
      ctx.strokeStyle=color;ctx.fillStyle=color;ctx.lineWidth=2;
      ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(b[0],b[1]);ctx.stroke();
      ctx.beginPath();ctx.moveTo(b[0],b[1]);ctx.lineTo(b[0]-ux*10-uy*4,b[1]-uy*10+ux*4);
      ctx.lineTo(b[0]-ux*10+uy*4,b[1]-uy*10-ux*4);ctx.closePath();ctx.fill();
      ctx.font='700 12px ui-monospace, monospace';ctx.fillText(label,b[0]+ux*9,b[1]+uy*9);
    }
    worldAxis([4.25,-1.31,0],'#ff8b79','X');
    worldAxis([0,1.45,0],'#63e5b5','Y');
    worldAxis([0,-1.31,-4.25],'#7cbcff','Z');
    const o=project(origin);ctx.fillStyle='#e5f4f2';ctx.font='700 11px ui-monospace, monospace';
    ctx.fillText('O',o[0]-12,o[1]+14);
    const light=unit([-3,7,-4]);
    const items=faces.map(face=>{const v=face.v.map(rotate);const center=v[0].map((_,i)=>v.reduce((a,p)=>a+p[i],0)/v.length);let normal=unit(cross(sub(v[1],v[0]),sub(v[2],v[0])));const view=unit(sub(eye,center));if(dot(normal,view)<0)normal=normal.map(x=>-x);const diffuse=Math.max(0,dot(normal,light));const half=unit(light.map((v,i)=>v+view[i]));const spec=Math.pow(Math.max(0,dot(normal,half)),face.shine>.8?35:18)*face.shine;const color=face.c.map(c=>Math.min(255,Math.round(c*(.40+.60*diffuse)+spec*110)));return {p:v.map(project),depth:dot(sub(center,eye),f),color};}).sort((a,b)=>b.depth-a.depth);
    for(const item of items){if(item.p.some(p=>p[2]<.1))continue;path(item.p);ctx.fillStyle=`rgb(${item.color.join(',')})`;ctx.fill();ctx.strokeStyle=ctx.fillStyle;ctx.lineWidth=.45;ctx.stroke();}
    // Visual mounting reference, attached to the fuselage so it follows Roll/Pitch/Yaw.
    const mpu=project(rotate([0,.55,.25]));
    if(mpu[2]>.1){
      const [mx,my]=mpu;
      ctx.fillStyle='rgba(255,89,94,.23)';ctx.beginPath();ctx.arc(mx,my,12,0,Math.PI*2);ctx.fill();
      ctx.fillStyle='#ff595e';ctx.strokeStyle='#fff4ed';ctx.lineWidth=2;
      ctx.beginPath();ctx.arc(mx,my,5.5,0,Math.PI*2);ctx.fill();ctx.stroke();
      ctx.font='700 11px ui-monospace, monospace';ctx.fillStyle='#fff4ed';
      ctx.fillText('MPU',mx+12,my-9);
    }
    // Corner axis indicator stays visible even when the aircraft hides the origin.
    const gx=width-76,gy=height-78;
    ctx.fillStyle='rgba(5,23,27,.82)';ctx.strokeStyle='rgba(124,188,255,.28)';ctx.lineWidth=1;
    ctx.beginPath();ctx.arc(gx,gy,57,0,Math.PI*2);ctx.fill();ctx.stroke();
    for(const [label,vec,color] of [['X',[1,0,0],'#ff8b79'],['Y',[0,1,0],'#63e5b5'],['Z',[0,0,-1],'#7cbcff']]){
      const vx=dot(vec,right),vy=-dot(vec,up),length=Math.hypot(vx,vy)||1;
      const ex=gx+vx/length*36,ey=gy+vy/length*36;
      ctx.strokeStyle=color;ctx.fillStyle=color;ctx.lineWidth=2.5;
      ctx.beginPath();ctx.moveTo(gx,gy);ctx.lineTo(ex,ey);ctx.stroke();
      ctx.beginPath();ctx.arc(ex,ey,3,0,Math.PI*2);ctx.fill();
      ctx.font='700 12px ui-monospace, monospace';ctx.fillText(label,ex+5,ey-4);
    }
    ctx.fillStyle='#e5f4f2';ctx.font='700 10px ui-monospace, monospace';ctx.fillText('O',gx-4,gy+4);
  }
  function resize(){const rect=host.getBoundingClientRect();width=Math.max(1,rect.width);height=Math.max(1,rect.height);const dpr=Math.min(window.devicePixelRatio||1,2);canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);draw();}
  new ResizeObserver(resize).observe(host);
  let pointer=null;
  canvas.addEventListener('pointerdown',e=>{pointer={id:e.pointerId,x:e.clientX,y:e.clientY};canvas.setPointerCapture(e.pointerId);canvas.classList.add('dragging');});
  canvas.addEventListener('pointermove',e=>{if(!pointer||pointer.id!==e.pointerId)return;yawCam-=(e.clientX-pointer.x)*.008;elev=Math.max(-.5,Math.min(1.45,elev+(e.clientY-pointer.y)*.007));pointer.x=e.clientX;pointer.y=e.clientY;draw();});
  const release=()=>{pointer=null;canvas.classList.remove('dragging');};
  canvas.addEventListener('pointerup',release);canvas.addEventListener('pointercancel',release);canvas.addEventListener('lostpointercapture',release);
  document.querySelectorAll('[data-camera]').forEach(button=>button.addEventListener('click',()=>{const mode=button.dataset.camera;[yawCam,elev]=mode==='front'?[0,.12]:mode==='top'?[0,1.45]:[.60,.43];document.querySelectorAll('[data-camera]').forEach(b=>{const active=b===button;b.classList.toggle('active',active);b.setAttribute('aria-pressed',String(active));});draw();}));
  window.aircraftView={setAttitude(roll,pitch,yaw){if([roll,pitch,yaw].every(Number.isFinite)){angles={roll,pitch,yaw};draw();}}};
  resize();
})();

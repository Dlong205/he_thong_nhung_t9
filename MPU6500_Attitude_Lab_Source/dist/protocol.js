/* K03 telemetry.c: AA55, LEN=81, little endian, CRC8 over bytes 2..83. */
(function(root){
  'use strict';
  function crc8(bytes){let c=0;for(const b of bytes){c^=b;for(let i=0;i<8;i++)c=(c&128)?((c<<1)^7)&255:(c<<1)&255;}return c;}
  function decode(bytes){
    if(bytes.length!==85||bytes[0]!==170||bytes[1]!==85||bytes[2]!==81)throw Error('length');
    if(crc8(bytes.subarray(2,84))!==bytes[84])throw Error('crc');
    const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength), floats=[];
    for(let i=0;i<14;i++)floats.push(v.getFloat32(7+4*i,true));
    const extra=[v.getFloat32(67,true),v.getFloat32(71,true),v.getFloat32(76,true),v.getFloat32(80,true)];
    if(![...floats,...extra].every(Number.isFinite))throw Error('nonfinite');
    return {timestamp:v.getUint32(3,true),ax:floats[0],ay:floats[1],az:floats[2],gx:floats[3],gy:floats[4],gz:floats[5],
      acc:{roll:floats[6],pitch:floats[7]},gyro:{roll:floats[8],pitch:floats[9]},cf:{roll:floats[10],pitch:floats[11]},kalman:{roll:floats[12],pitch:floats[13]},
      exec_us:v.getUint16(63,true),dt_us:v.getUint16(65,true),fixed:{roll:extra[0],pitch:extra[1]},ref:v.getInt8(75),yaw_raw:extra[2],yaw_tilt:extra[3],yaw:extra[3],protocol:'binary'};
  }
  class Decoder {
    constructor(onPacket,onText=()=>{}){this.onPacket=onPacket;this.onText=onText;this.reset();}
    reset(){this.buffer=new Uint8Array();this.stats={crc:0,length:0,nonfinite:0,discarded:0,valid:0};}
    text(b){const s=new TextDecoder().decode(b).trim();if(s&&/^[\x20-\x7e\r\t]*$/.test(s))this.onText(s);else this.stats.discarded+=b.length;}
    push(chunk){
      const joined=new Uint8Array(this.buffer.length+chunk.length);joined.set(this.buffer);joined.set(chunk,this.buffer.length);let b=joined,pos=0;
      while(pos<b.length){
        if(b[pos]===170){
          if(b.length-pos<2)break;
          if(b[pos+1]!==85){this.stats.discarded++;pos++;continue;}
          if(b.length-pos<3)break;
          if(b[pos+2]!==81){this.stats.length++;pos++;continue;}
          if(b.length-pos<85)break;
          const frame=b.subarray(pos,pos+85);let packet;
          try{packet=decode(frame);}catch(e){this.stats[e.message]=(this.stats[e.message]||0)+1;pos++;continue;}
          this.stats.valid++;pos+=85;this.onPacket(packet);continue;
        }
        let end=pos;while(end<b.length&&b[end]!==10&&b[end]!==170)end++;
        if(end===b.length){if(end-pos>2048){this.stats.discarded+=end-pos;pos=end;}break;}
        this.text(b.subarray(pos,end));pos=b[end]===10?end+1:end;
      }
      this.buffer=b.slice(pos);
    }
  }
  function metrics(values,reference){if(!values.length)return null;const n=values.length,mean=values.reduce((a,b)=>a+b,0)/n;return {n,mean,bias:mean-reference,mae:values.reduce((a,b)=>a+Math.abs(b-reference),0)/n,rmse:Math.sqrt(values.reduce((a,b)=>a+(b-reference)**2,0)/n),std:n>1?Math.sqrt(values.reduce((a,b)=>a+(b-mean)**2,0)/(n-1)):0};}
  root.K03={Decoder,decode,crc8,metrics};
  if(typeof module!=='undefined')module.exports=root.K03;
})(typeof window!=='undefined'?window:globalThis);

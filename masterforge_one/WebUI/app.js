(() => {
'use strict';

const backend = window.__JUCE__ && window.__JUCE__.backend;
const $ = (s, r=document) => r.querySelector(s);
const $$ = (s, r=document) => Array.from(r.querySelectorAll(s));
const clamp = (v,a,b) => Math.max(a, Math.min(b,v));

let state = { params:{}, pre:Array(64).fill(-90), post:Array(64).fill(-90), meters:{}, presets:[], presetIndex:0 };
let selectedModule = 0;
let selectedEqBand = 2;
let eqDrag = null;
let eqHover = -1;

const defs = [
 ['input','INPUT / LEVEL','Gain staging','smartGain','Сначала выставьте здоровый вход. Smart Gain работает плавно и не заменяет нормальный gain staging.',
  'Входной блок готовит микс к мастеринг-цепи. INPUT TRIM — ручной уровень, TARGET RMS — цель Smart Gain, SPEED — скорость адаптации, RANGE — предел автоматической коррекции.',
  [['inputTrim','INPUT TRIM','db','Ручной входной уровень до всей цепи.'],['targetInput','TARGET RMS','db','Целевой средний уровень Smart Gain.'],['smartSpeed','SMART SPEED','percent','Скорость автоматической коррекции уровня.'],['smartMaxGain','GAIN RANGE','db','Максимальная коррекция Smart Gain вверх или вниз.']]],
 ['eq','PARAMETRIC EQ','Graphical 6-band','cleanEqOn','Тяните точки прямо на графике. Горизонталь — частота, вертикаль — gain. Колесо над bell-полосой меняет Q.',
  'Шестиполосный графический параметрический EQ. LOW и AIR — shelf, остальные четыре точки — bell. Перетаскивайте точки, колесом меняйте Q, двойным кликом сбрасывайте полосу.',[]],
 ['dynamic','DYNAMIC EQ','Adaptive control','dynamicEqOn','Динамический EQ работает только когда низ или верх действительно выпирают.',
  'Адаптивный контроль низа и верха. AMOUNT задаёт глубину, THRESHOLD чувствительность, ATTACK/RELEASE скорость реакции, XOVER границы зон.',
  [['dynamicEq','AMOUNT','percent','Общая глубина динамического контроля.'],['dynThreshold','THRESHOLD','db','Порог срабатывания динамического EQ.'],['dynAttack','ATTACK','ms','Скорость реакции на всплеск.'],['dynRelease','RELEASE','ms','Время возврата после срабатывания.'],['dynLowHz','LOW XOVER','hz','Граница низкой динамической зоны.'],['dynHighHz','HIGH XOVER','hz','Граница верхней динамической зоны.']]],
 ['stabilizer','STABILIZER','Resonance control','resonanceOn','Найдите проблемную частоту и добавляйте AMOUNT только до исчезновения резкости.',
  'Узкополосный стабилизатор. FREQUENCY задаёт проблемную частоту, Q ширину, AMOUNT глубину подавления.',
  [['resonance','AMOUNT','percent','Глубина подавления резонанса.'],['resonanceHz','FREQUENCY','hz','Центральная частота резонанса.'],['resonanceQ','Q','number','Ширина коррекции. Выше Q — уже полоса.']]],
 ['comp','VINTAGE COMP','Bus glue','glueOn','Для ударного материала оставляйте атаку медленнее. MIX даёт параллельную склейку.',
  'Stereo bus-компрессор. THRESHOLD и RATIO определяют компрессию, ATTACK сохраняет транзиенты, RELEASE задаёт дыхание, MAKEUP возвращает уровень, MIX смешивает dry/wet.',
  [['glue','AMOUNT','percent','Общая интенсивность glue-обработки.'],['glueThreshold','THRESHOLD','db','Порог компрессора.'],['glueRatio','RATIO','ratio','Степень компрессии.'],['glueAttack','ATTACK','ms','Время атаки.'],['glueRelease','RELEASE','ms','Время восстановления.'],['glueMakeup','MAKEUP','db','Компенсационное усиление.'],['glueMix','MIX','percent','Параллельный dry/wet компрессора.']]],
 ['multiband','MULTIBAND','Three-zone density','multibandOn','Уплотняйте проблемную область, а не весь микс одинаково.',
  'Трёхполосная динамика. XOVER делят сигнал на зоны. GLOBAL — общая сила, LOW/MID/HIGH — интенсивность каждой полосы.',
  [['multiband','GLOBAL','percent','Общая сила многополосной обработки.'],['mbLowHz','LOW XOVER','hz','Граница низкой полосы.'],['mbHighHz','HIGH XOVER','hz','Граница верхней полосы.'],['mbLowAmount','LOW','percent','Плотность низкой полосы.'],['mbMidAmount','MID','percent','Плотность средней полосы.'],['mbHighAmount','HIGH','percent','Плотность верхней полосы.']]],
 ['impact','IMPACT','Transient design','impactOn','Добавляйте punch после компрессии, но не перегружайте финальный клиппер.',
  'Транзиентный модуль. PUNCH — сила атаки, SPEED — скорость детектора, MIX — количество обработанного сигнала.',
  [['impact','PUNCH','percent','Сила выделения транзиентов.'],['impactSpeed','SPEED','percent','Скорость детектора транзиентов.'],['impactMix','MIX','percent','Доля транзиентной обработки.']]],
 ['saturation','SATURATION','Harmonic density','analogOn','На мастер-шине небольшая сатурация обычно звучит дороже сильной.',
  'Мягкая гармоническая сатурация. DRIVE добавляет гармоники, TONE определяет яркость, MIX подмешивает эффект параллельно.',
  [['analog','DRIVE','percent','Количество гармонической сатурации.'],['analogTone','TONE','percent','Тон окраски: левее темнее, правее ярче.'],['analogMix','MIX','percent','Параллельное смешивание сатурации.']]],
 ['exciter','EXCITER','Upper harmonics','exciterOn','Exciter создаёт гармоники, а не просто поднимает верх.',
  'Гармонический exciter. FREQUENCY задаёт область, AMOUNT интенсивность гармоник, MIX долю эффекта.',
  [['exciter','AMOUNT','percent','Количество верхних гармоник.'],['exciterHz','FREQUENCY','hz','Рабочая частота exciter.'],['exciterMix','MIX','percent','Количество эффекта в сигнале.']]],
 ['lowend','LOW END FOCUS','Mono-safe bass','bassMonoOn','Саб в центре обычно переводится стабильнее на разных системах.',
  'Низкочастотный фокус. MONO BELOW задаёт границу, AMOUNT степень центровки баса.',
  [['bassMonoHz','MONO BELOW','hz','Частоты ниже этой точки постепенно центрируются.'],['bassMonoAmount','AMOUNT','percent','Степень моно-центровки низа.']]],
 ['imager','IMAGER','Three-band width','imagerOn','Низ держите ближе к центру. SAFETY защищает от плохой корреляции.',
  'Трёхполосный stereo imager. WIDTH задаёт ширину зон, XOVER границы, SAFETY уменьшает опасное расширение.',
  [['widthLow','LOW WIDTH','width','Ширина низкой полосы. 100% — исходная.'],['widthMid','MID WIDTH','width','Ширина средней полосы.'],['widthHigh','HIGH WIDTH','width','Ширина верхней полосы.'],['imagerLowHz','LOW XOVER','hz','Граница низкой полосы.'],['imagerHighHz','HIGH XOVER','hz','Граница верхней полосы.'],['imagerSafety','SAFETY','percent','Защита по фазовой корреляции.']]],
 ['clipper','CLIPPER 8X','Peak shaving','clipperOn','Клиппер должен снимать только короткие пики до лимитера.',
  '8x oversampled soft clipper. DRIVE подаёт сигнал, CEILING задаёт границу, SHAPE мягкость, MIX долю обработки.',
  [['clipDrive','DRIVE','db','Предусиление перед клиппером.'],['clipCeiling','CEILING','db2','Рабочий потолок клиппера.'],['clipShape','SHAPE','percent','Мягкость или жёсткость клиппинга.'],['clipMix','MIX','percent','Доля клиппированного сигнала.']]],
 ['maximizer','MAXIMIZER 8X','Look-ahead final level','limiterOn','DRIVE — главный регулятор громкости. Большой GR означает, что цепь передавлена.',
  'Финальный stereo-linked look-ahead limiter. DRIVE задаёт громкость, CEILING потолок, RELEASE скорость восстановления.',
  [['limiterDrive','DRIVE','db','Основной регулятор финальной громкости.'],['ceiling','CEILING','db2','Финальный выходной потолок.'],['limiterRelease','RELEASE','ms','Скорость восстановления лимитера.']]],
 ['output','OUTPUT','Final trim & dither',null,'Dither обычно нужен только при финальном экспорте с уменьшением битности.',
  'Финальный выход. OUTPUT TRIM согласует уровень, MASTER MIX смешивает dry/wet. DITHER выключен по умолчанию и включается только когда действительно нужен.',
  [['outputTrim','OUTPUT TRIM','db','Финальная коррекция уровня.'],['dryWet','MASTER MIX','percent','Глобальный dry/wet основной цепи.'],['ditherOn','DITHER 24-BIT','toggle','TPDF dither в самом конце. Включайте только при необходимости.']]]
];

const modules = defs.map((d,i) => ({
 id:d[0],title:d[1],sub:d[2],toggle:d[3],footer:d[4],help:d[5],
 special:i===1?'eq':null,
 params:d[6].map(x => ({id:x[0],label:x[1],unit:x[2],type:x[2]==='toggle'?'toggle':null,tip:x[3]}))
}));

const eqBands = [
 {name:'LOW',freq:'lowShelfHz',gain:'lowShelf',q:null,color:'#39d7ff'},
 {name:'LOW MID',freq:'lowMidHz',gain:'lowMid',q:'lowMidQ',color:'#64e6b1'},
 {name:'MID',freq:'midHz',gain:'midGain',q:'midQ',color:'#ffd06a'},
 {name:'PRESENCE',freq:'presenceHz',gain:'presence',q:'presenceQ',color:'#ff8f79'},
 {name:'HIGH MID',freq:'highMidHz',gain:'highMidGain',q:'highMidQ',color:'#c18cff'},
 {name:'AIR',freq:'airHz',gain:'air',q:null,color:'#8ceaff'}
];

const emit = (id,payload) => { if(backend) backend.emitEvent(id,payload); };
const p = id => state.params[id] || {norm:0,raw:0,def:0,defRaw:0,text:''};

function fmt(spec,raw){
 if(!Number.isFinite(raw)) return '—';
 if(spec.unit==='db') return raw.toFixed(1)+' dB';
 if(spec.unit==='db2') return raw.toFixed(2)+' dB';
 if(spec.unit==='hz') return raw>=1000?(raw/1000).toFixed(raw>=10000?1:2)+' kHz':Math.round(raw)+' Hz';
 if(spec.unit==='ms') return (raw<10?raw.toFixed(1):Math.round(raw))+' ms';
 if(spec.unit==='percent') return Math.round(raw*100)+' %';
 if(spec.unit==='width') return Math.round(raw*100)+' %';
 if(spec.unit==='ratio') return raw.toFixed(1)+' : 1';
 if(spec.unit==='number') return raw.toFixed(2);
 return raw.toFixed(2);
}
function setNorm(id,v){ v=clamp(v,0,1); if(state.params[id]) state.params[id].norm=v; emit('setParam',{id:id,norm:v}); }
function setRaw(id,v){ if(state.params[id]) state.params[id].raw=v; emit('setParam',{id:id,raw:v}); }
function gesture(id,phase){ emit('gesture',{id:id,phase:phase}); }

function renderPresets(){
 const s=$('#presetSelect');
 if(!state.presets.length) return;
 if(s.options.length!==state.presets.length){
   s.innerHTML='';
   state.presets.forEach((n,i)=>{ const o=document.createElement('option'); o.value=i; o.textContent=n; s.appendChild(o); });
 }
 s.value=String(state.presetIndex);
}
function renderChain(){
 const root=$('#chain'); root.innerHTML='';
 modules.forEach((m,i)=>{
  const on=m.toggle?p(m.toggle).norm>=.5:true;
  const el=document.createElement('div');
  el.className='chain-item'+(i===selectedModule?' active':'')+(on?' enabled':'');
  const tip=m.toggle?'Нажмите на индикатор, чтобы быстро включить или выключить модуль.':'Выходной блок всегда доступен.';
  el.innerHTML='<div class="chain-led" data-tip="'+tip+'"></div><div class="chain-copy"><div class="chain-title">'+m.title+'</div><div class="chain-sub">'+m.sub+'</div></div>';
  el.onclick=e=>{
   if(e.target.closest('.chain-led')&&m.toggle){ setNorm(m.toggle,on?0:1); return; }
   selectedModule=i; renderChain(); renderModule(); draw();
  };
  root.appendChild(el);
 });
}
function makeKnob(spec){
 const c=document.createElement('div'); c.className='control'; c.dataset.param=spec.id; c.dataset.tip=spec.tip;
 const l=document.createElement('div'); l.className='control-label'; l.textContent=spec.label;
 const w=document.createElement('div'); w.className='knob-wrap';
 const k=document.createElement('div'); k.className='knob'; k.style.setProperty('--p',(clamp(p(spec.id).norm,0,1)*270)+'deg'); w.appendChild(k);
 const v=document.createElement('div'); v.className='control-value'; v.textContent=fmt(spec,p(spec.id).raw);
 c.append(l,w,v);
 let sy=0,sn=0,drag=false;
 w.onpointerdown=e=>{ e.preventDefault();drag=true;sy=e.clientY;sn=p(spec.id).norm;w.setPointerCapture(e.pointerId);gesture(spec.id,'begin'); };
 w.onpointermove=e=>{ if(!drag)return;const n=clamp(sn+(sy-e.clientY)/150,0,1);k.style.setProperty('--p',(n*270)+'deg');setNorm(spec.id,n); };
 const end=e=>{ if(!drag)return;drag=false;try{w.releasePointerCapture(e.pointerId)}catch(_){}gesture(spec.id,'end'); };
 w.onpointerup=end; w.onpointercancel=end;
 w.ondblclick=e=>{ e.preventDefault();setNorm(spec.id,p(spec.id).def); };
 return c;
}
function makeToggle(spec){
 const c=document.createElement('div'); c.className='control toggle-control'; c.dataset.tip=spec.tip;
 const l=document.createElement('div'); l.className='control-label'; l.textContent=spec.label;
 const t=document.createElement('div'); t.className='big-toggle'+(p(spec.id).norm>=.5?' on':'');
 const v=document.createElement('div'); v.className='control-value'; v.textContent=p(spec.id).norm>=.5?'ON':'OFF';
 t.onclick=()=>setNorm(spec.id,p(spec.id).norm>=.5?0:1);
 c.append(l,t,v); return c;
}
function renderEqPanel(root){
 root.classList.add('eq-mode');
 const wrap=document.createElement('div'); wrap.className='eq-module-panel';
 const list=document.createElement('div'); list.className='eq-band-list';
 eqBands.forEach((b,i)=>{
  const f=p(b.freq).raw,g=p(b.gain).raw;
  const item=document.createElement('div'); item.className='eq-band'+(i===selectedEqBand?' active':'');
  item.dataset.tip=b.name+': выберите полосу, затем тяните точку на графике.'+(b.q?' Колесо мыши меняет Q.':' Shelf-полоса.');
  item.innerHTML='<div class="eq-band-top"><div class="eq-band-name">'+b.name+'</div><div class="eq-band-dot" style="background:'+b.color+';box-shadow:0 0 8px '+b.color+'88"></div></div>'+
   '<div class="eq-band-values"><div><span>FREQ</span><b>'+(f>=1000?(f/1000).toFixed(2)+'k':Math.round(f)+' Hz')+'</b></div><div><span>GAIN</span><b>'+(g>=0?'+':'')+g.toFixed(1)+' dB</b></div></div>';
  item.onclick=()=>{selectedEqBand=i;renderModule();draw();}; list.appendChild(item);
 });
 const b=eqBands[selectedEqBand], info=document.createElement('div'); info.className='eq-instructions';
 const qt=b.q?'Q: '+p(b.q).raw.toFixed(2):'SHELF BAND';
 info.innerHTML='<h3>'+b.name+' · '+qt+'</h3><p>Тяните точку на большом графике: влево/вправо — частота, вверх/вниз — gain. '+(b.q?'Колесо мыши меняет ширину Q.':'Ширина shelf настроена под мастеринг.')+'</p>'+
  '<div class="keys"><span class="key">DRAG = FREQ + GAIN</span>'+(b.q?'<span class="key">WHEEL = Q</span>':'')+'<span class="key">DOUBLE CLICK = RESET</span></div>';
 wrap.append(list,info); root.appendChild(wrap);
}
function renderModule(){
 const m=modules[selectedModule];
 $('#moduleTitle').textContent=m.title; $('#moduleKicker').textContent=m.sub.toUpperCase(); $('#moduleFooter').textContent=m.footer;
 const power=$('#modulePower');
 if(m.toggle){ power.style.display='';power.classList.toggle('on',p(m.toggle).norm>=.5);power.dataset.tip='Включить или выключить модуль '+m.title+'.';power.onclick=()=>setNorm(m.toggle,p(m.toggle).norm>=.5?0:1); }
 else{ power.style.display='none';power.onclick=null; }
 $('#helpBtn').onclick=()=>openHelp(m);
 const root=$('#moduleControls');root.className='controls-grid';root.innerHTML='';
 if(m.special==='eq') renderEqPanel(root); else m.params.forEach(s=>root.appendChild(s.type==='toggle'?makeToggle(s):makeKnob(s)));
}
function openHelp(m){ $('#modalTitle').textContent=m.title;$('#modalText').textContent=m.help;$('#modal').classList.add('open');$('#modal').setAttribute('aria-hidden','false'); }
function closeHelp(){ $('#modal').classList.remove('open');$('#modal').setAttribute('aria-hidden','true'); }

function syncControls(){
 const m=modules[selectedModule];
 if(m.toggle) $('#modulePower').classList.toggle('on',p(m.toggle).norm>=.5);
 $$('.control[data-param]').forEach(c=>{
  const s=m.params.find(x=>x.id===c.dataset.param);if(!s)return;
  const k=$('.knob',c),v=$('.control-value',c);if(k)k.style.setProperty('--p',(clamp(p(s.id).norm,0,1)*270)+'deg');if(v)v.textContent=fmt(s,p(s.id).raw);
 });
 $$('.toggle-control').forEach(c=>{
  const label=$('.control-label',c).textContent,s=m.params.find(x=>x.label===label);if(!s)return;
  const on=p(s.id).norm>=.5;$('.big-toggle',c).classList.toggle('on',on);$('.control-value',c).textContent=on?'ON':'OFF';
 });
 if(m.special==='eq') renderModule();
}
function updateBypass(){ $('#bypassBtn').classList.toggle('on',p('masterBypass').norm>=.5); }
function dbMeter(db){ return Number.isFinite(db)?clamp((db+60)/60,0,1):0; }
function dbText(db,suffix){ return Number.isFinite(db)&&db>-99?db.toFixed(1)+(suffix||''):'-∞'; }
function updateMeters(){
 const m=state.meters||{},ip=Number(m.inputPeak??-100),op=Number(m.outputPeak??-100);
 $('#inMeter').style.height=(dbMeter(ip)*100)+'%';$('#outMeter').style.height=(dbMeter(op)*100)+'%';
 $('#inPeak').textContent=dbText(ip,' dBFS');$('#outPeak').textContent=dbText(op,' dBFS');
 $('#lufs').textContent=dbText(Number(m.lufs??-100),'');$('#crest').textContent=Number(m.crest??0).toFixed(1)+' dB';
 $('#corr').textContent=Number(m.correlation??0).toFixed(2);$('#gr').textContent=Number(m.limiterGR??0).toFixed(1)+' dB';$('#inputGain').textContent=Number(m.inputGain??0).toFixed(1)+' dB';
 const corr=Number(m.correlation??0),gr=Number(m.limiterGR??0),badge=$('#qualityBadge');
 if(corr<-.05){badge.textContent='PHASE';badge.style.color='var(--red)';}
 else if(gr>7){badge.textContent='HARD';badge.style.color='var(--amber)';}
 else{badge.textContent='CLEAN';badge.style.color='var(--green)';}
 const rms=Number(m.inputRms??-100),coach=$('#coach');coach.classList.remove('warn','hot');
 if(rms<-60)$('#coachText').textContent='Ожидаю аудиосигнал…';
 else if(rms<-24){coach.classList.add('warn');$('#coachText').textContent='Вход тихий. Добавьте уровень или оставьте Smart Gain включённым.';}
 else if(rms>-10){coach.classList.add('hot');$('#coachText').textContent='Вход слишком горячий. Снизьте уровень, чтобы сохранить транзиенты.';}
 else $('#coachText').textContent='Вход в рабочей зоне. Цепь получает нормальный запас для плотного мастеринга.';
}

const canvas=$('#eqCanvas'),ctx=canvas.getContext('2d');
function resizeCanvas(){
 const r=canvas.getBoundingClientRect(),d=Math.max(1,window.devicePixelRatio||1),w=Math.max(10,Math.round(r.width*d)),h=Math.max(10,Math.round(r.height*d));
 if(canvas.width!==w||canvas.height!==h){canvas.width=w;canvas.height=h;}ctx.setTransform(d,0,0,d,0,0);draw();
}
function gr(){const r=canvas.getBoundingClientRect();return{x:18,y:14,w:Math.max(10,r.width-36),h:Math.max(10,r.height-35)};}
function fNorm(f){return Math.log(clamp(f,20,20000)/20)/Math.log(1000);}
function nFreq(n){return 20*Math.pow(1000,clamp(n,0,1));}
function fx(f,g){return g.x+fNorm(f)*g.w;}
function gy(v,g){return g.y+g.h*(.5-clamp(v,-12,12)/24);}
function sy(db,g){return g.y+g.h*(1-clamp((db+84)/84,0,1));}
function bell(f,c,q){const w=1.45-clamp((q-.25)/3.75,0,1)*1.2,x=Math.log2(f/Math.max(20,c))/w;return Math.exp(-.5*x*x);}
function eqAt(f){
 let v=(p('lowShelf').raw||0)/(1+Math.pow(f/Math.max(30,p('lowShelfHz').raw||105),3));
 v+=(p('lowMid').raw||0)*bell(f,p('lowMidHz').raw||320,p('lowMidQ').raw||.85);
 v+=(p('midGain').raw||0)*bell(f,p('midHz').raw||900,p('midQ').raw||.9);
 v+=(p('presence').raw||0)*bell(f,p('presenceHz').raw||3200,p('presenceQ').raw||.9);
 v+=(p('highMidGain').raw||0)*bell(f,p('highMidHz').raw||6200,p('highMidQ').raw||.9);
 v+=(p('air').raw||0)/(1+Math.pow(Math.max(3000,p('airHz').raw||10500)/Math.max(20,f),4));return v;
}
function spectrum(a,g,color,width,fill){
 if(!a||!a.length)return;ctx.beginPath();a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
 if(fill){ctx.lineTo(g.x+g.w,g.y+g.h);ctx.lineTo(g.x,g.y+g.h);ctx.closePath();const z=ctx.createLinearGradient(0,g.y,0,g.y+g.h);z.addColorStop(0,'rgba(57,215,255,.16)');z.addColorStop(1,'rgba(57,215,255,0)');ctx.fillStyle=z;ctx.fill();ctx.beginPath();a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});}
 ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();
}
function draw(){
 const r=canvas.getBoundingClientRect();if(!r.width||!r.height)return;const g=gr();ctx.clearRect(0,0,r.width,r.height);
 const bg=ctx.createLinearGradient(0,g.y,0,g.y+g.h);bg.addColorStop(0,'rgba(19,31,42,.52)');bg.addColorStop(1,'rgba(4,8,12,.1)');ctx.fillStyle=bg;ctx.fillRect(g.x,g.y,g.w,g.h);
 ctx.font='8px Segoe UI';ctx.textBaseline='bottom';
 [20,50,100,200,500,1000,2000,5000,10000,20000].forEach(f=>{const x=fx(f,g);ctx.strokeStyle='rgba(69,95,117,.26)';ctx.beginPath();ctx.moveTo(x,g.y);ctx.lineTo(x,g.y+g.h);ctx.stroke();ctx.fillStyle='rgba(100,122,142,.65)';ctx.textAlign=f===20?'left':f===20000?'right':'center';ctx.fillText(f>=1000?(f/1000)+'k':String(f),x,g.y+g.h-3);});
 [-12,-6,0,6,12].forEach(db=>{const y=gy(db,g);ctx.strokeStyle=db===0?'rgba(108,139,162,.35)':'rgba(69,95,117,.18)';ctx.beginPath();ctx.moveTo(g.x,y);ctx.lineTo(g.x+g.w,y);ctx.stroke();});
 spectrum(state.pre,g,'rgba(119,139,158,.42)',1,false);spectrum(state.post,g,'rgba(57,215,255,.88)',1.6,true);
 ctx.beginPath();const n=Math.max(240,Math.floor(g.w));for(let i=0;i<n;i++){const u=i/(n-1),x=g.x+u*g.w,y=gy(eqAt(nFreq(u)),g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);}const grad=ctx.createLinearGradient(g.x,0,g.x+g.w,0);grad.addColorStop(0,'#39d7ff');grad.addColorStop(.5,'#9f8bff');grad.addColorStop(1,'#8ceaff');ctx.strokeStyle=grad;ctx.lineWidth=2.2;ctx.stroke();
 if(modules[selectedModule].id==='eq')eqBands.forEach((b,i)=>{const x=fx(p(b.freq).raw,g),y=gy(p(b.gain).raw,g),a=i===selectedEqBand,h=i===eqHover;ctx.beginPath();ctx.arc(x,y,a?8:6,0,Math.PI*2);ctx.fillStyle=a?b.color:'#0b1219';ctx.fill();ctx.strokeStyle=b.color;ctx.lineWidth=a||h?2.2:1.5;ctx.stroke();if(a){ctx.beginPath();ctx.arc(x,y,14,0,Math.PI*2);ctx.strokeStyle=b.color+'55';ctx.stroke();}});
 renderEqInspector();
}
function renderEqInspector(){
 const b=eqBands[selectedEqBand],root=$('#eqInspector'),f=p(b.freq).raw,g=p(b.gain).raw,q=b.q?p(b.q).raw:null;
 root.innerHTML='<div class="eq-chip"><span>BAND</span><b>'+b.name+'</b></div><div class="eq-chip"><span>FREQ</span><b>'+(f>=1000?(f/1000).toFixed(2)+' kHz':Math.round(f)+' Hz')+'</b></div><div class="eq-chip"><span>GAIN</span><b>'+(g>=0?'+':'')+g.toFixed(1)+' dB</b></div>'+(q!==null?'<div class="eq-chip"><span>Q</span><b>'+q.toFixed(2)+'</b></div>':'');
}
function hit(cx,cy){
 if(modules[selectedModule].id!=='eq')return-1;const rect=canvas.getBoundingClientRect(),g=gr(),x=cx-rect.left,y=cy-rect.top;let bi=-1,bd=1e9;
 eqBands.forEach((b,i)=>{const d=Math.hypot(x-fx(p(b.freq).raw,g),y-gy(p(b.gain).raw,g));if(d<bd){bd=d;bi=i;}});return bd<=20?bi:-1;
}
canvas.onpointerdown=e=>{const i=hit(e.clientX,e.clientY);if(i<0)return;e.preventDefault();selectedEqBand=i;const b=eqBands[i];eqDrag={band:i,id:e.pointerId};canvas.setPointerCapture(e.pointerId);gesture(b.freq,'begin');gesture(b.gain,'begin');renderModule();draw();};
canvas.onpointermove=e=>{const i=hit(e.clientX,e.clientY);eqHover=i;if(eqDrag){const rect=canvas.getBoundingClientRect(),g=gr(),x=clamp(e.clientX-rect.left,g.x,g.x+g.w),y=clamp(e.clientY-rect.top,g.y,g.y+g.h),b=eqBands[eqDrag.band];setRaw(b.freq,nFreq((x-g.x)/g.w));setRaw(b.gain,clamp((.5-(y-g.y)/g.h)*24,-12,12));draw();return;}if(i>=0){const b=eqBands[i];showTip(b.name+': тяните точку для частоты и gain.'+(b.q?' Колесо мыши меняет Q.':' Shelf-полоса.'),e.clientX+15,e.clientY+15);}else hideTip();draw();};
function endEq(){if(!eqDrag)return;const b=eqBands[eqDrag.band];gesture(b.freq,'end');gesture(b.gain,'end');eqDrag=null;}
canvas.onpointerup=endEq;canvas.onpointercancel=endEq;
canvas.addEventListener('wheel',e=>{const i=hit(e.clientX,e.clientY);if(i<0)return;const b=eqBands[i];if(!b.q)return;e.preventDefault();selectedEqBand=i;setRaw(b.q,clamp((p(b.q).raw||.9)*(e.deltaY<0?1.1:.91),.25,4));renderModule();draw();},{passive:false});
canvas.ondblclick=e=>{const i=hit(e.clientX,e.clientY);if(i<0)return;const b=eqBands[i];setRaw(b.freq,p(b.freq).defRaw);setRaw(b.gain,p(b.gain).defRaw);if(b.q)setRaw(b.q,p(b.q).defRaw);selectedEqBand=i;renderModule();draw();};

function showTip(t,x,y){const z=$('#tooltip');z.textContent=t;z.style.left=Math.min(window.innerWidth-310,x)+'px';z.style.top=Math.min(window.innerHeight-90,y)+'px';z.classList.add('show');}
function hideTip(){$('#tooltip').classList.remove('show');}
document.addEventListener('pointermove',e=>{if(e.target===canvas)return;const el=e.target.closest&&e.target.closest('[data-tip]');if(el&&el.dataset.tip)showTip(el.dataset.tip,e.clientX+14,e.clientY+14);else hideTip();});
document.addEventListener('pointerleave',hideTip);

$('#presetSelect').onchange=e=>emit('preset',Number(e.target.value));
$('#bypassBtn').onclick=()=>setNorm('masterBypass',p('masterBypass').norm>=.5?0:1);
$('#modalClose').onclick=closeHelp;$('.modal-backdrop').onclick=closeHelp;document.addEventListener('keydown',e=>{if(e.key==='Escape')closeHelp();});

function applyState(s){if(!s)return;state=s;renderPresets();updateBypass();updateMeters();syncControls();renderChain();draw();}
if(backend){backend.addEventListener('state',applyState);emit('uiReady',{ready:true});}
else{
 const ids=new Set(['masterBypass']);modules.forEach(m=>{if(m.toggle)ids.add(m.toggle);m.params.forEach(x=>ids.add(x.id));});eqBands.forEach(b=>{ids.add(b.freq);ids.add(b.gain);if(b.q)ids.add(b.q);});
 ids.forEach(id=>state.params[id]={norm:.5,raw:0,def:.5,defRaw:0});
 Object.assign(state.params,{
  lowShelfHz:{norm:.38,raw:105,def:.38,defRaw:105},lowShelf:{norm:.5,raw:0,def:.5,defRaw:0},
  lowMidHz:{norm:.4,raw:320,def:.4,defRaw:320},lowMid:{norm:.5,raw:-.6,def:.5,defRaw:-.6},lowMidQ:{norm:.2,raw:.85,def:.2,defRaw:.85},
  midHz:{norm:.48,raw:900,def:.48,defRaw:900},midGain:{norm:.5,raw:0,def:.5,defRaw:0},midQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
  presenceHz:{norm:.62,raw:3200,def:.62,defRaw:3200},presence:{norm:.55,raw:.7,def:.55,defRaw:.7},presenceQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
  highMidHz:{norm:.7,raw:6200,def:.7,defRaw:6200},highMidGain:{norm:.5,raw:0,def:.5,defRaw:0},highMidQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
  airHz:{norm:.78,raw:10500,def:.78,defRaw:10500},air:{norm:.55,raw:.8,def:.55,defRaw:.8},smartGain:{norm:1,raw:1,def:1,defRaw:1},cleanEqOn:{norm:1,raw:1,def:1,defRaw:1}
 });
 state.presets=['Boom Bap - DENSE PUNCH','Boom Bap - DUSTY ANALOG','Hip-Hop - MODERN DENSE'];state.meters={inputPeak:-12.4,outputPeak:-.8,inputRms:-18.2,lufs:-9.4,crest:8.7,correlation:.72,limiterGR:2.8,inputGain:1.4};
 state.pre=Array.from({length:64},(_,i)=>-42+9*Math.sin(i*.13)-i*.16);state.post=Array.from({length:64},(_,i)=>-35+7*Math.sin(i*.12)-i*.13);
 renderPresets();renderChain();renderModule();updateBypass();updateMeters();draw();
}
if(window.ResizeObserver)new ResizeObserver(resizeCanvas).observe(canvas);
window.addEventListener('resize',resizeCanvas);
requestAnimationFrame(()=>{renderChain();renderModule();resizeCanvas();});
})();
(() => {
'use strict';

const backend = window.__JUCE__ && window.__JUCE__.backend;
const $ = (s, r=document) => r.querySelector(s);
const $$ = (s, r=document) => Array.from(r.querySelectorAll(s));
const clamp = (v,a,b) => Math.max(a, Math.min(b,v));
const emit = (name,payload) => { if (backend) backend.emitEvent(name,payload); };

let state = {
  params:{}, pre:Array(64).fill(-90), post:Array(64).fill(-90),
  meters:{}, presets:[], presetIndex:0,
  chain:['eq','dynamic','stabilizer','comp','multiband','impact','saturation','exciter','lowend','imager','clipper','maximizer']
};

let selectedId = 'input';
let selectedEqBand = 2;
let eqDrag = null;
let eqHover = -1;
let dragChainId = null;
let uiBuilt = false;
let chainSignature = '';
let pickerSignature = '';
const heldParams = new Map();

const C = (id,label,unit,type='knob') => ({id,label,unit,type});

const modules = [
  {id:'input',fixed:true,title:'INPUT / LEVEL',sub:'Gain staging',toggle:'smartGain',
   footer:'Сначала выставьте чистый вход. Если сигнал уже горячий, лучше убрать INPUT TRIM, чем заставлять остальные блоки постоянно давить.',
   help:'Входной блок задаёт рабочий уровень перед всей цепочкой. INPUT TRIM — ручной gain. TARGET RMS — цель Smart Gain. SMART SPEED — скорость адаптации. GAIN RANGE — предел автоматической коррекции.',
   controls:[
     C('inputTrim','INPUT TRIM','db','knob'), C('targetInput','TARGET RMS','db','knob'),
     C('smartSpeed','SMART SPEED','percent','knob'), C('smartMaxGain','GAIN RANGE','db','knob')
   ]},

  {id:'eq',title:'PARAMETRIC EQ',sub:'Graphical 6-band',toggle:'cleanEqOn',special:'eq',
   footer:'График используется только здесь. Перетаскивайте точки, колесом меняйте Q, двойным кликом сбрасывайте полосу.',
   help:'Шестиполосный параметрический EQ. LOW и AIR — полки, четыре средние точки — bell. Горизонталь меняет частоту, вертикаль — gain, колесо мыши — Q.'},

  {id:'dynamic',title:'DYNAMIC EQ',sub:'Adaptive control',toggle:'dynamicEqOn',
   footer:'Dynamic EQ должен реагировать на проблемные всплески, а не постоянно переделывать весь баланс.',
   help:'Адаптивная коррекция низкой и верхней зон. AMOUNT — сила, THRESHOLD — чувствительность, ATTACK/RELEASE — скорость реакции, LOW/HIGH XOVER — границы зон.',
   controls:[
     C('dynamicEq','AMOUNT','percent','v'), C('dynThreshold','THRESHOLD','db','v'),
     C('dynAttack','ATTACK','ms','knob'), C('dynRelease','RELEASE','ms','knob'),
     C('dynLowHz','LOW XOVER','hz','knob'), C('dynHighHz','HIGH XOVER','hz','knob')
   ]},

  {id:'stabilizer',title:'STABILIZER',sub:'Resonance control',toggle:'resonanceOn',
   footer:'Используйте точечно. Если нужна глубокая широкая коррекция — вернитесь к обычному EQ.',
   help:'Узкополосный контроль резонанса. FREQUENCY выбирает проблемную область, Q — ширину, AMOUNT — глубину подавления.',
   controls:[
     C('resonance','AMOUNT','percent','v'), C('resonanceHz','FREQUENCY','hz','knob'), C('resonanceQ','Q','number','knob')
   ]},

  {id:'comp',title:'VINTAGE COMP',sub:'Bus glue',toggle:'glueOn',
   footer:'На мастере обычно достаточно 1–3 dB компрессии. Слишком быстрый ATTACK съедает удар.',
   help:'Bus-компрессор для склейки. THRESHOLD и RATIO определяют глубину, ATTACK/RELEASE — движение, MAKEUP — компенсацию, MIX — параллельный баланс.',
   controls:[
     C('glueThreshold','THRESHOLD','db','v'), C('glueMakeup','MAKEUP','db','v'),
     C('glueRatio','RATIO','ratio','knob'), C('glueAttack','ATTACK','ms','knob'),
     C('glueRelease','RELEASE','ms','knob'), C('glueMix','MIX','percent','knob'), C('glue','AMOUNT','percent','knob')
   ]},

  {id:'multiband',title:'DYNAMICS',sub:'3-band mastering',toggle:'multibandOn',special:'multiband',
   footer:'Границы полос перетаскиваются. Каждая полоса имеет отдельный вертикальный фейдер Amount.',
   help:'Трёхполосная динамическая обработка. Перетаскивайте crossover-линии и регулируйте LOW/MID/HIGH отдельными вертикальными фейдерами.'},

  {id:'impact',title:'IMPACT',sub:'Transient design',toggle:'impactOn',
   footer:'Возвращайте атаку после компрессии. Если клиппер начинает слышимо хрустеть — уменьшайте PUNCH.',
   help:'Транзиентный модуль. PUNCH — сила атаки, SPEED — скорость детектора, MIX — доля эффекта.',
   controls:[C('impact','PUNCH','percent','v'), C('impactSpeed','SPEED','percent','knob'), C('impactMix','MIX','percent','knob')]},

  {id:'saturation',title:'SATURATION',sub:'Harmonic density',toggle:'analogOn',
   footer:'На мастер-шине сатурация должна ощущаться как плотность, а не как отдельный distortion-эффект.',
   help:'Мягкая гармоническая окраска. DRIVE — количество гармоник, TONE — яркость, MIX — параллельный баланс.',
   controls:[C('analog','DRIVE','percent','v'), C('analogTone','TONE','percent','knob'), C('analogMix','MIX','percent','knob')]},

  {id:'exciter',title:'EXCITER',sub:'4-band harmonics',toggle:'exciterOn',special:'exciter',
   footer:'Exciter разделён на четыре полосы. Перетаскивайте crossover-линии, меняйте Amount вертикальными фейдерами и выбирайте характер каждой полосы.',
   help:'Четырёхполосный Exciter. Для каждой полосы доступны Amount и характер Warm/Tape/Tube/Triode/Retro/Dual/Clean. MIX регулирует общий wet/dry.'},

  {id:'lowend',title:'LOW END FOCUS',sub:'Mono-safe bass',toggle:'bassMonoOn',
   footer:'Саб чаще всего переводится стабильнее, когда остаётся близко к центру.',
   help:'Центрирует низ. MONO BELOW задаёт верхнюю границу, AMOUNT — степень центровки.',
   controls:[C('bassMonoAmount','AMOUNT','percent','v'), C('bassMonoHz','MONO BELOW','hz','knob')]},

  {id:'imager',title:'IMAGER',sub:'3-band width',toggle:'imagerOn',special:'imager',
   footer:'Низ держите уже, верх можно расширять сильнее. Следите за CORRELATION справа.',
   help:'Трёхполосный stereo imager. Crossover-линии делят диапазон, вертикальные фейдеры LOW/MID/HIGH регулируют ширину, SAFETY ограничивает рискованное расширение.'},

  {id:'clipper',title:'CLIPPER',sub:'8x peak shaping',toggle:'clipperOn',
   footer:'Клиппер должен снять только короткие пики до Maximizer. Слышимый хруст означает, что DRIVE или SHAPE слишком велики.',
   help:'8x soft clipper. DRIVE — уровень в клиппер, CEILING — рабочий потолок, SHAPE — жёсткость, MIX — доля эффекта.',
   controls:[C('clipDrive','DRIVE','db','v'), C('clipCeiling','CEILING','db2','v'), C('clipShape','SHAPE','percent','knob'), C('clipMix','MIX','percent','knob')]},

  {id:'maximizer',title:'MAXIMIZER',sub:'8x adaptive look-ahead',toggle:'limiterOn',special:'maximizer',
   footer:'Главная громкость — DRIVE. Следите за LIMITER GR: постоянные большие значения съедают панч и глубину.',
   help:'Финальный 8x look-ahead Maximizer. Основные вертикальные фейдеры — DRIVE и CEILING. CHARACTER меняет скорость поведения, UPWARD COMPRESS поднимает тихие детали, SOFT CLIP снимает пики до лимитера, TRANSIENT EMPHASIS возвращает атаку, Stereo Independence управляет связью каналов.'},

  {id:'output',fixed:true,title:'OUTPUT',sub:'Final stage',toggle:null,
   footer:'OUTPUT TRIM используйте для точного level-match. Dither во время работы обычно выключен.',
   help:'Финальный выход. OUTPUT TRIM — последняя коррекция, MASTER MIX — общий dry/wet, DITHER — TPDF dither перед экспортом.',
   controls:[C('outputTrim','OUTPUT TRIM','db','knob'), C('dryWet','MASTER MIX','percent','knob'), C('ditherOn','DITHER 24-BIT','toggle')]}
];

const moduleMap = Object.fromEntries(modules.map(m=>[m.id,m]));
const processingIds = modules.filter(m=>!m.fixed).map(m=>m.id);

const guidance = {
  inputTrim:['Ручной уровень до всей обработки. Он определяет, насколько сильно будут работать динамика, сатурация, клиппер и лимитер.','Старт 0 dB. Обычно -3…+3 dB. Если входные пики уже выше -6 dBFS, чаще лучше убавить.'],
  targetInput:['Цель Smart Gain по среднему уровню. Более высокое значение плотнее подаёт сигнал в цепочку.','Старт -18 dBFS. Плотный hip-hop часто -17…-15 dBFS, если исходник чистый.'],
  smartSpeed:['Скорость адаптации Smart Gain. Слишком быстро — слышимое движение уровня.','Обычно 20–45%. Безопасный старт 25–35%.'],
  smartMaxGain:['Максимальная автоматическая коррекция Smart Gain.','Обычно 6–9 dB.'],

  dynamicEq:['Общая сила динамической коррекции.','Обычно 15–35%. Выше 45% — уже заметное вмешательство.'],
  dynThreshold:['Порог срабатывания Dynamic EQ. Ниже значение — модуль работает чаще.','Часто -24…-16 dB.'],
  dynAttack:['Скорость реакции на всплеск.','Обычно 10–30 ms.'],
  dynRelease:['Скорость отпускания.','Обычно 120–250 ms.'],
  dynLowHz:['Граница низкой зоны Dynamic EQ.','Обычно 180–350 Hz.'],
  dynHighHz:['Граница верхней зоны Dynamic EQ.','Обычно 4–8 kHz.'],

  resonance:['Глубина подавления резонанса.','Обычно 10–30%.'],
  resonanceHz:['Частота неприятного свиста или жёсткости.','Часто проблема находится около 2–6 kHz, но ищите на слух.'],
  resonanceQ:['Ширина подавления. Большой Q = узкая коррекция.','Обычно Q 2–5.'],

  glue:['Общая интенсивность glue-компрессии.','Обычно 20–50%.'],
  glueThreshold:['Порог компрессии.','Ориентируйтесь на 1–3 dB реального GR.'],
  glueRatio:['Степень компрессии.','Обычно 1.5:1–2.5:1.'],
  glueAttack:['Скорость атаки.','Для панча обычно 20–40 ms.'],
  glueRelease:['Скорость восстановления.','Обычно 100–250 ms.'],
  glueMakeup:['Компенсация после компрессии.','Обычно 0…+1.5 dB.'],
  glueMix:['Параллельный dry/wet компрессора.','Обычно 50–80%.'],

  multiband:['Общая сила многополосной динамики.','Обычно 10–35%.'],
  mbLowAmount:['Плотность LOW-полосы.','Обычно 20–45%.'],
  mbMidAmount:['Плотность MID-полосы.','Обычно 15–35%.'],
  mbHighAmount:['Плотность HIGH-полосы.','Обычно 10–30%.'],

  impact:['Сила транзиентной атаки.','Обычно 10–40%.'],
  impactSpeed:['Скорость детектора транзиентов.','Обычно 30–65%.'],
  impactMix:['Количество Impact в итоговом сигнале.','Обычно 40–75%.'],

  analog:['Количество гармонической сатурации.','Обычно 5–25%.'],
  analogTone:['Яркость сатурации.','Обычно 40–65%.'],
  analogMix:['Количество сатурированного сигнала.','Обычно 35–65%.'],

  exciterMix:['Общий wet/dry Exciter.','Обычно 50–80%.'],
  exciterBand1:['Гармоники LOW.','Обычно 0–10%.'],
  exciterBand2:['Гармоники LOW MID.','Обычно 3–15%.'],
  exciterBand3:['Гармоники HIGH MID.','Обычно 5–20%.'],
  exciterBand4:['Гармоники HIGH.','Обычно 5–18%. Если AIR EQ уже поднят — меньше.'],

  bassMonoHz:['Частоты ниже этой точки постепенно центрируются.','Обычно 80–130 Hz.'],
  bassMonoAmount:['Степень центровки низа.','Обычно 70–100%.'],

  widthLow:['Ширина низа. 100% = исходная.','Обычно 70–100%.'],
  widthMid:['Ширина середины.','Обычно 95–110%.'],
  widthHigh:['Ширина верха.','Обычно 100–120%. Следите за CORRELATION.'],
  imagerSafety:['Автоматическая защита от плохой корреляции.','Обычно 70–100%.'],

  clipDrive:['Drive в soft clipper.','Обычно 0.5–2.5 dB.'],
  clipCeiling:['Рабочий потолок клиппера.','Обычно -0.5…-0.2 dB.'],
  clipShape:['Мягкость ограничения.','Обычно 40–65%.'],
  clipMix:['Доля клиппированного сигнала.','Обычно 80–100%.'],

  limiterDrive:['Главный регулятор финальной громкости.','Поднимайте по 0.5 dB и следите за GR. Чистый мастер часто держится примерно в 1–4 dB постоянного GR.'],
  ceiling:['Финальный потолок.','Обычно -1.0…-0.7 dBFS.'],
  limiterRelease:['Базовая скорость отпускания.','Обычно 80–180 ms.'],
  limiterCharacter:['Меняет характер release-системы: слева быстрее и острее, справа спокойнее и плотнее.','Старт 3–5. Для ударной музыки обычно 2–5.'],
  limiterUpward:['Поднимает тихие детали до финального limiting, увеличивая ощущение плотности без прямого подъёма пиков.','Обычно 0–2 dB. Больше 3 dB используйте осторожно.'],
  limiterSoftClip:['Предварительный soft clip перед основной limiting-стадией.','Обычно 0–10%. Для прозрачного мастера начинайте с 2–6%.'],
  limiterTransient:['Подчёркивает транзиенты до лимитера.','Обычно 0–40%. Если Impact уже сильный — держите меньше.'],
  limiterStereoTransient:['Развязывает L/R на транзиентах.','Обычно 0–25%.'],
  limiterStereoSustain:['Развязывает L/R на sustain-части.','Обычно 0–15%.'],
  limiterTruePeak:['Финальная true-peak защита после 8x reconstruction.','Обычно ON.'],

  outputTrim:['Последняя коррекция уровня.','Обычно -1…+1 dB.'],
  dryWet:['Глобальный dry/wet основной цепи.','Для обычного мастеринга обычно 100%.'],
  ditherOn:['TPDF dither в самом конце.','Во время работы обычно OFF.']
};

const eqBands = [
  {name:'LOW',freq:'lowShelfHz',gain:'lowShelf',q:null,color:'#6bb8ff',guide:'Широкая low shelf. Обычно ±0.5–1.5 dB.'},
  {name:'LOW MID',freq:'lowMidHz',gain:'lowMid',q:'lowMidQ',color:'#73dfb1',guide:'Зона мути и тела. Часто -0.5…-1.5 dB.'},
  {name:'MID',freq:'midHz',gain:'midGain',q:'midQ',color:'#f6c86b',guide:'Середина влияет на тело и читаемость.'},
  {name:'PRESENCE',freq:'presenceHz',gain:'presence',q:'presenceQ',color:'#f49378',guide:'Атака и разборчивость.'},
  {name:'HIGH MID',freq:'highMidHz',gain:'highMidGain',q:'highMidQ',color:'#ad9bff',guide:'Жёсткость и деталь.'},
  {name:'AIR',freq:'airHz',gain:'air',q:null,color:'#82dcff',guide:'Воздух и блеск.'}
];

const p = id => state.params[id] || {norm:0,raw:0,def:0,defRaw:0,text:''};

function format(unit,raw){
  if(!Number.isFinite(raw)) return '—';
  if(unit==='db') return raw.toFixed(1)+' dB';
  if(unit==='db2') return raw.toFixed(2)+' dB';
  if(unit==='hz') return raw>=1000?(raw/1000).toFixed(raw>=10000?1:2)+' kHz':Math.round(raw)+' Hz';
  if(unit==='ms') return (raw<10?raw.toFixed(1):Math.round(raw))+' ms';
  if(unit==='percent') return Math.round(raw*100)+' %';
  if(unit==='width') return Math.round(raw*100)+' %';
  if(unit==='ratio') return raw.toFixed(1)+' : 1';
  if(unit==='number') return raw.toFixed(2);
  if(unit==='char') return raw.toFixed(2);
  return raw.toFixed(2);
}

function hold(id,mode='norm'){
  heldParams.set(id,mode);
  emit('gesture',{id,phase:'begin'});
}
function release(id){
  emit('gesture',{id,phase:'end'});
  setTimeout(()=>heldParams.delete(id),80);
}
function setNorm(id,n,notify=true){
  n=clamp(n,0,1);
  if(state.params[id]) state.params[id].norm=n;
  if(notify) emit('setParam',{id,norm:n});
}
function setRaw(id,raw,notify=true){
  if(state.params[id]) state.params[id].raw=raw;
  if(notify) emit('setParam',{id,raw});
}
function mergeRemote(next){
  if(!next) return;
  const old=state.params||{};
  const incoming=next.params||{};
  const merged={};

  Object.keys(incoming).forEach(id=>{
    const mode=heldParams.get(id);
    if(!mode||!old[id]){ merged[id]=incoming[id]; return; }

    if(mode==='raw')
      merged[id]={...incoming[id],raw:old[id].raw,norm:old[id].norm,text:old[id].text};
    else
      merged[id]={...incoming[id],norm:old[id].norm};
  });

  state={...next,params:merged};
  if(!Array.isArray(state.chain)) state.chain=processingIds.slice();
}

function findControlSpec(id){
  for(const m of modules){
    for(const spec of (m.controls||[]))
      if(spec.id===id) return spec;
  }
  const special={
    multiband:C('multiband','GLOBAL','percent','knob'),
    mbLowAmount:C('mbLowAmount','LOW','percent','v'),
    mbMidAmount:C('mbMidAmount','MID','percent','v'),
    mbHighAmount:C('mbHighAmount','HIGH','percent','v'),
    exciterMix:C('exciterMix','MIX','percent','knob'),
    exciterBand1:C('exciterBand1','LOW AMOUNT','percent','v'),
    exciterBand2:C('exciterBand2','LOW MID AMOUNT','percent','v'),
    exciterBand3:C('exciterBand3','HIGH MID AMOUNT','percent','v'),
    exciterBand4:C('exciterBand4','HIGH AMOUNT','percent','v'),
    imagerSafety:C('imagerSafety','SAFETY','percent','knob'),
    widthLow:C('widthLow','LOW WIDTH','width','v'),
    widthMid:C('widthMid','MID WIDTH','width','v'),
    widthHigh:C('widthHigh','HIGH WIDTH','width','v'),
    limiterDrive:C('limiterDrive','DRIVE','db','v'),
    ceiling:C('ceiling','CEILING','db2','v'),
    limiterRelease:C('limiterRelease','RELEASE','ms','knob'),
    limiterCharacter:C('limiterCharacter','CHARACTER','char','knob'),
    limiterUpward:C('limiterUpward','UPWARD COMPRESS','db','knob'),
    limiterSoftClip:C('limiterSoftClip','SOFT CLIP','percent','knob'),
    limiterTransient:C('limiterTransient','TRANSIENT EMPHASIS','percent','knob'),
    limiterStereoTransient:C('limiterStereoTransient','TRANSIENT','percent','v'),
    limiterStereoSustain:C('limiterStereoSustain','SUSTAIN','percent','v'),
    limiterTruePeak:C('limiterTruePeak','TRUE PEAK','toggle','toggle')
  };
  return special[id]||null;
}

function currentContext(id){
  const m=state.meters||{};
  if(id==='targetInput'&&Number(m.inputPeak)>-6) return 'Сейчас вход уже горячий. Сначала уменьшите INPUT TRIM.';
  if(['glue','glueThreshold','glueRatio'].includes(id)&&(p('dynamicEq').raw>.42||p('multiband').raw>.42))
    return 'Предыдущая динамическая обработка уже сильная. Компрессор начинайте мягче.';
  if(id==='impact'&&p('glue').raw>.48) return 'Glue уже заметный. Начните примерно с 20–35% PUNCH.';
  if(id.startsWith('exciterBand')&&p('air').raw>1.0) return 'AIR EQ уже поднят. Верхнюю полосу Exciter держите умеренно.';
  if((id.startsWith('width')||id==='imagerSafety')&&Number(m.correlation)<.2) return 'CORRELATION низкая. Не расширяйте сильнее.';
  if(id==='limiterDrive'&&Number(m.limiterGR)>4) return 'LIMITER GR уже выше 4 dB. Дополнительный Drive скорее уменьшит панч.';
  if(id==='limiterTransient'&&p('impact').raw>.55) return 'Impact уже сильно подчёркивает атаку. Transient Emphasis держите ниже.';
  return '';
}

function positionTip(x,y){
  const tip=$('#tooltip');
  tip.style.left=Math.max(8,Math.min(window.innerWidth-345,x+14))+'px';
  tip.style.top=Math.max(8,Math.min(window.innerHeight-170,y+14))+'px';
  tip.classList.add('show');
}
function showParamTip(id,x,y){
  const spec=findControlSpec(id),g=guidance[id];
  if(!spec||!g)return;
  $('#tooltipTitle').textContent=spec.label;
  $('#tooltipBody').textContent=g[0];
  $('#tooltipRange').textContent='Рабочая зона: '+g[1];
  $('#tooltipContext').textContent=currentContext(id);
  positionTip(x,y);
}
function showGenericTip(text,x,y){
  $('#tooltipTitle').textContent='Подсказка';
  $('#tooltipBody').textContent=text;
  $('#tooltipRange').textContent='';
  $('#tooltipContext').textContent='';
  positionTip(x,y);
}
function showEqTip(index,x,y){
  const b=eqBands[index];
  $('#tooltipTitle').textContent=b.name+' EQ';
  $('#tooltipBody').textContent=b.guide;
  $('#tooltipRange').textContent=b.q?'Колесо меняет Q. Для мастеринга часто 0.7–1.4.':'Это широкая shelf-полоса.';
  $('#tooltipContext').textContent='';
  positionTip(x,y);
}
function hideTip(){$('#tooltip').classList.remove('show');}

function buildPresets(){
  const sig=(state.presets||[]).join('|');
  const menu=$('#presetMenu');
  if(menu.dataset.sig===sig){syncPreset();return;}
  menu.dataset.sig=sig;
  menu.innerHTML='';

  (state.presets||[]).forEach((name,i)=>{
    const b=document.createElement('button');
    b.className='preset-item'+(name.includes('INIT')?' init':'');
    b.textContent=name;
    b.onclick=e=>{e.stopPropagation();emit('preset',i);state.presetIndex=i;closePreset();syncPreset();};
    menu.appendChild(b);
  });
  syncPreset();
}
function syncPreset(){
  $('#presetButtonText').textContent=(state.presets&&state.presets[state.presetIndex])||'Preset';
  $$('.preset-item').forEach((el,i)=>el.classList.toggle('active',i===state.presetIndex));
}
function togglePreset(){
  const open=!$('#presetMenu').classList.contains('open');
  $('#presetMenu').classList.toggle('open',open);
  $('#presetButton').classList.toggle('open',open);
}
function closePreset(){
  $('#presetMenu').classList.remove('open');
  $('#presetButton').classList.remove('open');
}

function chainIds(){
  return Array.isArray(state.chain)
    ? state.chain.filter(id=>moduleMap[id]&&!moduleMap[id].fixed)
    : processingIds.slice();
}
function sendChain(ids){
  state.chain=ids.slice();
  emit('chain',ids);
  chainSignature='';
  renderChain(true);
  renderPicker(true);
}
function selectModule(id){
  if(!moduleMap[id])return;
  selectedId=id;
  syncChain();
  renderModule();
  drawAnalyzer();
  const el=$('#chain [data-id="'+id+'"]');
  if(el) el.scrollIntoView({behavior:'smooth',block:'nearest',inline:'center'});
}
function removeFromChain(id){
  const order=chainIds().filter(x=>x!==id);
  sendChain(order);
  if(selectedId===id) selectModule(order[0]||'input');
}

function renderChain(force=false){
  const ids=chainIds();
  const sig=ids.join('|');
  if(!force&&sig===chainSignature){syncChain();return;}
  chainSignature=sig;

  const root=$('#chain');
  root.innerHTML='';

  ids.forEach(id=>{
    const m=moduleMap[id];
    const card=document.createElement('div');
    card.className='chain-item';
    card.dataset.id=id;
    card.tabIndex=0;

    const handle=document.createElement('div');
    handle.className='drag-handle';
    handle.draggable=true;
    handle.title='Перетащить модуль';
    handle.innerHTML='<span></span><span></span><span></span>';

    const power=document.createElement('button');
    power.className='chain-power';
    power.dataset.tip='Включить или выключить модуль, не удаляя его из цепочки.';
    power.onclick=e=>{
      e.stopPropagation();
      setNorm(m.toggle,p(m.toggle).norm>=.5?0:1);
      syncChain();
      syncModule();
    };

    const copy=document.createElement('div');
    copy.className='chain-copy';
    copy.innerHTML='<div class="chain-title">'+m.title+'</div><div class="chain-sub">'+m.sub+'</div>';

    const remove=document.createElement('button');
    remove.className='chain-remove';
    remove.textContent='×';
    remove.dataset.tip='Удалить модуль из цепочки. Его можно добавить обратно кнопкой +.';
    remove.onclick=e=>{e.stopPropagation();removeFromChain(id);};

    card.onclick=e=>{
      if(e.target.closest('.drag-handle,.chain-power,.chain-remove'))return;
      selectModule(id);
    };
    card.onpointerup=e=>{
      if(e.button===0&&!e.target.closest('.drag-handle,.chain-power,.chain-remove')) selectModule(id);
    };
    card.onkeydown=e=>{
      if(e.key==='Enter'||e.key===' '){e.preventDefault();selectModule(id);}
    };

    handle.ondragstart=e=>{
      dragChainId=id;
      card.classList.add('dragging');
      e.dataTransfer.effectAllowed='move';
      e.dataTransfer.setData('text/plain',id);
    };
    handle.ondragend=()=>{
      dragChainId=null;
      $$('.chain-item').forEach(x=>x.classList.remove('dragging','drag-over'));
    };
    card.ondragover=e=>{
      if(!dragChainId||dragChainId===id)return;
      e.preventDefault();
      card.classList.add('drag-over');
    };
    card.ondragleave=()=>card.classList.remove('drag-over');
    card.ondrop=e=>{
      e.preventDefault();
      card.classList.remove('drag-over');
      if(!dragChainId||dragChainId===id)return;
      const order=chainIds();
      const from=order.indexOf(dragChainId),to=order.indexOf(id);
      if(from<0||to<0)return;
      order.splice(from,1);
      order.splice(to,0,dragChainId);
      sendChain(order);
    };

    card.append(handle,power,copy,remove);
    root.appendChild(card);
  });

  syncChain();
}
function syncChain(){
  $$('.chain-item').forEach(card=>{
    const id=card.dataset.id,m=moduleMap[id];
    card.classList.toggle('active',selectedId===id);
    card.classList.toggle('enabled',m.toggle&&p(m.toggle).norm>=.5);
  });
}

function renderPicker(force=false){
  const current=new Set(chainIds());
  const sig=processingIds.filter(id=>!current.has(id)).join('|');
  if(!force&&sig===pickerSignature)return;
  pickerSignature=sig;
  const picker=$('#modulePicker');
  picker.innerHTML='<div class="module-picker-title">ДОБАВИТЬ МОДУЛЬ</div>';

  processingIds.filter(id=>!current.has(id)).forEach(id=>{
    const m=moduleMap[id];
    const b=document.createElement('button');
    b.className='module-pick';
    b.textContent=m.title+'  ·  '+m.sub;
    b.onclick=e=>{
      e.stopPropagation();
      const order=chainIds();order.push(id);sendChain(order);selectModule(id);closePicker();
    };
    picker.appendChild(b);
  });

  if(current.size===processingIds.length){
    const empty=document.createElement('div');
    empty.className='module-picker-title';
    empty.textContent='ВСЕ МОДУЛИ УЖЕ В ЦЕПОЧКЕ';
    picker.appendChild(empty);
  }
}
function togglePicker(){
  const open=!$('#modulePicker').classList.contains('open');
  $('#modulePicker').classList.toggle('open',open);
  $('#addModuleBtn').classList.toggle('open',open);
  renderPicker(true);
}
function closePicker(){
  $('#modulePicker').classList.remove('open');
  $('#addModuleBtn').classList.remove('open');
}

function makeKnob(spec){
  const c=document.createElement('div');
  c.className='knob-control';
  c.dataset.paramId=spec.id;

  c.innerHTML=
    '<div class="control-label">'+spec.label+'</div>'+
    '<div class="rotary"><div class="rotary-ring"></div><div class="rotary-cap"><div class="rotary-pointer"></div></div></div>'+
    '<div class="control-value"></div>';

  const rotary=$('.rotary',c),value=$('.control-value',c);
  let drag=false,startY=0,startNorm=0;

  const sync=()=>{
    const n=clamp(p(spec.id).norm,0,1);
    const deg=-135+n*270;
    rotary.style.setProperty('--angle',deg+'deg');
    rotary.style.setProperty('--sweep',(n*270)+'deg');
    value.textContent=format(spec.unit,p(spec.id).raw);
  };
  c._sync=sync;

  rotary.onpointerdown=e=>{
    e.preventDefault();
    drag=true;startY=e.clientY;startNorm=p(spec.id).norm;
    rotary.classList.add('dragging');
    rotary.setPointerCapture(e.pointerId);
    hold(spec.id,'norm');
  };
  rotary.onpointermove=e=>{
    if(!drag)return;
    const n=clamp(startNorm+(startY-e.clientY)/150,0,1);
    setNorm(spec.id,n);
    rotary.style.setProperty('--angle',(-135+n*270)+'deg');
    rotary.style.setProperty('--sweep',(n*270)+'deg');
  };
  const end=e=>{
    if(!drag)return;
    drag=false;rotary.classList.remove('dragging');
    try{rotary.releasePointerCapture(e.pointerId)}catch(_){}
    release(spec.id);
  };
  rotary.onpointerup=end;
  rotary.onpointercancel=end;
  rotary.ondblclick=e=>{e.preventDefault();setNorm(spec.id,p(spec.id).def);sync();};
  sync();
  return c;
}

function makeVertical(spec,compact=false){
  const c=document.createElement('div');
  c.className='v-fader-control'+(compact?' compact':'');
  c.dataset.paramId=spec.id;
  c.innerHTML=
    '<div class="control-label">'+spec.label+'</div>'+
    '<div class="v-fader"><div class="v-track"><div class="v-fill"></div><div class="v-thumb"></div></div></div>'+
    '<div class="control-value"></div>';

  const f=$('.v-fader',c),value=$('.control-value',c);
  let drag=false;

  const sync=()=>{
    const n=clamp(p(spec.id).norm,0,1);
    $('.v-fill',c).style.height=(n*100)+'%';
    $('.v-thumb',c).style.bottom=(n*100)+'%';
    value.textContent=format(spec.unit,p(spec.id).raw);
  };
  c._sync=sync;

  const move=e=>{
    const r=f.getBoundingClientRect();
    const n=clamp(1-(e.clientY-r.top)/Math.max(1,r.height),0,1);
    setNorm(spec.id,n);
    $('.v-fill',c).style.height=(n*100)+'%';
    $('.v-thumb',c).style.bottom=(n*100)+'%';
  };
  f.onpointerdown=e=>{
    e.preventDefault();drag=true;hold(spec.id,'norm');
    f.setPointerCapture(e.pointerId);move(e);
  };
  f.onpointermove=e=>{if(drag)move(e);};
  const end=e=>{
    if(!drag)return;drag=false;
    try{f.releasePointerCapture(e.pointerId)}catch(_){}
    release(spec.id);
  };
  f.onpointerup=end;f.onpointercancel=end;
  f.ondblclick=e=>{e.preventDefault();setNorm(spec.id,p(spec.id).def);sync();};
  sync();
  return c;
}

function makeToggle(spec){
  const c=document.createElement('div');
  c.className='toggle-control';
  c.dataset.paramId=spec.id;
  c.innerHTML='<div class="control-label">'+spec.label+'</div><button class="big-toggle"></button><div class="control-value"></div>';

  const sync=()=>{
    const on=p(spec.id).norm>=.5;
    $('.big-toggle',c).classList.toggle('on',on);
    $('.control-value',c).textContent=on?'ON':'OFF';
  };
  c._sync=sync;
  $('.big-toggle',c).onclick=()=>{setNorm(spec.id,p(spec.id).norm>=.5?0:1);sync();};
  sync();
  return c;
}

function renderEqPanel(root){
  root.classList.add('eq-mode');
  const wrap=document.createElement('div');
  wrap.className='eq-module-panel';
  const list=document.createElement('div');
  list.className='eq-band-list';

  eqBands.forEach((b,i)=>{
    const item=document.createElement('div');
    item.className='eq-band';
    item.dataset.eqBand=String(i);
    item.innerHTML=
      '<div class="eq-band-top"><div class="eq-band-name">'+b.name+'</div><div class="eq-band-dot" style="background:'+b.color+'"></div></div>'+
      '<div class="eq-band-values"><div><span>FREQ</span><b class="eq-freq"></b></div><div><span>GAIN</span><b class="eq-gain"></b></div></div>';
    item.onclick=()=>{selectedEqBand=i;syncEqPanel();drawAnalyzer();};
    list.appendChild(item);
  });

  const info=document.createElement('div');
  info.className='eq-instructions';
  info.id='eqInstructions';
  wrap.append(list,info);
  root.appendChild(wrap);
  syncEqPanel();
}
function syncEqPanel(){
  $$('.eq-band').forEach((item,i)=>{
    const b=eqBands[i],f=p(b.freq).raw,g=p(b.gain).raw;
    item.classList.toggle('active',i===selectedEqBand);
    $('.eq-freq',item).textContent=format('hz',f);
    $('.eq-gain',item).textContent=(g>=0?'+':'')+g.toFixed(1)+' dB';
  });

  const b=eqBands[selectedEqBand],info=$('#eqInstructions');
  if(info){
    info.innerHTML=
      '<h3>'+b.name+(b.q?' · Q '+p(b.q).raw.toFixed(2):' · SHELF')+'</h3>'+
      '<p>'+b.guide+' Тяните точку на верхнем графике. '+(b.q?'Колесо мыши меняет Q.':'Полка работает широко и мягко.')+'</p>'+
      '<div class="keys"><span class="key">DRAG = FREQ + GAIN</span>'+(b.q?'<span class="key">WHEEL = Q</span>':'')+'<span class="key">DOUBLE CLICK = RESET</span></div>';
  }
}

function freqNorm(freq,min=20,max=20000){return Math.log(freq/min)/Math.log(max/min);}
function normFreq(n,min=20,max=20000){return min*Math.pow(max/min,clamp(n,0,1));}

function makeBandColumn(id,label,unit='percent',modeId=null){
  const col=document.createElement('div');
  col.className='band-column';
  col.dataset.paramId=id;
  const title=document.createElement('div');
  title.className='band-name';
  title.textContent=label;
  col.appendChild(title);

  const f=makeVertical(C(id,'',unit,'v'),true);
  $('.control-label',f).remove();
  col.appendChild(f);

  if(modeId){
    const sel=document.createElement('select');
    sel.className='mode-select';
    sel.dataset.paramId=modeId;
    ['Warm','Tape','Tube','Triode','Retro','Dual','Clean'].forEach((name,i)=>{
      const o=document.createElement('option');o.value=i;o.textContent=name;sel.appendChild(o);
    });
    sel.value=String(Math.round(p(modeId).raw||0));
    sel.onchange=()=>{
      hold(modeId,'raw');setRaw(modeId,Number(sel.value));release(modeId);
    };
    sel._sync=()=>{sel.value=String(Math.round(p(modeId).raw||0));};
    col.appendChild(sel);
  }
  return col;
}

function renderBandModule(root,type){
  root.classList.add('band-mode');
  const wrap=document.createElement('div');
  wrap.className='band-editor';

  const top=document.createElement('div');
  top.className='band-topbar';

  let globalSpec;
  if(type==='exciter') globalSpec=C('exciterMix','MIX','percent','knob');
  else if(type==='multiband') globalSpec=C('multiband','GLOBAL','percent','knob');
  else globalSpec=C('imagerSafety','SAFETY','percent','knob');

  const global=makeKnob(globalSpec);
  global.classList.add('mini-knob');
  top.appendChild(global);

  const descriptor=document.createElement('div');
  descriptor.className='band-descriptor';
  descriptor.textContent=type==='exciter'
    ? '4 BANDS · HARMONIC MODES'
    : type==='multiband'
      ? '3 BANDS · DYNAMIC DENSITY'
      : '3 BANDS · STEREO WIDTH';
  top.appendChild(descriptor);

  const work=document.createElement('div');
  work.className='band-work';

  let crossovers=[],bands=[];
  if(type==='exciter'){
    crossovers=[
      {id:'exciterX1',min:60,max:1000},
      {id:'exciterX2',min:300,max:6000},
      {id:'exciterX3',min:1800,max:16000}
    ];
    bands=[
      makeBandColumn('exciterBand1','LOW','percent','exciterMode1'),
      makeBandColumn('exciterBand2','LOW MID','percent','exciterMode2'),
      makeBandColumn('exciterBand3','HIGH MID','percent','exciterMode3'),
      makeBandColumn('exciterBand4','HIGH','percent','exciterMode4')
    ];
  }else if(type==='multiband'){
    crossovers=[
      {id:'mbLowHz',min:70,max:500},
      {id:'mbHighHz',min:1800,max:12000}
    ];
    bands=[
      makeBandColumn('mbLowAmount','LOW'),
      makeBandColumn('mbMidAmount','MID'),
      makeBandColumn('mbHighAmount','HIGH')
    ];
    work.style.gridTemplateColumns='repeat(3,minmax(0,1fr))';
  }else{
    crossovers=[
      {id:'imagerLowHz',min:80,max:500},
      {id:'imagerHighHz',min:1800,max:12000}
    ];
    bands=[
      makeBandColumn('widthLow','LOW','width'),
      makeBandColumn('widthMid','MID','width'),
      makeBandColumn('widthHigh','HIGH','width')
    ];
    work.style.gridTemplateColumns='repeat(3,minmax(0,1fr))';
  }

  bands.forEach(b=>work.appendChild(b));
  wrap.append(top,work);
  root.appendChild(wrap);

  const syncCross=()=>{
    $$('.crossover-line',work).forEach((line,i)=>{
      const def=crossovers[i],n=freqNorm(p(def.id).raw,20,20000);
      line.style.left=(n*100)+'%';
      const tag=line.previousElementSibling;
      tag.style.left=(n*100)+'%';
      tag.textContent=format('hz',p(def.id).raw);
    });
  };

  crossovers.forEach((def,i)=>{
    const tag=document.createElement('div');tag.className='crossover-tag';
    const line=document.createElement('div');line.className='crossover-line';line.dataset.paramId=def.id;
    work.append(tag,line);

    let drag=false;
    const move=e=>{
      const r=work.getBoundingClientRect();
      let f=normFreq(clamp((e.clientX-r.left)/Math.max(1,r.width),0,1),20,20000);
      f=clamp(f,def.min,def.max);

      if(type==='exciter'){
        if(i===0)f=Math.min(f,p('exciterX2').raw-100);
        if(i===1){f=Math.max(f,p('exciterX1').raw+100);f=Math.min(f,p('exciterX3').raw-250);}
        if(i===2)f=Math.max(f,p('exciterX2').raw+250);
      }else{
        if(i===0)f=Math.min(f,p(crossovers[1].id).raw-250);
        if(i===1)f=Math.max(f,p(crossovers[0].id).raw+250);
      }

      setRaw(def.id,f);
      syncCross();
    };

    line.onpointerdown=e=>{
      e.preventDefault();drag=true;hold(def.id,'raw');
      line.setPointerCapture(e.pointerId);move(e);
    };
    line.onpointermove=e=>{if(drag)move(e);};
    const end=e=>{
      if(!drag)return;drag=false;
      try{line.releasePointerCapture(e.pointerId)}catch(_){}
      release(def.id);
    };
    line.onpointerup=end;line.onpointercancel=end;
  });

  wrap._sync=()=>{
    syncCross();
    $$('.band-column',work).forEach(col=>{
      const child=$('.v-fader-control',col);if(child&&child._sync)child._sync();
      const sel=$('.mode-select',col);if(sel&&sel._sync)sel._sync();
    });
    if(global._sync)global._sync();
  };
  wrap._sync();
}

function makeSegmented(id,labels){
  const wrap=document.createElement('div');
  wrap.className='segmented';
  wrap.dataset.paramId=id;
  labels.forEach((label,i)=>{
    const b=document.createElement('button');b.textContent=label;
    b.onclick=()=>{hold(id,'raw');setRaw(id,i);release(id);sync();};
    wrap.appendChild(b);
  });
  const sync=()=>{
    const index=Math.round(p(id).raw||0);
    $$('button',wrap).forEach((b,i)=>b.classList.toggle('active',i===index));
  };
  wrap._sync=sync;sync();return wrap;
}

function renderMaximizer(root){
  root.classList.add('maximizer-mode');
  const wrap=document.createElement('div');
  wrap.className='maximizer-layout';

  const mains=document.createElement('div');
  mains.className='max-main-faders';
  const drive=makeVertical(C('limiterDrive','DRIVE','db','v'));
  const ceiling=makeVertical(C('ceiling','CEILING','db2','v'));
  drive.classList.add('hero-fader');ceiling.classList.add('hero-fader');
  mains.append(drive,ceiling);

  const controls=document.createElement('div');
  controls.className='max-controls-grid';
  [
    C('limiterCharacter','CHARACTER','char','knob'),
    C('limiterRelease','RELEASE','ms','knob'),
    C('limiterUpward','UPWARD COMPRESS','db','knob'),
    C('limiterTransient','TRANSIENT EMPHASIS','percent','knob')
  ].forEach(s=>controls.appendChild(makeKnob(s)));

  const stereo=document.createElement('div');
  stereo.className='max-stereo';
  stereo.innerHTML='<div class="mini-heading">STEREO INDEPENDENCE</div>';
  const pair=document.createElement('div');pair.className='stereo-pair';
  pair.append(
    makeVertical(C('limiterStereoTransient','TRANSIENT','percent','v'),true),
    makeVertical(C('limiterStereoSustain','SUSTAIN','percent','v'),true)
  );
  stereo.appendChild(pair);

  const soft=document.createElement('div');
  soft.className='max-soft';
  soft.innerHTML='<div class="mini-heading">SOFT CLIP</div>';
  const softKnob=makeKnob(C('limiterSoftClip','AMOUNT','percent','knob'));
  softKnob.classList.add('mini-knob');
  const modes=makeSegmented('limiterSoftClipMode',['H','M','L']);
  soft.append(softKnob,modes);

  const truePeak=makeToggle(C('limiterTruePeak','TRUE PEAK','toggle','toggle'));
  truePeak.classList.add('max-truepeak');

  wrap.append(mains,controls,stereo,soft,truePeak);
  root.appendChild(wrap);

  wrap._sync=()=>{
    [drive,ceiling,truePeak,softKnob].forEach(x=>x._sync&&x._sync());
    $$('.knob-control,.v-fader-control,.toggle-control',controls).forEach(x=>x._sync&&x._sync());
    $$('.v-fader-control',stereo).forEach(x=>x._sync&&x._sync());
    modes._sync();
  };
  wrap._sync();
}

function renderModule(){
  const m=moduleMap[selectedId]||moduleMap.input;
  $('#moduleTitle').textContent=m.title;
  $('#moduleKicker').textContent=m.sub.toUpperCase();
  $('#moduleFooter').textContent=m.footer;
  const title=$('.section-title');
  if(title)title.textContent=selectedId==='eq'?'Spectrum + Parametric EQ':'Spectrum';

  const power=$('#modulePower');
  if(m.toggle){
    power.style.display='';
    power.dataset.paramId=m.toggle;
    power.onclick=()=>{setNorm(m.toggle,p(m.toggle).norm>=.5?0:1);syncModule();syncChain();};
  }else{
    power.style.display='none';power.onclick=null;delete power.dataset.paramId;
  }

  $('#helpBtn').onclick=()=>openHelp(m);
  const removable=!m.fixed&&chainIds().includes(m.id);
  $('#removeModuleBtn').style.display=removable?'':'none';
  $('#removeModuleBtn').onclick=()=>removeFromChain(m.id);

  const root=$('#moduleControls');
  root.className='controls-grid';
  root.innerHTML='';

  if(m.special==='eq') renderEqPanel(root);
  else if(m.special==='exciter'||m.special==='multiband'||m.special==='imager') renderBandModule(root,m.special);
  else if(m.special==='maximizer') renderMaximizer(root);
  else{
    (m.controls||[]).forEach(spec=>{
      if(spec.type==='toggle')root.appendChild(makeToggle(spec));
      else if(spec.type==='v')root.appendChild(makeVertical(spec));
      else root.appendChild(makeKnob(spec));
    });
  }

  syncModule();
}

function syncModule(){
  const m=moduleMap[selectedId]||moduleMap.input;
  if(m.toggle)$('#modulePower').classList.toggle('on',p(m.toggle).norm>=.5);

  $$('#moduleControls .knob-control,#moduleControls .v-fader-control,#moduleControls .toggle-control').forEach(el=>{
    if(el._sync)el._sync();
  });
  $$('#moduleControls .mode-select,#moduleControls .segmented').forEach(el=>{
    if(el._sync)el._sync();
  });
  if(m.special==='eq')syncEqPanel();

  const band=$('#moduleControls .band-editor');if(band&&band._sync)band._sync();
  const max=$('#moduleControls .maximizer-layout');if(max&&max._sync)max._sync();
}
function openHelp(m){$('#modalTitle').textContent=m.title;$('#modalText').textContent=m.help;$('#modal').classList.add('open');}
function closeHelp(){$('#modal').classList.remove('open');}

function updateBypass(){$('#bypassBtn').classList.toggle('on',p('masterBypass').norm>=.5);}
function meterNorm(db){return Number.isFinite(db)?clamp((db+60)/60,0,1):0;}
function dbText(db,suffix=''){return Number.isFinite(db)&&db>-99?db.toFixed(1)+suffix:'-∞';}
function updateMeters(){
  const m=state.meters||{},ip=Number(m.inputPeak??-100),op=Number(m.outputPeak??-100);
  $('#inMeter').style.height=(meterNorm(ip)*100)+'%';
  $('#outMeter').style.height=(meterNorm(op)*100)+'%';
  $('#inPeak').textContent=dbText(ip,' dBFS');
  $('#outPeak').textContent=dbText(op,' dBFS');
  $('#lufs').textContent=dbText(Number(m.lufs??-100));
  $('#crest').textContent=Number(m.crest??0).toFixed(1)+' dB';
  $('#corr').textContent=Number(m.correlation??0).toFixed(2);
  $('#gr').textContent=Number(m.limiterGR??0).toFixed(1)+' dB';
  $('#inputGain').textContent=Number(m.inputGain??0).toFixed(1)+' dB';

  const badge=$('#qualityBadge'),corr=Number(m.correlation??0),gr=Number(m.limiterGR??0);
  if(corr<-.05){badge.textContent='PHASE';badge.className='quality-badge bad';}
  else if(gr>7){badge.textContent='HARD';badge.className='quality-badge warn';}
  else{badge.textContent='CLEAN';badge.className='quality-badge';}

  const rms=Number(m.inputRms??-100),coach=$('#coach');
  coach.classList.remove('warn','hot');
  if(rms<-60)$('#coachText').textContent='Ожидаю аудиосигнал…';
  else if(rms<-24){coach.classList.add('warn');$('#coachText').textContent='Вход тихий. Хороший старт — средний уровень примерно -18…-16 dBFS.';}
  else if(rms>-10){coach.classList.add('hot');$('#coachText').textContent='Вход слишком горячий. Снизьте INPUT TRIM, чтобы сохранить транзиенты и headroom.';}
  else $('#coachText').textContent='Вход в рабочей зоне. Настраивайте тон, динамику и финальную громкость по очереди.';
}

const canvas=$('#eqCanvas'),ctx=canvas.getContext('2d');
function resizeCanvas(){
  const r=canvas.getBoundingClientRect(),d=Math.max(1,window.devicePixelRatio||1);
  const w=Math.max(10,Math.round(r.width*d)),h=Math.max(10,Math.round(r.height*d));
  if(canvas.width!==w||canvas.height!==h){canvas.width=w;canvas.height=h;}
  ctx.setTransform(d,0,0,d,0,0);drawAnalyzer();
}
function graphRect(){const r=canvas.getBoundingClientRect();return{x:18,y:14,w:Math.max(10,r.width-36),h:Math.max(10,r.height-35)};}
function fx(f,g){return g.x+freqNorm(f,20,20000)*g.w;}
function gy(v,g){return g.y+g.h*(.5-clamp(v,-12,12)/24);}
function sy(db,g){return g.y+g.h*(1-clamp((db+84)/84,0,1));}
function bell(f,c,q){const w=1.45-clamp((q-.25)/3.75,0,1)*1.2,x=Math.log2(f/Math.max(20,c))/w;return Math.exp(-.5*x*x);}
function eqAt(f){
  let v=(p('lowShelf').raw||0)/(1+Math.pow(f/Math.max(30,p('lowShelfHz').raw||105),3));
  v+=(p('lowMid').raw||0)*bell(f,p('lowMidHz').raw||320,p('lowMidQ').raw||.85);
  v+=(p('midGain').raw||0)*bell(f,p('midHz').raw||900,p('midQ').raw||.9);
  v+=(p('presence').raw||0)*bell(f,p('presenceHz').raw||3200,p('presenceQ').raw||.9);
  v+=(p('highMidGain').raw||0)*bell(f,p('highMidHz').raw||6200,p('highMidQ').raw||.9);
  v+=(p('air').raw||0)/(1+Math.pow(Math.max(3000,p('airHz').raw||10500)/Math.max(20,f),4));
  return v;
}
function drawSpectrum(a,g,color,width,fill){
  if(!a||!a.length)return;
  ctx.beginPath();
  a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
  if(fill){
    ctx.lineTo(g.x+g.w,g.y+g.h);ctx.lineTo(g.x,g.y+g.h);ctx.closePath();
    const z=ctx.createLinearGradient(0,g.y,0,g.y+g.h);
    z.addColorStop(0,'rgba(88,168,255,.14)');z.addColorStop(1,'rgba(88,168,255,0)');
    ctx.fillStyle=z;ctx.fill();
    ctx.beginPath();
    a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
  }
  ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();
}
function drawAnalyzer(){
  const r=canvas.getBoundingClientRect();if(!r.width||!r.height)return;
  const g=graphRect();ctx.clearRect(0,0,r.width,r.height);

  const bg=ctx.createLinearGradient(0,g.y,0,g.y+g.h);
  bg.addColorStop(0,'rgba(18,24,31,.88)');bg.addColorStop(1,'rgba(7,10,14,.82)');
  ctx.fillStyle=bg;ctx.fillRect(g.x,g.y,g.w,g.h);

  ctx.font='8px Segoe UI';ctx.textBaseline='bottom';
  [20,50,100,200,500,1000,2000,5000,10000,20000].forEach(f=>{
    const x=fx(f,g);ctx.strokeStyle='rgba(104,120,136,.17)';
    ctx.beginPath();ctx.moveTo(x,g.y);ctx.lineTo(x,g.y+g.h);ctx.stroke();
    ctx.fillStyle='rgba(112,129,145,.55)';
    ctx.textAlign=f===20?'left':f===20000?'right':'center';
    ctx.fillText(f>=1000?(f/1000)+'k':String(f),x,g.y+g.h-3);
  });
  [-12,-6,0,6,12].forEach(db=>{
    const y=gy(db,g);ctx.strokeStyle=db===0?'rgba(120,139,157,.30)':'rgba(100,118,135,.12)';
    ctx.beginPath();ctx.moveTo(g.x,y);ctx.lineTo(g.x+g.w,y);ctx.stroke();
  });

  drawSpectrum(state.pre,g,'rgba(127,143,158,.34)',1,false);
  drawSpectrum(state.post,g,'rgba(90,178,255,.92)',1.6,true);

  if(selectedId==='eq'){
    ctx.beginPath();
    const n=Math.max(240,Math.floor(g.w));
    for(let i=0;i<n;i++){
      const u=i/(n-1),f=normFreq(u,20,20000),x=g.x+u*g.w,y=gy(eqAt(f),g);
      if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);
    }
    const grad=ctx.createLinearGradient(g.x,0,g.x+g.w,0);
    grad.addColorStop(0,'#6bb8ff');grad.addColorStop(.5,'#ad9bff');grad.addColorStop(1,'#82dcff');
    ctx.strokeStyle=grad;ctx.lineWidth=2.2;ctx.stroke();

    eqBands.forEach((b,i)=>{
      const x=fx(p(b.freq).raw,g),y=gy(p(b.gain).raw,g),active=i===selectedEqBand,hover=i===eqHover;
      ctx.beginPath();ctx.arc(x,y,active?8:6,0,Math.PI*2);
      ctx.fillStyle=active?b.color:'#0a0e13';ctx.fill();
      ctx.strokeStyle=b.color;ctx.lineWidth=active||hover?2.2:1.5;ctx.stroke();
      if(active){ctx.beginPath();ctx.arc(x,y,14,0,Math.PI*2);ctx.strokeStyle=b.color+'55';ctx.stroke();}
    });
  }

  renderEqInspector();
}
function renderEqInspector(){
  const root=$('#eqInspector');
  if(selectedId!=='eq'){root.innerHTML='';return;}
  const b=eqBands[selectedEqBand],f=p(b.freq).raw,g=p(b.gain).raw,q=b.q?p(b.q).raw:null;
  root.innerHTML=
    '<div class="eq-chip"><span>BAND</span><b>'+b.name+'</b></div>'+
    '<div class="eq-chip"><span>FREQ</span><b>'+format('hz',f)+'</b></div>'+
    '<div class="eq-chip"><span>GAIN</span><b>'+(g>=0?'+':'')+g.toFixed(1)+' dB</b></div>'+
    (q!==null?'<div class="eq-chip"><span>Q</span><b>'+q.toFixed(2)+'</b></div>':'');
}
function hitEq(cx,cy){
  if(selectedId!=='eq')return-1;
  const rect=canvas.getBoundingClientRect(),g=graphRect(),x=cx-rect.left,y=cy-rect.top;
  let best=-1,dist=1e9;
  eqBands.forEach((b,i)=>{
    const d=Math.hypot(x-fx(p(b.freq).raw,g),y-gy(p(b.gain).raw,g));
    if(d<dist){dist=d;best=i;}
  });
  return dist<=21?best:-1;
}

canvas.onpointerdown=e=>{
  const i=hitEq(e.clientX,e.clientY);if(i<0)return;
  e.preventDefault();selectedEqBand=i;
  const b=eqBands[i];eqDrag={band:i,id:e.pointerId};
  hold(b.freq,'raw');hold(b.gain,'raw');
  canvas.setPointerCapture(e.pointerId);syncEqPanel();drawAnalyzer();
};
canvas.onpointermove=e=>{
  const i=hitEq(e.clientX,e.clientY);eqHover=i;
  if(eqDrag){
    const rect=canvas.getBoundingClientRect(),g=graphRect();
    const x=clamp(e.clientX-rect.left,g.x,g.x+g.w),y=clamp(e.clientY-rect.top,g.y,g.y+g.h),b=eqBands[eqDrag.band];
    const freq=normFreq((x-g.x)/g.w,20,20000),gain=clamp((.5-(y-g.y)/g.h)*24,-12,12);
    setRaw(b.freq,freq);setRaw(b.gain,gain);syncEqPanel();drawAnalyzer();return;
  }
  if(i>=0)showEqTip(i,e.clientX,e.clientY);else hideTip();
  drawAnalyzer();
};
function endEq(){
  if(!eqDrag)return;
  const b=eqBands[eqDrag.band];
  release(b.freq);release(b.gain);eqDrag=null;
}
canvas.onpointerup=endEq;canvas.onpointercancel=endEq;
canvas.addEventListener('wheel',e=>{
  const i=hitEq(e.clientX,e.clientY);if(i<0)return;
  const b=eqBands[i];if(!b.q)return;
  e.preventDefault();selectedEqBand=i;hold(b.q,'raw');
  setRaw(b.q,clamp((p(b.q).raw||.9)*(e.deltaY<0?1.09:.92),.25,4));
  release(b.q);syncEqPanel();drawAnalyzer();
},{passive:false});
canvas.ondblclick=e=>{
  const i=hitEq(e.clientX,e.clientY);if(i<0)return;
  e.preventDefault();const b=eqBands[i];
  hold(b.freq,'raw');hold(b.gain,'raw');
  setRaw(b.freq,p(b.freq).defRaw);setRaw(b.gain,p(b.gain).defRaw);
  release(b.freq);release(b.gain);
  if(b.q){hold(b.q,'raw');setRaw(b.q,p(b.q).defRaw);release(b.q);}
  selectedEqBand=i;syncEqPanel();drawAnalyzer();
};

document.addEventListener('pointermove',e=>{
  if(e.target===canvas)return;
  const pe=e.target.closest&&e.target.closest('[data-param-id]');
  if(pe&&pe.dataset.paramId){showParamTip(pe.dataset.paramId,e.clientX,e.clientY);return;}
  const qe=e.target.closest&&e.target.closest('[data-eq-band]');
  if(qe){showEqTip(Number(qe.dataset.eqBand),e.clientX,e.clientY);return;}
  const te=e.target.closest&&e.target.closest('[data-tip]');
  if(te&&te.dataset.tip){showGenericTip(te.dataset.tip,e.clientX,e.clientY);return;}
  hideTip();
});
document.addEventListener('pointerleave',hideTip);
document.addEventListener('click',e=>{
  if(!e.target.closest('#presetControl'))closePreset();
  if(!e.target.closest('.chain-actions'))closePicker();
});

$('#presetButton').onclick=e=>{e.stopPropagation();togglePreset();};
$('#addModuleBtn').onclick=e=>{e.stopPropagation();togglePicker();};
$('#bypassBtn').onclick=()=>{setNorm('masterBypass',p('masterBypass').norm>=.5?0:1);updateBypass();};
$('.input-card').onclick=()=>selectModule('input');
$('.output-card').onclick=()=>selectModule('output');
$('#modalClose').onclick=closeHelp;
$('.modal-backdrop').onclick=closeHelp;
document.addEventListener('keydown',e=>{
  if(e.key==='Escape'){closeHelp();closePreset();closePicker();}
});

const chainScroll=$('#chainScroll');
chainScroll.addEventListener('wheel',e=>{
  if(Math.abs(e.deltaY)>=Math.abs(e.deltaX)){
    chainScroll.scrollLeft+=e.deltaY;
    e.preventDefault();
  }
},{passive:false});

function applyState(next){
  mergeRemote(next);
  buildPresets();
  updateBypass();
  updateMeters();

  const ids=chainIds();
  if(!uiBuilt){
    renderChain(true);renderPicker(true);renderModule();uiBuilt=true;
  }else{
    if(!ids.includes(selectedId)&&!moduleMap[selectedId]?.fixed)selectedId=ids[0]||'input';
    renderChain(false);renderPicker(false);syncModule();
  }

  drawAnalyzer();
}

if(backend){
  backend.addEventListener('state',applyState);
  emit('uiReady',{ready:true});
}else{
  const ids=new Set(['masterBypass']);
  modules.forEach(m=>{if(m.toggle)ids.add(m.toggle);(m.controls||[]).forEach(s=>ids.add(s.id));});
  eqBands.forEach(b=>{ids.add(b.freq);ids.add(b.gain);if(b.q)ids.add(b.q);});
  [
    'multiband','mbLowHz','mbHighHz','mbLowAmount','mbMidAmount','mbHighAmount',
    'exciterMix','exciterX1','exciterX2','exciterX3','exciterBand1','exciterBand2','exciterBand3','exciterBand4',
    'exciterMode1','exciterMode2','exciterMode3','exciterMode4',
    'imagerLowHz','imagerHighHz','imagerSafety','widthLow','widthMid','widthHigh',
    'limiterDrive','ceiling','limiterRelease','limiterCharacter','limiterUpward','limiterSoftClip','limiterSoftClipMode',
    'limiterTransient','limiterStereoTransient','limiterStereoSustain','limiterTruePeak'
  ].forEach(x=>ids.add(x));

  ids.forEach(id=>state.params[id]={norm:.5,raw:0,def:.5,defRaw:0,text:'0'});

  Object.assign(state.params,{
    cleanEqOn:{norm:1,raw:1,def:1,defRaw:1},smartGain:{norm:1,raw:1,def:1,defRaw:1},
    lowShelfHz:{norm:.25,raw:105,def:.25,defRaw:105},lowShelf:{norm:.5,raw:0,def:.5,defRaw:0},
    lowMidHz:{norm:.35,raw:320,def:.35,defRaw:320},lowMid:{norm:.46,raw:-.6,def:.46,defRaw:-.6},lowMidQ:{norm:.2,raw:.85,def:.2,defRaw:.85},
    midHz:{norm:.48,raw:900,def:.48,defRaw:900},midGain:{norm:.5,raw:0,def:.5,defRaw:0},midQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
    presenceHz:{norm:.62,raw:3200,def:.62,defRaw:3200},presence:{norm:.55,raw:.7,def:.55,defRaw:.7},presenceQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
    highMidHz:{norm:.72,raw:6200,def:.72,defRaw:6200},highMidGain:{norm:.5,raw:0,def:.5,defRaw:0},highMidQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
    airHz:{norm:.84,raw:10500,def:.84,defRaw:10500},air:{norm:.55,raw:.8,def:.55,defRaw:.8},
    exciterX1:{norm:.30,raw:180,def:.30,defRaw:180},exciterX2:{norm:.50,raw:1800,def:.50,defRaw:1800},exciterX3:{norm:.75,raw:6500,def:.75,defRaw:6500},
    exciterBand1:{norm:.05,raw:.05,def:.05,defRaw:.05},exciterBand2:{norm:.08,raw:.08,def:.08,defRaw:.08},exciterBand3:{norm:.12,raw:.12,def:.12,defRaw:.12},exciterBand4:{norm:.14,raw:.14,def:.14,defRaw:.14},
    exciterMode1:{norm:0,raw:0,def:0,defRaw:0},exciterMode2:{norm:.16,raw:1,def:.16,defRaw:1},exciterMode3:{norm:.33,raw:2,def:.33,defRaw:2},exciterMode4:{norm:.5,raw:3,def:.5,defRaw:3},
    mbLowHz:{norm:.30,raw:150,def:.30,defRaw:150},mbHighHz:{norm:.72,raw:4500,def:.72,defRaw:4500},
    imagerLowHz:{norm:.30,raw:180,def:.30,defRaw:180},imagerHighHz:{norm:.72,raw:5000,def:.72,defRaw:5000},
    widthLow:{norm:.61,raw:.92,def:.61,defRaw:.92},widthMid:{norm:.57,raw:1.02,def:.57,defRaw:1.02},widthHigh:{norm:.54,raw:1.08,def:.54,defRaw:1.08},imagerSafety:{norm:.8,raw:.8,def:.8,defRaw:.8},
    limiterDrive:{norm:.36,raw:5.0,def:.21,defRaw:3.0},ceiling:{norm:.72,raw:-.8,def:.72,defRaw:-.9},
    limiterRelease:{norm:.22,raw:110,def:.22,defRaw:120},limiterCharacter:{norm:.38,raw:3.8,def:.4,defRaw:4},
    limiterUpward:{norm:.12,raw:1.2,def:0,defRaw:0},limiterSoftClip:{norm:.06,raw:.06,def:0,defRaw:0},
    limiterSoftClipMode:{norm:.5,raw:1,def:.5,defRaw:1},limiterTransient:{norm:.14,raw:.28,def:0,defRaw:0},
    limiterStereoTransient:{norm:.18,raw:.18,def:0,defRaw:0},limiterStereoSustain:{norm:.08,raw:.08,def:0,defRaw:0},
    limiterTruePeak:{norm:1,raw:1,def:1,defRaw:1}
  });

  state.presets=['Boom Bap - DENSE PUNCH','Boom Bap - DUSTY ANALOG','Hip-Hop - MODERN DENSE','INIT - EMPTY / ALL OFF'];
  state.meters={inputPeak:-12.4,outputPeak:-.8,inputRms:-18.2,lufs:-9.4,crest:8.7,correlation:.72,limiterGR:2.8,inputGain:1.4};
  state.pre=Array.from({length:64},(_,i)=>-42+8*Math.sin(i*.13)-i*.15);
  state.post=Array.from({length:64},(_,i)=>-35+6*Math.sin(i*.12)-i*.13);
  applyState(state);
}

if(window.ResizeObserver)new ResizeObserver(resizeCanvas).observe(canvas);
window.addEventListener('resize',resizeCanvas);
requestAnimationFrame(resizeCanvas);
})();
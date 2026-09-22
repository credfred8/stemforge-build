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
const heldParams = new Set();

const modules = [
  {id:'input',fixed:true,title:'INPUT / LEVEL',sub:'Gain staging',toggle:'smartGain',
   footer:'Сначала добейтесь чистого входа с запасом по пикам. Уже после этого настраивайте тон, динамику и громкость.',
   help:'INPUT / LEVEL управляет тем, с каким уровнем сигнал входит во всю мастеринг-цепь. INPUT TRIM — ручной уровень. TARGET RMS — цель Smart Gain. SPEED — скорость его реакции. RANGE — максимальная автоматическая коррекция.',
   controls:[
     ['inputTrim','INPUT TRIM','db'],['targetInput','TARGET RMS','db'],['smartSpeed','SMART SPEED','percent'],['smartMaxGain','GAIN RANGE','db']
   ]},
  {id:'eq',title:'PARAMETRIC EQ',sub:'Graphical 6-band',toggle:'cleanEqOn',special:'eq',
   footer:'EQ оставлен графическим специально: здесь форма кривой важнее набора отдельных фейдеров.',
   help:'Шестиполосный графический параметрический EQ. Тяните точку: горизонталь меняет частоту, вертикаль gain. Колесо мыши над bell-полосой меняет Q. Двойной клик сбрасывает полосу.'},
  {id:'dynamic',title:'DYNAMIC EQ',sub:'Adaptive control',toggle:'dynamicEqOn',
   footer:'Dynamic EQ должен ловить только выпирающие области. Если он работает постоянно, сначала поправьте обычный EQ.',
   help:'Адаптивный контроль низа и верха. AMOUNT — глубина, THRESHOLD — чувствительность, ATTACK/RELEASE — скорость, XOVER — границы зон.',
   controls:[
     ['dynamicEq','AMOUNT','percent'],['dynThreshold','THRESHOLD','db'],['dynAttack','ATTACK','ms'],['dynRelease','RELEASE','ms'],['dynLowHz','LOW XOVER','hz'],['dynHighHz','HIGH XOVER','hz']
   ]},
  {id:'stabilizer',title:'STABILIZER',sub:'Resonance control',toggle:'resonanceOn',
   footer:'Используйте точечно. Если приходится сильно давить широкую область — вернитесь к EQ.',
   help:'Узкополосный контроль неприятных резонансов. FREQUENCY — центр проблемы, Q — ширина, AMOUNT — глубина.',
   controls:[['resonance','AMOUNT','percent'],['resonanceHz','FREQUENCY','hz'],['resonanceQ','Q','number']]},
  {id:'comp',title:'VINTAGE COMP',sub:'Bus glue',toggle:'glueOn',
   footer:'Для панча обычно оставляют атаку медленнее. Следите, чтобы компрессор не работал тяжело после Dynamic EQ и Multiband.',
   help:'Stereo bus compressor для склейки. THRESHOLD/RATIO задают компрессию, ATTACK сохраняет или прижимает транзиенты, RELEASE задаёт возврат, MAKEUP возвращает уровень, MIX даёт параллельную компрессию.',
   controls:[
     ['glueThreshold','THRESHOLD','db','v'],['glueRatio','RATIO','ratio','v'],['glueAttack','ATTACK','ms'],['glueRelease','RELEASE','ms'],['glueMakeup','MAKEUP','db'],['glueMix','MIX','percent'],['glue','AMOUNT','percent']
   ]},
  {id:'multiband',title:'MULTIBAND',sub:'Three-band density',toggle:'multibandOn',special:'multiband',
   footer:'Разделители полос тянутся мышью. Каждая полоса имеет отдельный фейдер плотности.',
   help:'Трёхполосная динамика. Перетаскивайте границы LOW/MID/HIGH, затем регулируйте плотность каждой полосы отдельным вертикальным фейдером.'},
  {id:'impact',title:'IMPACT',sub:'Transient design',toggle:'impactOn',
   footer:'Возвращайте удар после компрессии. Если клиппер начинает слышимо хрустеть — уменьшайте PUNCH.',
   help:'Транзиентный модуль. PUNCH усиливает атаку, SPEED меняет скорость детектора, MIX задаёт количество эффекта.',
   controls:[['impact','PUNCH','percent','v'],['impactSpeed','SPEED','percent'],['impactMix','MIX','percent']]},
  {id:'saturation',title:'SATURATION',sub:'Harmonic density',toggle:'analogOn',
   footer:'Сатурация на мастере должна ощущаться как плотность, а не как отдельный эффект.',
   help:'Мягкая гармоническая сатурация. DRIVE — количество гармоник, TONE — яркость окраски, MIX — параллельное смешивание.',
   controls:[['analog','DRIVE','percent','v'],['analogTone','TONE','percent'],['analogMix','MIX','percent']]},
  {id:'exciter',title:'EXCITER',sub:'4-band harmonics',toggle:'exciterOn',special:'exciter',
   footer:'Это многополосный exciter: перетаскивайте 3 crossover-линии, затем регулируйте гармоники каждой полосы отдельным фейдером и выбирайте характер.',
   help:'Четыре независимые полосы гармонического возбуждения. Три вертикальные границы делят спектр. Для каждой полосы доступны Amount и характер: Warm, Tape, Tube, Triode, Retro, Dual, Clean.'},
  {id:'lowend',title:'LOW END FOCUS',sub:'Mono-safe bass',toggle:'bassMonoOn',
   footer:'Низ обычно лучше держать стабильным и близким к центру. Не поднимайте MONO BELOW слишком высоко.',
   help:'Центрирует низкие частоты. MONO BELOW задаёт верхнюю границу области, AMOUNT — степень центровки.',
   controls:[['bassMonoHz','MONO BELOW','hz'],['bassMonoAmount','AMOUNT','percent','v']]},
  {id:'imager',title:'IMAGER',sub:'Three-band width',toggle:'imagerOn',special:'imager',
   footer:'Низ держите уже, верх можно расширять сильнее. SAFETY ограничивает опасное расширение при плохой корреляции.',
   help:'Трёхполосный stereo imager. Перетаскивайте crossover-линии и регулируйте ширину LOW/MID/HIGH отдельными фейдерами.'},
  {id:'clipper',title:'CLIPPER 8X',sub:'Peak shaving',toggle:'clipperOn',
   footer:'Клиппер должен снимать короткие пики до лимитера, а не превращать мастер в слышимый дисторшн.',
   help:'8x oversampled soft clipper. DRIVE — подача, CEILING — рабочий потолок, SHAPE — жёсткость, MIX — доля эффекта.',
   controls:[['clipDrive','DRIVE','db','v'],['clipCeiling','CEILING','db2','v'],['clipShape','SHAPE','percent'],['clipMix','MIX','percent']]},
  {id:'maximizer',title:'MAXIMIZER 8X',sub:'Look-ahead final level',toggle:'limiterOn',
   footer:'Главная громкость — DRIVE. Следите за LIMITER GR: постоянное сильное подавление почти всегда ухудшает панч.',
   help:'Финальный stereo-linked look-ahead limiter. DRIVE задаёт громкость, CEILING — выходной потолок, RELEASE — скорость восстановления.',
   controls:[['limiterDrive','DRIVE','db','v'],['ceiling','CEILING','db2','v'],['limiterRelease','RELEASE','ms']]},
  {id:'output',fixed:true,title:'OUTPUT',sub:'Final stage',toggle:null,
   footer:'OUTPUT TRIM нужен для точного level-match. Dither оставляйте OFF до финального экспорта, если он вообще требуется.',
   help:'Финальный выход. OUTPUT TRIM — последняя коррекция уровня, MASTER MIX — dry/wet всей основной цепи, DITHER — TPDF dither для финального экспорта.',
   controls:[['outputTrim','OUTPUT TRIM','db'],['dryWet','MASTER MIX','percent'],['ditherOn','DITHER 24-BIT','toggle']]}
];

const moduleMap = Object.fromEntries(modules.map(m => [m.id,m]));
const processingIds = modules.filter(m=>!m.fixed).map(m=>m.id);

const guidance = {
  inputTrim:['Ручной gain до всей обработки. Чем выше вход, тем сильнее будут реагировать динамика, сатурация, клиппер и лимитер.','Старт: 0 dB. Обычно -3…+3 dB. Если пики уже близко к 0 dBFS — лучше убавить.'],
  targetInput:['Цель Smart Gain по среднему уровню. Более высокое значение подаёт цепь плотнее.','Старт: -18 dBFS. Для плотного hip-hop часто -17…-15 dBFS, только если исходник чистый.'],
  smartSpeed:['Скорость изменения Smart Gain. Слишком быстрое значение может слышимо качать уровень.','Обычно 20–45%. Безопасный старт 25–35%.'],
  smartMaxGain:['Максимальная автоматическая коррекция входа.','Обычно 6–9 dB. Больше — только для очень тихого исходника.'],

  dynamicEq:['Сила динамического подавления избытка энергии.','Обычно 15–35%. Выше 45% — уже заметное вмешательство.'],
  dynThreshold:['Порог срабатывания. Чем ниже, тем чаще модуль работает.','Старт около -20 dB. Часто рабочая зона -24…-16 dB.'],
  dynAttack:['Скорость реакции на всплеск.','Обычно 10–30 ms. Быстрее = жёстче, медленнее = естественнее.'],
  dynRelease:['Скорость отпускания после всплеска.','Обычно 120–250 ms. Очень короткий Release может давать нервное движение.'],
  dynLowHz:['Граница низкой динамической зоны.','Обычно 180–350 Hz.'],
  dynHighHz:['Граница верхней динамической зоны.','Обычно 4–8 kHz.'],

  resonance:['Глубина подавления выбранного резонанса.','Обычно 10–30%. Если нужно намного больше — перепроверьте частоту.'],
  resonanceHz:['Частота проблемного свиста/жёсткости.','Часто 2–6 kHz, но ищите на слух.'],
  resonanceQ:['Ширина коррекции. Большой Q = узкая полоса.','Обычно Q 2–5.'],

  glue:['Общая интенсивность glue-компрессии.','Обычно 20–50%.'],
  glueThreshold:['Порог компрессора. Ниже порог = больше gain reduction.','Для мастера часто достаточно 1–3 dB фактического GR.'],
  glueRatio:['Степень компрессии выше порога.','Обычно 1.5:1–2.5:1.'],
  glueAttack:['Скорость срабатывания. Быстро = меньше транзиентов.','Для панча обычно 20–40 ms.'],
  glueRelease:['Скорость восстановления.','Обычно 100–250 ms.'],
  glueMakeup:['Компенсационный уровень после компрессора.','Обычно 0…+1.5 dB. Сравнивайте на похожей громкости.'],
  glueMix:['Параллельное смешивание компрессора.','Обычно 50–80%.'],
  multiband:['Общая сила многополосной динамики.','Обычно 10–35%.'],
  mbLowAmount:['Плотность LOW-полосы.','Обычно 20–45%.'],
  mbMidAmount:['Плотность MID-полосы.','Обычно 15–35%.'],
  mbHighAmount:['Плотность HIGH-полосы.','Обычно 10–30%.'],

  impact:['Усиление транзиентной атаки.','Обычно 10–40%.'],
  impactSpeed:['Скорость детектора транзиентов.','Обычно 30–65%.'],
  impactMix:['Количество Impact в итоговом сигнале.','Обычно 40–75%.'],
  analog:['Количество сатурации.','Обычно 5–25%.'],
  analogTone:['Яркость сатурации.','Обычно 40–65%.'],
  analogMix:['Количество сатурированного сигнала.','Обычно 35–65%.'],

  exciter:['Глобальная глубина гармонического возбуждения всех полос.','Обычно 20–50%.'],
  exciterMix:['Глобальный dry/wet многополосного Exciter.','Обычно 50–80%.'],
  exciterBand1:['Гармоники в самой низкой полосе.','Обычно 0–10%. С низом осторожно.'],
  exciterBand2:['Гармоники в low-mid полосе.','Обычно 3–15%.'],
  exciterBand3:['Гармоники в high-mid полосе.','Обычно 5–20%.'],
  exciterBand4:['Гармоники в верхней полосе.','Обычно 5–18%. Если AIR EQ уже поднят — меньше.'],

  bassMonoHz:['Частоты ниже этой точки постепенно центрируются.','Обычно 80–130 Hz.'],
  bassMonoAmount:['Степень центровки низа.','Обычно 70–100%.'],

  widthLow:['Ширина низкой полосы. 100% = исходная.','Обычно 70–100%.'],
  widthMid:['Ширина середины.','Обычно 95–110%.'],
  widthHigh:['Ширина верха.','Обычно 100–120%. Следите за correlation.'],
  imagerSafety:['Защита от отрицательной корреляции.','Обычно 70–100%.'],

  clipDrive:['Drive в soft clipper. Больше = больше срезанных коротких пиков.','Обычно 0.5–2.5 dB.'],
  clipCeiling:['Рабочий потолок клиппера.','Обычно -0.5…-0.2 dB.'],
  clipShape:['Форма ограничения. Меньше = мягче.','Обычно 40–65%.'],
  clipMix:['Доля клиппированного сигнала.','Обычно 80–100%.'],

  limiterDrive:['Главный регулятор финальной громкости.','Поднимайте по 0.5 dB. Для чистого мастера старайтесь держать постоянный GR примерно 1–4 dB.'],
  ceiling:['Финальный выходной потолок.','Обычно -1.0…-0.7 dBFS.'],
  limiterRelease:['Скорость отпускания лимитера.','Обычно 80–180 ms.'],

  outputTrim:['Последняя коррекция уровня после всей цепи.','Обычно -1…+1 dB. Используйте для level-match.'],
  dryWet:['Глобальный dry/wet основной цепи.','Для обычного мастеринга обычно 100%.'],
  ditherOn:['TPDF dither в самом конце.','Во время работы обычно OFF. Включайте только при необходимости финального экспорта.']
};

const eqBands = [
  {name:'LOW',freq:'lowShelfHz',gain:'lowShelf',q:null,color:'#35d8ff',guide:'Широкая low shelf. Обычно ±0.5–1.5 dB.'},
  {name:'LOW MID',freq:'lowMidHz',gain:'lowMid',q:'lowMidQ',color:'#66e3a2',guide:'Зона мути/тела. Часто -0.5…-1.5 dB, Q 0.6–1.4.'},
  {name:'MID',freq:'midHz',gain:'midGain',q:'midQ',color:'#ffc866',guide:'Середина влияет на тело и читаемость. Начинайте с ±0.5 dB.'},
  {name:'PRESENCE',freq:'presenceHz',gain:'presence',q:'presenceQ',color:'#ff8f79',guide:'Атака и разборчивость. Обычно ±0.5–1 dB.'},
  {name:'HIGH MID',freq:'highMidHz',gain:'highMidGain',q:'highMidQ',color:'#c18cff',guide:'Контролирует жёсткость и деталь.'},
  {name:'AIR',freq:'airHz',gain:'air',q:null,color:'#86eaff',guide:'Воздух и блеск. Обычно +0.3…+1.5 dB.'}
];

const p = id => state.params[id] || {norm:0,raw:0,def:0,defRaw:0,text:''};
const format = (unit,raw) => {
  if(!Number.isFinite(raw)) return '—';
  if(unit==='db') return raw.toFixed(1)+' dB';
  if(unit==='db2') return raw.toFixed(2)+' dB';
  if(unit==='hz') return raw>=1000?(raw/1000).toFixed(raw>=10000?1:2)+' kHz':Math.round(raw)+' Hz';
  if(unit==='ms') return (raw<10?raw.toFixed(1):Math.round(raw))+' ms';
  if(unit==='percent') return Math.round(raw*100)+' %';
  if(unit==='width') return Math.round(raw*100)+' %';
  if(unit==='ratio') return raw.toFixed(1)+' : 1';
  if(unit==='number') return raw.toFixed(2);
  return raw.toFixed(2);
};

function hold(id){ heldParams.add(id); emit('gesture',{id,phase:'begin'}); }
function release(id){ emit('gesture',{id,phase:'end'}); setTimeout(()=>heldParams.delete(id),90); }

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
    merged[id]=heldParams.has(id)&&old[id] ? {...incoming[id],...old[id]} : incoming[id];
  });
  state={...next,params:merged};
  if(!Array.isArray(state.chain)) state.chain=processingIds.slice();
}

function findControlSpec(id){
  for(const m of modules){
    if(!m.controls) continue;
    for(const spec of m.controls){
      if(spec[0]===id) return {id:spec[0],label:spec[1],unit:spec[2],orient:spec[3]||'h'};
    }
  }
  return null;
}

function currentContext(id){
  const m=state.meters||{};
  if(id==='targetInput'&&Number(m.inputPeak)>-6) return 'Сейчас входные пики уже высокие. Не поднимайте TARGET RMS — сначала снизьте INPUT TRIM.';
  if(['glue','glueThreshold','glueRatio'].includes(id)&&(p('dynamicEq').raw>.42||p('multiband').raw>.42)) return 'До компрессора уже стоит сильная динамическая обработка. Начинайте компрессор мягче.';
  if(id==='impact'&&p('glue').raw>.48) return 'Glue уже заметный. Начните примерно с 20–35% PUNCH.';
  if((id.startsWith('exciterBand')||id==='exciter')&&p('air').raw>1.0) return 'AIR EQ уже поднят. Exciter держите умеренно, чтобы верх не стал шершавым.';
  if((id.startsWith('width')||id==='imagerSafety')&&Number(m.correlation)<.2) return 'CORRELATION низкая. Не расширяйте сильнее; лучше уменьшите WIDTH.';
  if(id==='limiterDrive'&&Number(m.limiterGR)>4) return 'LIMITER GR уже выше 4 dB. Дополнительный Drive скорее уменьшит панч, чем улучшит громкость.';
  return '';
}

function showTipForParam(id,x,y){
  const spec=findControlSpec(id);
  const g=guidance[id];
  if(!g) return;
  $('#tooltipTitle').textContent=spec?spec.label:id;
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
  $('#tooltipRange').textContent=b.q?'Колесо мыши меняет Q. Обычно начинайте около 0.7–1.2.':'Shelf-полоса — используйте широкие и небольшие движения.';
  $('#tooltipContext').textContent=(b.name==='AIR'&&p('exciterBand4').raw>.15)?'Верхняя полоса Exciter уже активна — AIR добавляйте осторожно.':'';
  positionTip(x,y);
}
function positionTip(x,y){
  const tip=$('#tooltip');
  tip.style.left=Math.max(8,Math.min(window.innerWidth-340,x+14))+'px';
  tip.style.top=Math.max(8,Math.min(window.innerHeight-160,y+14))+'px';
  tip.classList.add('show');
}
function hideTip(){ $('#tooltip').classList.remove('show'); }

function buildPresets(){
  const menu=$('#presetMenu');
  if(menu.dataset.sig===(state.presets||[]).join('|')){ syncPreset(); return; }
  menu.dataset.sig=(state.presets||[]).join('|');
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
function togglePreset(){const open=!$('#presetMenu').classList.contains('open');$('#presetMenu').classList.toggle('open',open);$('#presetButton').classList.toggle('open',open);}
function closePreset(){$('#presetMenu').classList.remove('open');$('#presetButton').classList.remove('open');}

function chainIds(){ return Array.isArray(state.chain)?state.chain.filter(id=>moduleMap[id]&&!moduleMap[id].fixed):processingIds.slice(); }
function sendChain(ids){ state.chain=ids.slice(); emit('chain',ids); renderChain(); renderPicker(); }

function renderChain(){
  const root=$('#chain');
  root.innerHTML='';
  const ids=chainIds();

  ids.forEach(id=>{
    const m=moduleMap[id];
    const el=document.createElement('div');
    el.className='chain-item'+(selectedId===id?' active':'')+(m.toggle&&p(m.toggle).norm>=.5?' enabled':'');
    el.draggable=true;
    el.dataset.id=id;

    const power=document.createElement('button');
    power.className='chain-power';
    power.dataset.tip='Включить или выключить модуль, не удаляя его из цепочки.';
    power.onclick=e=>{e.stopPropagation();setNorm(m.toggle,p(m.toggle).norm>=.5?0:1);syncModule();renderChain();};

    const copy=document.createElement('div');
    copy.className='chain-copy';
    copy.innerHTML='<div class="chain-title">'+m.title+'</div><div class="chain-sub">'+m.sub+'</div>';

    const remove=document.createElement('button');
    remove.className='chain-remove';
    remove.textContent='×';
    remove.dataset.tip='Удалить модуль из цепочки. Настройки сохранятся и вернутся, если добавить модуль снова.';
    remove.onclick=e=>{e.stopPropagation();removeFromChain(id);};

    el.onclick=()=>selectModule(id);
    el.ondragstart=e=>{dragChainId=id;el.classList.add('dragging');e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('text/plain',id);};
    el.ondragend=()=>{dragChainId=null;$$('.chain-item').forEach(x=>x.classList.remove('dragging','drag-over'));};
    el.ondragover=e=>{if(!dragChainId||dragChainId===id)return;e.preventDefault();el.classList.add('drag-over');};
    el.ondragleave=()=>el.classList.remove('drag-over');
    el.ondrop=e=>{
      e.preventDefault();el.classList.remove('drag-over');
      if(!dragChainId||dragChainId===id)return;
      const order=chainIds();
      const from=order.indexOf(dragChainId),to=order.indexOf(id);
      if(from<0||to<0)return;
      order.splice(from,1);
      order.splice(to,0,dragChainId);
      sendChain(order);
    };

    el.append(power,copy,remove);
    root.appendChild(el);
  });
}

function renderPicker(){
  const picker=$('#modulePicker');
  const current=new Set(chainIds());
  picker.innerHTML='<div class="module-picker-title">ДОБАВИТЬ МОДУЛЬ</div>';
  processingIds.filter(id=>!current.has(id)).forEach(id=>{
    const m=moduleMap[id];
    const b=document.createElement('button');
    b.className='module-pick';
    b.textContent=m.title+'  ·  '+m.sub;
    b.onclick=e=>{e.stopPropagation();const order=chainIds();order.push(id);sendChain(order);selectModule(id);closePicker();};
    picker.appendChild(b);
  });
  if(current.size===processingIds.length){
    const empty=document.createElement('div');
    empty.className='module-picker-title';
    empty.textContent='ВСЕ МОДУЛИ УЖЕ В ЦЕПОЧКЕ';
    picker.appendChild(empty);
  }
}
function togglePicker(){const open=!$('#modulePicker').classList.contains('open');$('#modulePicker').classList.toggle('open',open);$('#addModuleBtn').classList.toggle('open',open);renderPicker();}
function closePicker(){$('#modulePicker').classList.remove('open');$('#addModuleBtn').classList.remove('open');}
function removeFromChain(id){
  const order=chainIds().filter(x=>x!==id);
  sendChain(order);
  if(selectedId===id) selectModule(order[0]||'input');
}
function selectModule(id){
  if(!moduleMap[id])return;
  selectedId=id;
  renderChain();
  renderModule();
  drawAnalyzer();
  $('#chainScroll').querySelector('[data-id="'+id+'"]')?.scrollIntoView({behavior:'smooth',block:'nearest',inline:'center'});
}

function makeHorizontal(spec){
  const id=spec[0],label=spec[1],unit=spec[2];
  const c=document.createElement('div');c.className='linear-control';c.dataset.paramId=id;
  c.innerHTML='<div class="control-label">'+label+'</div><div class="h-fader"><div class="h-fader-track"><div class="h-fader-fill"></div><div class="h-fader-thumb"></div></div></div><div class="control-value"></div>';
  const f=$('.h-fader',c),value=$('.control-value',c);
  let drag=false;
  const updateVisual=()=>{const n=clamp(p(id).norm,0,1);$('.h-fader-fill',c).style.width=(n*100)+'%';$('.h-fader-thumb',c).style.left=(n*100)+'%';value.textContent=format(unit,p(id).raw);};
  const move=e=>{const r=f.getBoundingClientRect(),n=clamp((e.clientX-r.left)/r.width,0,1);setNorm(id,n);updateVisual();};
  f.onpointerdown=e=>{e.preventDefault();drag=true;hold(id);f.setPointerCapture(e.pointerId);move(e);};
  f.onpointermove=e=>{if(drag)move(e);};
  const end=e=>{if(!drag)return;drag=false;try{f.releasePointerCapture(e.pointerId)}catch(_){}release(id);};
  f.onpointerup=end;f.onpointercancel=end;
  f.ondblclick=e=>{e.preventDefault();setNorm(id,p(id).def);updateVisual();};
  c._sync=updateVisual;updateVisual();return c;
}
function makeVertical(spec){
  const id=spec[0],label=spec[1],unit=spec[2];
  const c=document.createElement('div');c.className='v-fader-control';c.dataset.paramId=id;
  c.innerHTML='<div class="control-label">'+label+'</div><div class="v-fader"><div class="v-track"><div class="v-fill"></div><div class="v-thumb"></div></div></div><div class="control-value"></div>';
  const f=$('.v-fader',c),value=$('.control-value',c);
  let drag=false;
  const updateVisual=()=>{const n=clamp(p(id).norm,0,1);$('.v-fill',c).style.height=(n*100)+'%';$('.v-thumb',c).style.bottom=(n*100)+'%';value.textContent=format(unit,p(id).raw);};
  const move=e=>{const r=f.getBoundingClientRect(),n=clamp(1-(e.clientY-r.top)/r.height,0,1);setNorm(id,n);updateVisual();};
  f.onpointerdown=e=>{e.preventDefault();drag=true;hold(id);f.setPointerCapture(e.pointerId);move(e);};
  f.onpointermove=e=>{if(drag)move(e);};
  const end=e=>{if(!drag)return;drag=false;try{f.releasePointerCapture(e.pointerId)}catch(_){}release(id);};
  f.onpointerup=end;f.onpointercancel=end;
  f.ondblclick=e=>{e.preventDefault();setNorm(id,p(id).def);updateVisual();};
  c._sync=updateVisual;updateVisual();return c;
}
function makeToggle(spec){
  const id=spec[0],label=spec[1];
  const c=document.createElement('div');c.className='toggle-control';c.dataset.paramId=id;
  c.innerHTML='<div class="control-label">'+label+'</div><div class="big-toggle"></div><div class="control-value"></div>';
  const sync=()=>{const on=p(id).norm>=.5;$('.big-toggle',c).classList.toggle('on',on);$('.control-value',c).textContent=on?'ON':'OFF';};
  $('.big-toggle',c).onclick=()=>{setNorm(id,p(id).norm>=.5?0:1);sync();};
  c._sync=sync;sync();return c;
}

function renderEqPanel(root){
  root.classList.add('eq-mode');
  const wrap=document.createElement('div');wrap.className='eq-module-panel';
  const list=document.createElement('div');list.className='eq-band-list';
  eqBands.forEach((b,i)=>{
    const item=document.createElement('div');item.className='eq-band';item.dataset.eqBand=i;
    item.innerHTML='<div class="eq-band-top"><div class="eq-band-name">'+b.name+'</div><div class="eq-band-dot" style="background:'+b.color+'"></div></div><div class="eq-band-values"><div><span>FREQ</span><b class="eq-freq"></b></div><div><span>GAIN</span><b class="eq-gain"></b></div></div>';
    item.onclick=()=>{selectedEqBand=i;syncEqPanel();drawAnalyzer();};
    list.appendChild(item);
  });
  const info=document.createElement('div');info.className='eq-instructions';info.id='eqInstructions';
  wrap.append(list,info);root.appendChild(wrap);syncEqPanel();
}
function syncEqPanel(){
  $$('.eq-band').forEach((item,i)=>{
    const b=eqBands[i],f=p(b.freq).raw,g=p(b.gain).raw;
    item.classList.toggle('active',i===selectedEqBand);
    $('.eq-freq',item).textContent=f>=1000?(f/1000).toFixed(2)+'k':Math.round(f)+' Hz';
    $('.eq-gain',item).textContent=(g>=0?'+':'')+g.toFixed(1)+' dB';
  });
  const b=eqBands[selectedEqBand],info=$('#eqInstructions');
  if(info) info.innerHTML='<h3>'+b.name+(b.q?' · Q '+p(b.q).raw.toFixed(2):' · SHELF')+'</h3><p>'+b.guide+' Тяните точку на верхнем графике. '+(b.q?'Колесо мыши меняет Q.':'Полка остаётся широкой и музыкальной.')+'</p><div class="keys"><span class="key">DRAG = FREQ + GAIN</span>'+(b.q?'<span class="key">WHEEL = Q</span>':'')+'<span class="key">DOUBLE CLICK = RESET</span></div>';
}

function freqNorm(freq,min=20,max=20000){return Math.log(freq/min)/Math.log(max/min);}
function normFreq(n,min=20,max=20000){return min*Math.pow(max/min,clamp(n,0,1));}

function makeBandFader(id,label,unit='percent',modeId=null){
  const col=document.createElement('div');col.className='band-column';col.dataset.paramId=id;
  col.innerHTML='<div class="band-name">'+label+'</div>';
  const f=makeVertical([id,'',unit,'v']);
  $('.control-label',f).remove();
  col.appendChild(f);
  if(modeId){
    const sel=document.createElement('select');sel.className='mode-select';sel.dataset.paramId=modeId;
    ['Warm','Tape','Tube','Triode','Retro','Dual','Clean'].forEach((name,i)=>{const o=document.createElement('option');o.value=i;o.textContent=name;sel.appendChild(o);});
    sel.value=String(Math.round(p(modeId).raw||0));
    sel.onchange=()=>{hold(modeId);setRaw(modeId,Number(sel.value));release(modeId);};
    col.appendChild(sel);
  }
  return col;
}

function renderBandModule(root,type){
  root.classList.add('band-mode');
  const wrap=document.createElement('div');wrap.className='band-editor';
  const top=document.createElement('div');top.className='band-topbar';
  const work=document.createElement('div');work.className='band-work';

  const mix=document.createElement('div');mix.className='band-mix';
  let globalId=null,globalLabel='GLOBAL';
  if(type==='exciter'){globalId='exciterMix';globalLabel='MIX';}
  if(type==='multiband'){globalId='multiband';globalLabel='GLOBAL';}
  if(type==='imager'){globalId='imagerSafety';globalLabel='SAFETY';}
  mix.innerHTML='<div class="band-mix-label">'+globalLabel+'</div>';
  const global=makeHorizontal([globalId,'','percent']);
  $('.control-label',global).remove();$('.control-value',global).style.minWidth='58px';global.style.flex='1';
  mix.appendChild(global);top.appendChild(mix);
  wrap.append(top,work);root.appendChild(wrap);

  let crossovers=[],bands=[];
  if(type==='exciter'){
    crossovers=[
      {id:'exciterX1',min:60,max:1000},
      {id:'exciterX2',min:300,max:6000},
      {id:'exciterX3',min:1800,max:16000}
    ];
    bands=[
      makeBandFader('exciterBand1','LOW','percent','exciterMode1'),
      makeBandFader('exciterBand2','LOW MID','percent','exciterMode2'),
      makeBandFader('exciterBand3','HIGH MID','percent','exciterMode3'),
      makeBandFader('exciterBand4','HIGH','percent','exciterMode4')
    ];
  } else if(type==='multiband'){
    crossovers=[
      {id:'mbLowHz',min:70,max:500},
      {id:'mbHighHz',min:1800,max:12000}
    ];
    bands=[
      makeBandFader('mbLowAmount','LOW'),
      makeBandFader('mbMidAmount','MID'),
      makeBandFader('mbHighAmount','HIGH')
    ];
    work.style.gridTemplateColumns='repeat(3,1fr)';
  } else {
    crossovers=[
      {id:'imagerLowHz',min:80,max:500},
      {id:'imagerHighHz',min:1800,max:12000}
    ];
    bands=[
      makeBandFader('widthLow','LOW','width'),
      makeBandFader('widthMid','MID','width'),
      makeBandFader('widthHigh','HIGH','width')
    ];
    work.style.gridTemplateColumns='repeat(3,1fr)';
  }
  bands.forEach(b=>work.appendChild(b));

  const syncCross=()=>{
    $$('.crossover-line',work).forEach((line,i)=>{
      const c=crossovers[i];
      const n=freqNorm(p(c.id).raw,20,20000);
      line.style.left=(n*100)+'%';
      const tag=line.previousElementSibling;
      tag.style.left=(n*100)+'%';
      tag.textContent=format('hz',p(c.id).raw);
    });
  };

  crossovers.forEach((c,i)=>{
    const tag=document.createElement('div');tag.className='crossover-tag';
    const line=document.createElement('div');line.className='crossover-line';line.dataset.paramId=c.id;
    work.append(tag,line);
    let drag=false;
    const move=e=>{
      const r=work.getBoundingClientRect();
      let f=normFreq(clamp((e.clientX-r.left)/r.width,0,1),20,20000);
      f=clamp(f,c.min,c.max);
      if(type==='exciter'){
        if(i===0) f=Math.min(f,p('exciterX2').raw-100);
        if(i===1){f=Math.max(f,p('exciterX1').raw+100);f=Math.min(f,p('exciterX3').raw-250);}
        if(i===2) f=Math.max(f,p('exciterX2').raw+250);
      }else{
        if(i===0) f=Math.min(f,p(crossovers[1].id).raw-250);
        if(i===1) f=Math.max(f,p(crossovers[0].id).raw+250);
      }
      setRaw(c.id,f);syncCross();
    };
    line.onpointerdown=e=>{e.preventDefault();drag=true;hold(c.id);line.setPointerCapture(e.pointerId);move(e);};
    line.onpointermove=e=>{if(drag)move(e);};
    const end=e=>{if(!drag)return;drag=false;try{line.releasePointerCapture(e.pointerId)}catch(_){}release(c.id);};
    line.onpointerup=end;line.onpointercancel=end;
  });
  wrap._sync=()=>{syncCross();$$('.band-column [data-param-id]').forEach(el=>el._sync&&el._sync());};
  syncCross();
}

function renderModule(){
  const m=moduleMap[selectedId]||moduleMap.input;
  $('#moduleTitle').textContent=m.title;$('#moduleKicker').textContent=m.sub.toUpperCase();$('#moduleFooter').textContent=m.footer;
  const power=$('#modulePower');
  if(m.toggle){power.style.display='';power.dataset.paramId=m.toggle;power.onclick=()=>{setNorm(m.toggle,p(m.toggle).norm>=.5?0:1);syncModule();renderChain();};}
  else{power.style.display='none';power.onclick=null;delete power.dataset.paramId;}
  $('#helpBtn').onclick=()=>openHelp(m);
  const removable=!m.fixed&&chainIds().includes(m.id);
  $('#removeModuleBtn').style.display=removable?'':'none';
  $('#removeModuleBtn').onclick=()=>removeFromChain(m.id);

  const root=$('#moduleControls');root.className='controls-grid';root.innerHTML='';
  if(m.special==='eq') renderEqPanel(root);
  else if(['exciter','multiband','imager'].includes(m.special||m.id)) renderBandModule(root,m.special||m.id);
  else (m.controls||[]).forEach(spec=>{
    if(spec[2]==='toggle') root.appendChild(makeToggle(spec));
    else if(spec[3]==='v') root.appendChild(makeVertical(spec));
    else root.appendChild(makeHorizontal(spec));
  });
  syncModule();
}
function syncModule(){
  const m=moduleMap[selectedId]||moduleMap.input;
  if(m.toggle) $('#modulePower').classList.toggle('on',p(m.toggle).norm>=.5);
  $$('#moduleControls [data-param-id]').forEach(el=>{if(el._sync)el._sync();});
  $$('#moduleControls .linear-control, #moduleControls .v-fader-control, #moduleControls .toggle-control').forEach(el=>{if(el._sync)el._sync();});
  if(m.special==='eq')syncEqPanel();
  const band=$('#moduleControls .band-editor');if(band&&band._sync)band._sync();
}

function openHelp(m){$('#modalTitle').textContent=m.title;$('#modalText').textContent=m.help;$('#modal').classList.add('open');}
function closeHelp(){$('#modal').classList.remove('open');}

function updateBypass(){$('#bypassBtn').classList.toggle('on',p('masterBypass').norm>=.5);}
function meterNorm(db){return Number.isFinite(db)?clamp((db+60)/60,0,1):0;}
function dbText(db,suffix=''){return Number.isFinite(db)&&db>-99?db.toFixed(1)+suffix:'-∞';}
function updateMeters(){
  const m=state.meters||{},ip=Number(m.inputPeak??-100),op=Number(m.outputPeak??-100);
  $('#inMeter').style.height=(meterNorm(ip)*100)+'%';$('#outMeter').style.height=(meterNorm(op)*100)+'%';
  $('#inPeak').textContent=dbText(ip,' dBFS');$('#outPeak').textContent=dbText(op,' dBFS');
  $('#lufs').textContent=dbText(Number(m.lufs??-100));$('#crest').textContent=Number(m.crest??0).toFixed(1)+' dB';
  $('#corr').textContent=Number(m.correlation??0).toFixed(2);$('#gr').textContent=Number(m.limiterGR??0).toFixed(1)+' dB';$('#inputGain').textContent=Number(m.inputGain??0).toFixed(1)+' dB';
  const badge=$('#qualityBadge'),corr=Number(m.correlation??0),gr=Number(m.limiterGR??0);
  if(corr<-.05){badge.textContent='PHASE';badge.style.color='var(--red)';}
  else if(gr>7){badge.textContent='HARD';badge.style.color='var(--amber)';}
  else{badge.textContent='CLEAN';badge.style.color='var(--green)';}
  const rms=Number(m.inputRms??-100),coach=$('#coach');coach.classList.remove('warn','hot');
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
  if(!a||!a.length)return;ctx.beginPath();
  a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
  if(fill){ctx.lineTo(g.x+g.w,g.y+g.h);ctx.lineTo(g.x,g.y+g.h);ctx.closePath();const z=ctx.createLinearGradient(0,g.y,0,g.y+g.h);z.addColorStop(0,'rgba(53,216,255,.14)');z.addColorStop(1,'rgba(53,216,255,0)');ctx.fillStyle=z;ctx.fill();ctx.beginPath();a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});}
  ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();
}
function drawAnalyzer(){
  const r=canvas.getBoundingClientRect();if(!r.width||!r.height)return;const g=graphRect();ctx.clearRect(0,0,r.width,r.height);
  const bg=ctx.createLinearGradient(0,g.y,0,g.y+g.h);bg.addColorStop(0,'rgba(20,33,45,.5)');bg.addColorStop(1,'rgba(4,8,12,.08)');ctx.fillStyle=bg;ctx.fillRect(g.x,g.y,g.w,g.h);
  ctx.font='8px Segoe UI';ctx.textBaseline='bottom';
  [20,50,100,200,500,1000,2000,5000,10000,20000].forEach(f=>{const x=fx(f,g);ctx.strokeStyle='rgba(69,95,117,.24)';ctx.beginPath();ctx.moveTo(x,g.y);ctx.lineTo(x,g.y+g.h);ctx.stroke();ctx.fillStyle='rgba(100,122,142,.62)';ctx.textAlign=f===20?'left':f===20000?'right':'center';ctx.fillText(f>=1000?(f/1000)+'k':String(f),x,g.y+g.h-3);});
  [-12,-6,0,6,12].forEach(db=>{const y=gy(db,g);ctx.strokeStyle=db===0?'rgba(108,139,162,.34)':'rgba(69,95,117,.17)';ctx.beginPath();ctx.moveTo(g.x,y);ctx.lineTo(g.x+g.w,y);ctx.stroke();});
  drawSpectrum(state.pre,g,'rgba(119,139,158,.36)',1,false);drawSpectrum(state.post,g,'rgba(53,216,255,.9)',1.6,true);
  ctx.beginPath();const n=Math.max(240,Math.floor(g.w));for(let i=0;i<n;i++){const u=i/(n-1),f=normFreq(u,20,20000),x=g.x+u*g.w,y=gy(eqAt(f),g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);}
  const grad=ctx.createLinearGradient(g.x,0,g.x+g.w,0);grad.addColorStop(0,'#35d8ff');grad.addColorStop(.5,'#9c8cff');grad.addColorStop(1,'#86eaff');ctx.strokeStyle=grad;ctx.lineWidth=2.2;ctx.stroke();
  if(selectedId==='eq')eqBands.forEach((b,i)=>{const x=fx(p(b.freq).raw,g),y=gy(p(b.gain).raw,g),active=i===selectedEqBand,hover=i===eqHover;ctx.beginPath();ctx.arc(x,y,active?8:6,0,Math.PI*2);ctx.fillStyle=active?b.color:'#081018';ctx.fill();ctx.strokeStyle=b.color;ctx.lineWidth=active||hover?2.2:1.5;ctx.stroke();if(active){ctx.beginPath();ctx.arc(x,y,14,0,Math.PI*2);ctx.strokeStyle=b.color+'55';ctx.stroke();}});
  renderEqInspector();
}
function renderEqInspector(){
  const b=eqBands[selectedEqBand],f=p(b.freq).raw,g=p(b.gain).raw,q=b.q?p(b.q).raw:null,root=$('#eqInspector');
  root.innerHTML='<div class="eq-chip"><span>BAND</span><b>'+b.name+'</b></div><div class="eq-chip"><span>FREQ</span><b>'+format('hz',f)+'</b></div><div class="eq-chip"><span>GAIN</span><b>'+(g>=0?'+':'')+g.toFixed(1)+' dB</b></div>'+(q!==null?'<div class="eq-chip"><span>Q</span><b>'+q.toFixed(2)+'</b></div>':'');
}
function hitEq(cx,cy){
  if(selectedId!=='eq')return-1;const rect=canvas.getBoundingClientRect(),g=graphRect(),x=cx-rect.left,y=cy-rect.top;let best=-1,dist=1e9;
  eqBands.forEach((b,i)=>{const d=Math.hypot(x-fx(p(b.freq).raw,g),y-gy(p(b.gain).raw,g));if(d<dist){dist=d;best=i;}});
  return dist<=21?best:-1;
}
canvas.onpointerdown=e=>{
  const i=hitEq(e.clientX,e.clientY);if(i<0)return;e.preventDefault();
  selectedEqBand=i;const b=eqBands[i];eqDrag={band:i,id:e.pointerId};
  hold(b.freq);hold(b.gain);canvas.setPointerCapture(e.pointerId);syncEqPanel();drawAnalyzer();
};
canvas.onpointermove=e=>{
  const i=hitEq(e.clientX,e.clientY);eqHover=i;
  if(eqDrag){
    const rect=canvas.getBoundingClientRect(),g=graphRect(),x=clamp(e.clientX-rect.left,g.x,g.x+g.w),y=clamp(e.clientY-rect.top,g.y,g.y+g.h),b=eqBands[eqDrag.band];
    const freq=normFreq((x-g.x)/g.w,20,20000),gain=clamp((.5-(y-g.y)/g.h)*24,-12,12);
    setRaw(b.freq,freq);setRaw(b.gain,gain);syncEqPanel();drawAnalyzer();return;
  }
  if(i>=0)showEqTip(i,e.clientX,e.clientY);else hideTip();drawAnalyzer();
};
function endEq(){
  if(!eqDrag)return;const b=eqBands[eqDrag.band];release(b.freq);release(b.gain);eqDrag=null;
}
canvas.onpointerup=endEq;canvas.onpointercancel=endEq;
canvas.addEventListener('wheel',e=>{
  const i=hitEq(e.clientX,e.clientY);if(i<0)return;const b=eqBands[i];if(!b.q)return;e.preventDefault();selectedEqBand=i;hold(b.q);setRaw(b.q,clamp((p(b.q).raw||.9)*(e.deltaY<0?1.09:.92),.25,4));release(b.q);syncEqPanel();drawAnalyzer();
},{passive:false});
canvas.ondblclick=e=>{
  const i=hitEq(e.clientX,e.clientY);if(i<0)return;const b=eqBands[i];e.preventDefault();
  hold(b.freq);hold(b.gain);setRaw(b.freq,p(b.freq).defRaw);setRaw(b.gain,p(b.gain).defRaw);release(b.freq);release(b.gain);
  if(b.q){hold(b.q);setRaw(b.q,p(b.q).defRaw);release(b.q);}selectedEqBand=i;syncEqPanel();drawAnalyzer();
};

document.addEventListener('pointermove',e=>{
  if(e.target===canvas)return;
  const pe=e.target.closest&&e.target.closest('[data-param-id]');if(pe&&pe.dataset.paramId){showTipForParam(pe.dataset.paramId,e.clientX,e.clientY);return;}
  const qe=e.target.closest&&e.target.closest('[data-eq-band]');if(qe){showEqTip(Number(qe.dataset.eqBand),e.clientX,e.clientY);return;}
  const te=e.target.closest&&e.target.closest('[data-tip]');if(te&&te.dataset.tip){showGenericTip(te.dataset.tip,e.clientX,e.clientY);return;}
  hideTip();
});
document.addEventListener('pointerleave',hideTip);
document.addEventListener('click',e=>{if(!e.target.closest('#presetControl'))closePreset();if(!e.target.closest('.chain-actions'))closePicker();});

$('#presetButton').onclick=e=>{e.stopPropagation();togglePreset();};
$('#addModuleBtn').onclick=e=>{e.stopPropagation();togglePicker();};
$('#bypassBtn').onclick=()=>{setNorm('masterBypass',p('masterBypass').norm>=.5?0:1);updateBypass();};
$('.input-card').onclick=()=>selectModule('input');
$('.output-card').onclick=()=>selectModule('output');
$('#modalClose').onclick=closeHelp;$('.modal-backdrop').onclick=closeHelp;
document.addEventListener('keydown',e=>{if(e.key==='Escape'){closeHelp();closePreset();closePicker();}});

function applyState(next){
  mergeRemote(next);
  buildPresets();updateBypass();updateMeters();
  if(!uiBuilt){renderChain();renderPicker();renderModule();uiBuilt=true;}
  else{
    if(!chainIds().includes(selectedId)&&!moduleMap[selectedId]?.fixed) selectedId=chainIds()[0]||'input';
    renderChain();renderPicker();syncModule();
  }
  drawAnalyzer();
}

if(backend){
  backend.addEventListener('state',applyState);
  emit('uiReady',{ready:true});
}else{
  const allIds=new Set(['masterBypass']);
  modules.forEach(m=>{if(m.toggle)allIds.add(m.toggle);(m.controls||[]).forEach(s=>allIds.add(s[0]));});
  eqBands.forEach(b=>{allIds.add(b.freq);allIds.add(b.gain);if(b.q)allIds.add(b.q);});
  ['exciterMix','exciterX1','exciterX2','exciterX3','exciterBand1','exciterBand2','exciterBand3','exciterBand4','exciterMode1','exciterMode2','exciterMode3','exciterMode4','mbLowHz','mbHighHz','imagerLowHz','imagerHighHz','imagerSafety','widthLow','widthMid','widthHigh'].forEach(x=>allIds.add(x));
  allIds.forEach(id=>state.params[id]={norm:.5,raw:0,def:.5,defRaw:0});
  Object.assign(state.params,{
    cleanEqOn:{norm:1,raw:1,def:1,defRaw:1},smartGain:{norm:1,raw:1,def:1,defRaw:1},
    lowShelfHz:{norm:.25,raw:105,def:.25,defRaw:105},lowShelf:{norm:.5,raw:0,def:.5,defRaw:0},
    lowMidHz:{norm:.35,raw:320,def:.35,defRaw:320},lowMid:{norm:.46,raw:-.6,def:.46,defRaw:-.6},lowMidQ:{norm:.2,raw:.85,def:.2,defRaw:.85},
    midHz:{norm:.48,raw:900,def:.48,defRaw:900},midGain:{norm:.5,raw:0,def:.5,defRaw:0},midQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
    presenceHz:{norm:.62,raw:3200,def:.62,defRaw:3200},presence:{norm:.55,raw:.7,def:.55,defRaw:.7},presenceQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
    highMidHz:{norm:.72,raw:6200,def:.72,defRaw:6200},highMidGain:{norm:.5,raw:0,def:.5,defRaw:0},highMidQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
    airHz:{norm:.84,raw:10500,def:.84,defRaw:10500},air:{norm:.55,raw:.8,def:.55,defRaw:.8},
    exciterX1:{norm:.3,raw:180,def:.3,defRaw:180},exciterX2:{norm:.5,raw:1800,def:.5,defRaw:1800},exciterX3:{norm:.75,raw:6500,def:.75,defRaw:6500},
    exciterBand1:{norm:.05,raw:.05,def:.05,defRaw:.05},exciterBand2:{norm:.08,raw:.08,def:.08,defRaw:.08},exciterBand3:{norm:.12,raw:.12,def:.12,defRaw:.12},exciterBand4:{norm:.14,raw:.14,def:.14,defRaw:.14},
    exciterMode1:{norm:0,raw:0,def:0,defRaw:0},exciterMode2:{norm:.16,raw:1,def:.16,defRaw:1},exciterMode3:{norm:.33,raw:2,def:.33,defRaw:2},exciterMode4:{norm:.5,raw:3,def:.5,defRaw:3},
    mbLowHz:{norm:.3,raw:150,def:.3,defRaw:150},mbHighHz:{norm:.72,raw:4500,def:.72,defRaw:4500},
    imagerLowHz:{norm:.3,raw:180,def:.3,defRaw:180},imagerHighHz:{norm:.72,raw:5000,def:.72,defRaw:5000},
    widthLow:{norm:.61,raw:.92,def:.61,defRaw:.92},widthMid:{norm:.57,raw:1.02,def:.57,defRaw:1.02},widthHigh:{norm:.54,raw:1.08,def:.54,defRaw:1.08},imagerSafety:{norm:.8,raw:.8,def:.8,defRaw:.8}
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
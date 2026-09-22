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
let uiBuilt = false;
let presetSignature = '';

const defs = [
 ['input','INPUT / LEVEL','Gain staging','smartGain',
  'Сначала выставьте вход. Если микс уже громкий, не добавляйте лишний gain до динамики.',
  'Этот блок задаёт уровень, с которым весь микс входит в мастеринг-цепь. INPUT TRIM — ручная коррекция. TARGET RMS — цель Smart Gain. SPEED определяет скорость адаптации, RANGE ограничивает автоматическое усиление.',
  [['inputTrim','INPUT TRIM','db'],['targetInput','TARGET RMS','db'],['smartSpeed','SMART SPEED','percent'],['smartMaxGain','GAIN RANGE','db']]],
 ['eq','PARAMETRIC EQ','Graphical 6-band','cleanEqOn',
  'Двигайте точки на графике. На мастеринге обычно достаточно небольших движений.',
  'Шестиполосный графический параметрический EQ. LOW и AIR — полки, четыре средние точки — bell. По горизонтали меняется частота, по вертикали gain, колесом мыши — Q. Двойной клик сбрасывает выбранную полосу.',
  []],
 ['dynamic','DYNAMIC EQ','Adaptive control','dynamicEqOn',
  'Сначала исправьте постоянный тональный баланс EQ, потом динамически ловите только выпирающие места.',
  'Dynamic EQ приглушает избыток низа и верха только в моменты всплесков. AMOUNT задаёт силу, THRESHOLD чувствительность, ATTACK/RELEASE скорость, XOVER границы зон.',
  [['dynamicEq','AMOUNT','percent'],['dynThreshold','THRESHOLD','db'],['dynAttack','ATTACK','ms'],['dynRelease','RELEASE','ms'],['dynLowHz','LOW XOVER','hz'],['dynHighHz','HIGH XOVER','hz']]],
 ['stabilizer','STABILIZER','Resonance control','resonanceOn',
  'Используйте только если слышите жёсткий свист или колкость. Не делайте широкую глубокую яму без причины.',
  'Узкополосный контроль резонансов. FREQUENCY ищет проблемную частоту, Q задаёт ширину, AMOUNT — глубину подавления.',
  [['resonance','AMOUNT','percent'],['resonanceHz','FREQUENCY','hz'],['resonanceQ','Q','number']]],
 ['comp','VINTAGE COMP','Bus glue','glueOn',
  'Для панча держите атаку медленнее. Если микс уже сильно прижат Dynamic EQ/Multiband, компрессор делайте мягче.',
  'Stereo bus-компрессор для склейки. THRESHOLD и RATIO определяют степень компрессии, ATTACK сохраняет или прижимает удар, RELEASE задаёт восстановление, MAKEUP возвращает уровень, MIX позволяет параллельную компрессию.',
  [['glue','AMOUNT','percent'],['glueThreshold','THRESHOLD','db'],['glueRatio','RATIO','ratio'],['glueAttack','ATTACK','ms'],['glueRelease','RELEASE','ms'],['glueMakeup','MAKEUP','db'],['glueMix','MIX','percent']]],
 ['multiband','MULTIBAND','Three-zone density','multibandOn',
  'Уплотняйте только проблемную полосу. Если весь микс уже ровный, GLOBAL оставляйте небольшим.',
  'Трёхполосная динамика. XOVER делят сигнал на LOW/MID/HIGH. GLOBAL задаёт общую силу, отдельные LOW/MID/HIGH — плотность каждой зоны.',
  [['multiband','GLOBAL','percent'],['mbLowHz','LOW XOVER','hz'],['mbHighHz','HIGH XOVER','hz'],['mbLowAmount','LOW','percent'],['mbMidAmount','MID','percent'],['mbHighAmount','HIGH','percent']]],
 ['impact','IMPACT','Transient design','impactOn',
  'Добавляйте punch после компрессии. Если кик или снейр начинают щёлкать — уменьшайте PUNCH/MIX.',
  'Транзиентный модуль возвращает атаку после плотной динамической обработки. PUNCH — сила, SPEED — скорость детектора, MIX — количество обработанного сигнала.',
  [['impact','PUNCH','percent'],['impactSpeed','SPEED','percent'],['impactMix','MIX','percent']]],
 ['saturation','SATURATION','Harmonic density','analogOn',
  'На мастер-шине лучше чуть-чуть. Сильная сатурация быстро делает верх шершавым и съедает глубину.',
  'Мягкая гармоническая сатурация. DRIVE добавляет гармоники и плотность, TONE меняет яркость окраски, MIX подмешивает обработку параллельно.',
  [['analog','DRIVE','percent'],['analogTone','TONE','percent'],['analogMix','MIX','percent']]],
 ['exciter','EXCITER','Upper harmonics','exciterOn',
  'Если AIR уже поднят в EQ, Exciter нужен меньше. Сначала сравните яркость без него.',
  'Exciter создаёт новые верхние гармоники. FREQUENCY задаёт область, AMOUNT интенсивность, MIX количество эффекта.',
  [['exciter','AMOUNT','percent'],['exciterHz','FREQUENCY','hz'],['exciterMix','MIX','percent']]],
 ['lowend','LOW END FOCUS','Mono-safe bass','bassMonoOn',
  'Саб обычно полезно держать в центре. Не поднимайте MONO BELOW слишком высоко, иначе микс станет узким.',
  'Центрирует низкие частоты для стабильного перевода на разные системы. MONO BELOW — граница, AMOUNT — степень центровки.',
  [['bassMonoHz','MONO BELOW','hz'],['bassMonoAmount','AMOUNT','percent']]],
 ['imager','IMAGER','Three-band width','imagerOn',
  'Низ обычно не расширяют. Если CORRELATION приближается к нулю или уходит в минус — уменьшайте ширину.',
  'Трёхполосный stereo imager. LOW/MID/HIGH WIDTH задают ширину зон, XOVER их границы, SAFETY автоматически ограничивает опасное расширение.',
  [['widthLow','LOW WIDTH','width'],['widthMid','MID WIDTH','width'],['widthHigh','HIGH WIDTH','width'],['imagerLowHz','LOW XOVER','hz'],['imagerHighHz','HIGH XOVER','hz'],['imagerSafety','SAFETY','percent']]],
 ['clipper','CLIPPER 8X','Peak shaving','clipperOn',
  'Клиппер должен снимать короткие пики до лимитера. Хруст и песок = слишком много DRIVE или слишком жёсткий SHAPE.',
  '8x oversampled soft clipper. DRIVE подаёт сигнал в клиппер, CEILING задаёт рабочий потолок, SHAPE — форму ограничения, MIX — долю эффекта.',
  [['clipDrive','DRIVE','db'],['clipCeiling','CEILING','db2'],['clipShape','SHAPE','percent'],['clipMix','MIX','percent']]],
 ['maximizer','MAXIMIZER 8X','Look-ahead final level','limiterOn',
  'DRIVE — главный регулятор финальной громкости. Следите за LIMITER GR справа: постоянные большие значения съедают панч.',
  'Финальный stereo-linked look-ahead limiter. DRIVE задаёт громкость, CEILING — выходной потолок, RELEASE — скорость восстановления.',
  [['limiterDrive','DRIVE','db'],['ceiling','CEILING','db2'],['limiterRelease','RELEASE','ms']]],
 ['output','OUTPUT','Final trim & dither',null,
  'OUTPUT TRIM используйте для точного level-match. Dither нужен только при финальном экспорте с уменьшением битности.',
  'Финальный выход. OUTPUT TRIM корректирует уровень после мастеринга, MASTER MIX смешивает dry/wet. DITHER обычно оставляют выключенным до финального рендера.',
  [['outputTrim','OUTPUT TRIM','db'],['dryWet','MASTER MIX','percent'],['ditherOn','DITHER 24-BIT','toggle']]]
];

const modules = defs.map((d,i) => ({
 id:d[0], title:d[1], sub:d[2], toggle:d[3], footer:d[4], help:d[5], special:i===1?'eq':null,
 params:d[6].map(x => ({id:x[0],label:x[1],unit:x[2],type:x[2]==='toggle'?'toggle':null}))
}));

const guidance = {
 inputTrim:{body:'Ручной gain до всей обработки. Он влияет на то, насколько сильно будут работать компрессор, сатурация, клиппер и лимитер.',range:'Старт: 0 dB. Обычно: -3…+3 dB. Если входные пики уже выше -6 dBFS — чаще лучше убавить, а не прибавлять.'},
 targetInput:{body:'Цель Smart Gain по среднему уровню. Чем значение выше, тем плотнее сигнал входит в последующие блоки.',range:'Старт: -18 dBFS. Плотный hip-hop: примерно -17…-15 dBFS, только если исходник чистый и без клиппинга.'},
 smartSpeed:{body:'Скорость, с которой Smart Gain догоняет целевой уровень. Слишком быстрое значение может слышимо качать громкость.',range:'Обычно: 20–45%. Для естественного мастеринга начинайте с 25–35%.'},
 smartMaxGain:{body:'Максимум, на который Smart Gain может поднять или опустить вход.',range:'Обычно: 6–9 dB. Больший диапазон нужен только для очень тихого или неровного исходника.'},

 dynamicEq:{body:'Общая сила динамического подавления низа и верха, когда они становятся избыточными.',range:'Обычно: 15–35%. Выше 45% — уже заметная коррекция, используйте только при реальной проблеме.'},
 dynThreshold:{body:'Порог срабатывания Dynamic EQ. Ниже значение — модуль реагирует чаще.',range:'Старт: около -20 dB. Часто рабочая зона: -24…-16 dB.'},
 dynAttack:{body:'Как быстро Dynamic EQ реагирует на всплеск.',range:'Обычно: 10–30 ms. Быстрее — жёстче и чище; медленнее — естественнее и ударнее.'},
 dynRelease:{body:'Как быстро подавление отпускает после всплеска.',range:'Обычно: 120–250 ms. Слишком короткий Release может давать нервное движение.'},
 dynLowHz:{body:'Граница низкой динамической зоны.',range:'Обычно: 180–350 Hz. Ниже — больше контроль саба, выше — захватывается нижняя середина.'},
 dynHighHz:{body:'Граница верхней динамической зоны.',range:'Обычно: 4–8 kHz. Ниже — сильнее контролируется резкость, выше — в основном воздух и тарелки.'},

 resonance:{body:'Глубина подавления выбранного резонанса.',range:'Обычно: 10–30%. Если нужно больше 40%, сначала проверьте, точно ли выбрана правильная частота.'},
 resonanceHz:{body:'Частота, где слышится свист, звон или неприятная жёсткость.',range:'Ищите на слух. Часто проблемная зона мастера: примерно 2–6 kHz, но это зависит от микса.'},
 resonanceQ:{body:'Ширина подавления. Большой Q = узкая точечная коррекция.',range:'Обычно: Q 2–5. Для очень узкого свиста можно выше; для общей жёсткости — ниже.'},

 glue:{body:'Общая интенсивность bus-компрессии.',range:'Обычно: 20–50%. Цель — склейка без ощущения, что микс "сел".'},
 glueThreshold:{body:'Порог компрессора. Ниже порог — больше gain reduction.',range:'Настраивайте по результату: для мастера часто достаточно 1–3 dB реального gain reduction.'},
 glueRatio:{body:'Насколько сильно компрессор давит сигнал выше порога.',range:'Обычно: 1.5:1–2.5:1. Для мастеринга редко нужен высокий Ratio.'},
 glueAttack:{body:'Скорость срабатывания. Быстрая атака сильнее съедает кик и снейр.',range:'Для панча: примерно 20–40 ms. Для более мягкого контроля можно 5–20 ms.'},
 glueRelease:{body:'Скорость восстановления после компрессии.',range:'Обычно: 100–250 ms. Подбирайте так, чтобы компрессор успевал отпустить к следующему сильному удару.'},
 glueMakeup:{body:'Компенсация громкости после компрессии.',range:'Обычно: 0…+1.5 dB. Сравнивайте bypass на похожей громкости, иначе громче почти всегда кажется лучше.'},
 glueMix:{body:'Баланс обработанного и исходного сигнала внутри компрессора.',range:'Обычно: 50–80%. Меньше Mix = больше сохранённых транзиентов.'},

 multiband:{body:'Общая сила трёхполосной динамической обработки.',range:'Обычно: 10–35%. Если нужно 50%+, проверьте сначала EQ и обычный компрессор.'},
 mbLowHz:{body:'Граница LOW/MID в Multiband.',range:'Обычно: 100–220 Hz. Ниже — только саб/бас, выше — захватывается тело микса.'},
 mbHighHz:{body:'Граница MID/HIGH в Multiband.',range:'Обычно: 3.5–7 kHz.'},
 mbLowAmount:{body:'Насколько сильно уплотняется низкая полоса.',range:'Обычно: 20–45%. Если бас уже ровный после Dynamic EQ — держите ниже.'},
 mbMidAmount:{body:'Плотность средней полосы, где находится большая часть музыкальной информации.',range:'Обычно: 15–35%. Слишком много делает микс плоским.'},
 mbHighAmount:{body:'Плотность верхней полосы.',range:'Обычно: 10–30%. Высокие значения могут убрать воздух и живость.'},

 impact:{body:'Возвращает атаку транзиентов после компрессии.',range:'Обычно: 10–40%. Для drum-heavy материала можно больше, но следите за клиппером.'},
 impactSpeed:{body:'Скорость детектора транзиентов.',range:'Обычно: 30–65%. Быстрее сильнее цепляется за короткие удары.'},
 impactMix:{body:'Количество транзиентной обработки в итоговом сигнале.',range:'Обычно: 40–75%.'},

 analog:{body:'Количество гармонической сатурации и плотности.',range:'Обычно: 5–25%. На мастер-шине маленькие значения почти всегда безопаснее.'},
 analogTone:{body:'Тон сатурации: влево темнее, вправо ярче.',range:'Обычно: 40–65%. Если микс уже яркий после EQ/Exciter — держите ближе к середине или ниже.'},
 analogMix:{body:'Доля сатурированного сигнала.',range:'Обычно: 35–65%. Для сохранения глубины начинайте около 50%.'},

 exciter:{body:'Количество новых верхних гармоник.',range:'Обычно: 4–15%. Выше 20% на мастере быстро становится слышимым эффектом.'},
 exciterHz:{body:'Область, где Exciter становится наиболее заметным.',range:'Обычно: 6–10 kHz. Ниже — больше присутствия, выше — больше воздуха.'},
 exciterMix:{body:'Доля Exciter в итоговом сигнале.',range:'Обычно: 25–50%.'},

 bassMonoHz:{body:'Частоты ниже этой точки постепенно собираются в центр.',range:'Обычно: 80–130 Hz. Для очень широкого баса можно выше, но осторожно.'},
 bassMonoAmount:{body:'Степень центровки низких частот.',range:'Обычно: 70–100%. Для клубного/рэп-мастера часто удобно 100% ниже выбранной частоты.'},

 widthLow:{body:'Ширина низкой полосы. 100% = исходная ширина.',range:'Обычно: 70–100%. Саб редко стоит расширять.'},
 widthMid:{body:'Ширина середины.',range:'Обычно: 95–110%. Маленькое расширение обычно звучит естественнее.'},
 widthHigh:{body:'Ширина верхней полосы.',range:'Обычно: 100–120%. Следите за CORRELATION.'},
 imagerLowHz:{body:'Граница низкой полосы stereo imager.',range:'Обычно: 120–220 Hz.'},
 imagerHighHz:{body:'Граница верхней полосы stereo imager.',range:'Обычно: 4–7 kHz.'},
 imagerSafety:{body:'Насколько активно плагин ограничивает расширение при плохой фазовой корреляции.',range:'Обычно: 70–100%. Для безопасного мастера оставляйте ближе к 100%.'},

 clipDrive:{body:'Уровень, которым сигнал подаётся в soft clipper. Больше Drive = больше срезанных коротких пиков.',range:'Обычно: 0.5–2.5 dB. Если слышите хруст — уменьшайте Drive.'},
 clipCeiling:{body:'Рабочая граница клиппера перед финальным лимитером.',range:'Обычно: -0.5…-0.2 dB. Финальный Ceiling всё равно задаётся в Maximizer.'},
 clipShape:{body:'Форма ограничения: меньше = мягче, больше = жёстче.',range:'Обычно: 40–65%. Жёсткий shape громче, но быстрее даёт слышимые артефакты.'},
 clipMix:{body:'Доля клиппированного сигнала.',range:'Обычно: 80–100%. Если клиппер используется как тонкий эффект — можно меньше.'},

 limiterDrive:{body:'Главный регулятор финальной громкости. Чем выше Drive, тем больше limiter gain reduction.',range:'Поднимайте по 0.5 dB и следите за LIMITER GR. Для чистого мастера старайтесь держать постоянный GR примерно в пределах 1–4 dB.'},
 ceiling:{body:'Финальный максимальный уровень выхода.',range:'Обычно: -1.0…-0.7 dBFS для безопасного true-peak запаса. Для специальных задач можно иначе.'},
 limiterRelease:{body:'Как быстро лимитер отпускает после пика.',range:'Обычно: 80–180 ms. Слишком быстро может давать зернистость, слишком медленно — съедать удар.'},

 outputTrim:{body:'Финальная ручная коррекция после всей цепи.',range:'Обычно: -1…+1 dB. Используйте для level-match, а не для достижения громкости.'},
 dryWet:{body:'Глобальный dry/wet основной мастеринг-цепи.',range:'Для полноценного мастеринга обычно 100%. Уменьшайте только для параллельного характера.'},
 ditherOn:{body:'Очень тихий TPDF dither в самом конце.',range:'Обычно OFF во время работы. Включайте только при финальном экспорте, если реально уменьшается битность.'}
};

const eqBands = [
 {name:'LOW',freq:'lowShelfHz',gain:'lowShelf',q:null,color:'#34d8ff',gainGuide:'Обычно ±0.5–1.5 dB. Используйте для общего веса низа.'},
 {name:'LOW MID',freq:'lowMidHz',gain:'lowMid',q:'lowMidQ',color:'#65e2a2',gainGuide:'Часто здесь убирают муть. Обычно -0.5…-1.5 dB, Q около 0.6–1.4.'},
 {name:'MID',freq:'midHz',gain:'midGain',q:'midQ',color:'#ffc766',gainGuide:'Середина влияет на тело и читаемость. Начинайте с ±0.5 dB.'},
 {name:'PRESENCE',freq:'presenceHz',gain:'presence',q:'presenceQ',color:'#ff8f79',gainGuide:'Зона атаки и разборчивости. Обычно ±0.5–1.0 dB.'},
 {name:'HIGH MID',freq:'highMidHz',gain:'highMidGain',q:'highMidQ',color:'#c18cff',gainGuide:'Контролирует жёсткость/деталь. Маленькие движения звучат естественнее.'},
 {name:'AIR',freq:'airHz',gain:'air',q:null,color:'#86eaff',gainGuide:'Воздух. Обычно +0.3…+1.5 dB или лёгкое уменьшение, если микс уже яркий.'}
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

function setNorm(id,v){ v=clamp(v,0,1); if(state.params[id]) state.params[id].norm=v; emit('setParam',{id:id,norm:v}); syncChain(); syncModule(); updateBypass(); }
function setRaw(id,v){ if(state.params[id]) state.params[id].raw=v; emit('setParam',{id:id,raw:v}); syncModule(); draw(); }
function gesture(id,phase){ emit('gesture',{id:id,phase:phase}); }

function buildPresetMenu(){
 const sig=(state.presets||[]).join('|');
 if(sig===presetSignature) return;
 presetSignature=sig;
 const menu=$('#presetMenu');
 menu.innerHTML='';
 (state.presets||[]).forEach((name,i)=>{
   const b=document.createElement('button');
   b.className='preset-item'+(name.toUpperCase().includes('EMPTY')||name.toUpperCase().includes('INIT')?' init':'');
   b.textContent=name;
   b.dataset.index=String(i);
   b.onclick=e=>{
     e.stopPropagation();
     emit('preset',i);
     state.presetIndex=i;
     closePresetMenu();
     syncPreset();
   };
   menu.appendChild(b);
 });
 syncPreset();
}

function syncPreset(){
 const name=(state.presets&&state.presets[state.presetIndex])||'Preset';
 $('#presetButtonText').textContent=name;
 $$('.preset-item').forEach((el,i)=>el.classList.toggle('active',i===state.presetIndex));
}

function togglePresetMenu(){
 const open=!$('#presetMenu').classList.contains('open');
 $('#presetMenu').classList.toggle('open',open);
 $('#presetButton').classList.toggle('open',open);
 $('#presetButton').setAttribute('aria-expanded',open?'true':'false');
}
function closePresetMenu(){
 $('#presetMenu').classList.remove('open');
 $('#presetButton').classList.remove('open');
 $('#presetButton').setAttribute('aria-expanded','false');
}

function buildChain(){
 const root=$('#chain');
 root.innerHTML='';
 modules.forEach((m,i)=>{
   const el=document.createElement('div');
   el.className='chain-item';
   el.tabIndex=0;
   el.dataset.module=String(i);

   const power=document.createElement('button');
   power.className='chain-power';
   power.dataset.tip=m.toggle?'Быстро включить или выключить этот модуль.':'Этот выходной блок всегда доступен.';
   power.onclick=e=>{
     e.stopPropagation();
     if(m.toggle) setNorm(m.toggle,p(m.toggle).norm>=.5?0:1);
   };

   const copy=document.createElement('div');
   copy.className='chain-copy';
   copy.innerHTML='<div class="chain-title">'+m.title+'</div><div class="chain-sub">'+m.sub+'</div>';

   const select=()=>{selectedModule=i;syncChain();renderModule();draw();};
   el.onclick=select;
   el.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();select();}};
   el.append(power,copy);
   root.appendChild(el);
 });
 syncChain();
}

function syncChain(){
 $$('.chain-item').forEach((el,i)=>{
   const m=modules[i];
   const on=m.toggle?p(m.toggle).norm>=.5:true;
   el.classList.toggle('active',i===selectedModule);
   el.classList.toggle('enabled',on);
 });
}

function makeKnob(spec){
 const c=document.createElement('div');
 c.className='control';
 c.dataset.param=spec.id;

 const l=document.createElement('div');
 l.className='control-label';
 l.textContent=spec.label;

 const w=document.createElement('div');
 w.className='knob-wrap';
 w.dataset.paramId=spec.id;
 const k=document.createElement('div');
 k.className='knob';
 k.style.setProperty('--p',(clamp(p(spec.id).norm,0,1)*270)+'deg');
 w.appendChild(k);

 const v=document.createElement('div');
 v.className='control-value';
 v.textContent=fmt(spec,p(spec.id).raw);

 c.append(l,w,v);

 let sy=0,sn=0,drag=false;
 w.onpointerdown=e=>{
   e.preventDefault();drag=true;sy=e.clientY;sn=p(spec.id).norm;
   w.classList.add('dragging');
   w.setPointerCapture(e.pointerId);gesture(spec.id,'begin');
 };
 w.onpointermove=e=>{
   if(!drag)return;
   const n=clamp(sn+(sy-e.clientY)/150,0,1);
   k.style.setProperty('--p',(n*270)+'deg');
   if(state.params[spec.id]) state.params[spec.id].norm=n;
   emit('setParam',{id:spec.id,norm:n});
   v.textContent=fmt(spec,p(spec.id).raw);
 };
 const end=e=>{
   if(!drag)return;drag=false;w.classList.remove('dragging');
   try{w.releasePointerCapture(e.pointerId)}catch(_){}
   gesture(spec.id,'end');
 };
 w.onpointerup=end;w.onpointercancel=end;
 w.ondblclick=e=>{e.preventDefault();setNorm(spec.id,p(spec.id).def);};
 return c;
}

function makeToggle(spec){
 const c=document.createElement('div');
 c.className='control toggle-control';
 c.dataset.param=spec.id;
 const l=document.createElement('div');l.className='control-label';l.textContent=spec.label;
 const t=document.createElement('div');t.className='big-toggle'+(p(spec.id).norm>=.5?' on':'');t.dataset.paramId=spec.id;
 const v=document.createElement('div');v.className='control-value';v.textContent=p(spec.id).norm>=.5?'ON':'OFF';
 t.onclick=()=>setNorm(spec.id,p(spec.id).norm>=.5?0:1);
 c.append(l,t,v);
 return c;
}

function renderEqPanel(root){
 root.classList.add('eq-mode');
 const wrap=document.createElement('div');wrap.className='eq-module-panel';
 const list=document.createElement('div');list.className='eq-band-list';

 eqBands.forEach((b,i)=>{
   const item=document.createElement('div');
   item.className='eq-band';
   item.dataset.band=String(i);
   item.dataset.eqBand=String(i);
   item.innerHTML='<div class="eq-band-top"><div class="eq-band-name">'+b.name+'</div><div class="eq-band-dot" style="background:'+b.color+';box-shadow:0 0 8px '+b.color+'88"></div></div>'+
     '<div class="eq-band-values"><div><span>FREQ</span><b class="eq-freq"></b></div><div><span>GAIN</span><b class="eq-gain"></b></div></div>';
   item.onclick=()=>{selectedEqBand=i;syncEqPanel();draw();};
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
   const ft=$('.eq-freq',item),gt=$('.eq-gain',item);
   if(ft)ft.textContent=f>=1000?(f/1000).toFixed(2)+'k':Math.round(f)+' Hz';
   if(gt)gt.textContent=(g>=0?'+':'')+g.toFixed(1)+' dB';
 });
 const b=eqBands[selectedEqBand];
 const info=$('#eqInstructions');
 if(info){
   const q=b.q?'Q: '+p(b.q).raw.toFixed(2):'SHELF BAND';
   info.innerHTML='<h3>'+b.name+' · '+q+'</h3><p>'+b.gainGuide+' Тяните точку на большом графике: влево/вправо — частота, вверх/вниз — gain. '+(b.q?'Колесо мыши меняет Q.':'Shelf-полоса работает шире и мягче.')+'</p>'+
     '<div class="keys"><span class="key">DRAG = FREQ + GAIN</span>'+(b.q?'<span class="key">WHEEL = Q</span>':'')+'<span class="key">DOUBLE CLICK = RESET</span></div>';
 }
}

function renderModule(){
 const m=modules[selectedModule];
 $('#moduleTitle').textContent=m.title;
 $('#moduleKicker').textContent=m.sub.toUpperCase();
 $('#moduleFooter').textContent=m.footer;

 const power=$('#modulePower');
 if(m.toggle){
   power.style.display='';
   power.dataset.paramId=m.toggle;
   power.onclick=()=>setNorm(m.toggle,p(m.toggle).norm>=.5?0:1);
 }else{
   power.style.display='none';
   power.onclick=null;
   delete power.dataset.paramId;
 }

 $('#helpBtn').onclick=()=>openHelp(m);

 const root=$('#moduleControls');
 root.className='controls-grid';
 root.innerHTML='';
 if(m.special==='eq') renderEqPanel(root);
 else m.params.forEach(s=>root.appendChild(s.type==='toggle'?makeToggle(s):makeKnob(s)));
 syncModule();
}

function syncModule(){
 const m=modules[selectedModule];
 if(m.toggle) $('#modulePower').classList.toggle('on',p(m.toggle).norm>=.5);

 $$('.control[data-param]').forEach(c=>{
   const id=c.dataset.param;
   const spec=m.params.find(x=>x.id===id);
   if(!spec)return;
   const knob=$('.knob',c),val=$('.control-value',c);
   if(knob)knob.style.setProperty('--p',(clamp(p(id).norm,0,1)*270)+'deg');
   if(val)val.textContent=spec.type==='toggle'?(p(id).norm>=.5?'ON':'OFF'):fmt(spec,p(id).raw);
   const tog=$('.big-toggle',c);if(tog)tog.classList.toggle('on',p(id).norm>=.5);
 });

 if(m.special==='eq')syncEqPanel();
}

function openHelp(m){
 $('#modalTitle').textContent=m.title;
 $('#modalText').textContent=m.help;
 $('#modal').classList.add('open');
 $('#modal').setAttribute('aria-hidden','false');
}
function closeHelp(){
 $('#modal').classList.remove('open');
 $('#modal').setAttribute('aria-hidden','true');
}

function currentContext(id){
 const m=state.meters||{};
 if(id==='targetInput'&&Number(m.inputPeak)>-6)return 'Сейчас входные пики уже высокие. Не поднимайте TARGET RMS, сначала уберите INPUT TRIM.';
 if(id==='glue'||id==='glueThreshold'||id==='glueRatio'){
   if(p('dynamicEq').raw>.42||p('multiband').raw>.42)return 'Предыдущая динамическая обработка уже сильная. Начинайте компрессор мягче обычного.';
 }
 if(id==='impact'&&p('glue').raw>.48)return 'Glue уже заметный. Для возврата удара начните примерно с 20–35% PUNCH, а не с максимума.';
 if((id==='exciter'||id==='exciterMix')&&p('air').raw>1.0)return 'AIR EQ уже поднят. Exciter держите ниже, чтобы верх не стал стеклянным и шершавым.';
 if((id==='widthMid'||id==='widthHigh'||id==='imagerSafety')&&Number(m.correlation)<.2)return 'CORRELATION сейчас низкая. Не расширяйте сильнее; лучше уменьшите WIDTH или поднимите SAFETY.';
 if(id==='limiterDrive'&&p('clipDrive').raw>2.2)return 'Clipper уже снимает много пиков. Добавляйте Limiter Drive небольшими шагами и следите, чтобы GR не стал постоянным.';
 if((id==='clipDrive'||id==='clipShape')&&p('impact').raw>.55)return 'IMPACT уже усиливает транзиенты. Клиппер настраивайте мягче, чтобы не получить щелчки и хруст.';
 return '';
}

function showParamTip(id,x,y){
 const spec=modules.flatMap(m=>m.params).find(s=>s.id===id);
 const g=guidance[id];
 if(!spec||!g)return;
 $('#tooltipTitle').textContent=spec.label;
 $('#tooltipBody').textContent=g.body;
 $('#tooltipRange').textContent='Рабочая зона: '+g.range;
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
 $('#tooltipBody').textContent=b.gainGuide+' Горизонталь меняет частоту, вертикаль — gain.';
 $('#tooltipRange').textContent=b.q?'Q обычно начинайте около 0.7–1.2. Колесом мыши меняйте ширину.':'Это широкая shelf-полоса — используйте небольшие движения.';
 $('#tooltipContext').textContent=(b.name==='AIR'&&p('exciter').raw>.12)?'Exciter уже заметный. AIR добавляйте очень умеренно.':'';
 positionTip(x,y);
}
function positionTip(x,y){
 const tip=$('#tooltip');
 tip.style.left=Math.min(window.innerWidth-345,x+14)+'px';
 tip.style.top=Math.min(window.innerHeight-155,y+14)+'px';
 tip.classList.add('show');
}
function hideTip(){$('#tooltip').classList.remove('show');}

function updateBypass(){$('#bypassBtn').classList.toggle('on',p('masterBypass').norm>=.5);}
function dbMeter(db){return Number.isFinite(db)?clamp((db+60)/60,0,1):0;}
function dbText(db,suffix){return Number.isFinite(db)&&db>-99?db.toFixed(1)+(suffix||''):'-∞';}
function updateMeters(){
 const m=state.meters||{},ip=Number(m.inputPeak??-100),op=Number(m.outputPeak??-100);
 $('#inMeter').style.height=(dbMeter(ip)*100)+'%';
 $('#outMeter').style.height=(dbMeter(op)*100)+'%';
 $('#inPeak').textContent=dbText(ip,' dBFS');
 $('#outPeak').textContent=dbText(op,' dBFS');
 $('#lufs').textContent=dbText(Number(m.lufs??-100),'');
 $('#crest').textContent=Number(m.crest??0).toFixed(1)+' dB';
 $('#corr').textContent=Number(m.correlation??0).toFixed(2);
 $('#gr').textContent=Number(m.limiterGR??0).toFixed(1)+' dB';
 $('#inputGain').textContent=Number(m.inputGain??0).toFixed(1)+' dB';

 const corr=Number(m.correlation??0),grv=Number(m.limiterGR??0),badge=$('#qualityBadge');
 if(corr<-.05){badge.textContent='PHASE';badge.style.color='var(--red)';}
 else if(grv>7){badge.textContent='HARD';badge.style.color='var(--amber)';}
 else{badge.textContent='CLEAN';badge.style.color='var(--green)';}

 const rms=Number(m.inputRms??-100),coach=$('#coach');
 coach.classList.remove('warn','hot');
 if(rms<-60)$('#coachText').textContent='Ожидаю аудиосигнал…';
 else if(rms<-24){coach.classList.add('warn');$('#coachText').textContent='Вход тихий. Поднимайте уровень умеренно: хороший старт — средний уровень около -18…-16 dBFS.';}
 else if(rms>-10){coach.classList.add('hot');$('#coachText').textContent='Вход слишком горячий. Снизьте INPUT TRIM: так компрессор и клиппер будут работать чище.';}
 else $('#coachText').textContent='Вход в рабочей зоне. Теперь настраивайте тон, динамику и финальную громкость по очереди.';
}

const canvas=$('#eqCanvas'),ctx=canvas.getContext('2d');
function resizeCanvas(){
 const r=canvas.getBoundingClientRect(),d=Math.max(1,window.devicePixelRatio||1);
 const w=Math.max(10,Math.round(r.width*d)),h=Math.max(10,Math.round(r.height*d));
 if(canvas.width!==w||canvas.height!==h){canvas.width=w;canvas.height=h;}
 ctx.setTransform(d,0,0,d,0,0);draw();
}
function graphRect(){const r=canvas.getBoundingClientRect();return{x:18,y:14,w:Math.max(10,r.width-36),h:Math.max(10,r.height-35)};}
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
 v+=(p('air').raw||0)/(1+Math.pow(Math.max(3000,p('airHz').raw||10500)/Math.max(20,f),4));
 return v;
}
function spectrum(a,g,color,width,fill){
 if(!a||!a.length)return;
 ctx.beginPath();
 a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
 if(fill){
   ctx.lineTo(g.x+g.w,g.y+g.h);ctx.lineTo(g.x,g.y+g.h);ctx.closePath();
   const z=ctx.createLinearGradient(0,g.y,0,g.y+g.h);z.addColorStop(0,'rgba(52,216,255,.14)');z.addColorStop(1,'rgba(52,216,255,0)');
   ctx.fillStyle=z;ctx.fill();
   ctx.beginPath();a.forEach((db,i)=>{const x=g.x+i/(a.length-1)*g.w,y=sy(db,g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
 }
 ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();
}
function draw(){
 const r=canvas.getBoundingClientRect();if(!r.width||!r.height)return;
 const g=graphRect();ctx.clearRect(0,0,r.width,r.height);

 const bg=ctx.createLinearGradient(0,g.y,0,g.y+g.h);
 bg.addColorStop(0,'rgba(20,33,45,.50)');bg.addColorStop(.55,'rgba(10,17,24,.22)');bg.addColorStop(1,'rgba(4,8,12,.08)');
 ctx.fillStyle=bg;ctx.fillRect(g.x,g.y,g.w,g.h);

 ctx.font='8px Segoe UI';ctx.textBaseline='bottom';
 [20,50,100,200,500,1000,2000,5000,10000,20000].forEach(f=>{
   const x=fx(f,g);ctx.strokeStyle='rgba(69,95,117,.24)';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(x,g.y);ctx.lineTo(x,g.y+g.h);ctx.stroke();
   ctx.fillStyle='rgba(100,122,142,.62)';ctx.textAlign=f===20?'left':f===20000?'right':'center';ctx.fillText(f>=1000?(f/1000)+'k':String(f),x,g.y+g.h-3);
 });
 [-12,-6,0,6,12].forEach(db=>{const y=gy(db,g);ctx.strokeStyle=db===0?'rgba(108,139,162,.34)':'rgba(69,95,117,.17)';ctx.beginPath();ctx.moveTo(g.x,y);ctx.lineTo(g.x+g.w,y);ctx.stroke();});

 spectrum(state.pre,g,'rgba(119,139,158,.38)',1,false);
 spectrum(state.post,g,'rgba(52,216,255,.90)',1.6,true);

 ctx.beginPath();
 const n=Math.max(240,Math.floor(g.w));
 for(let i=0;i<n;i++){const u=i/(n-1),x=g.x+u*g.w,y=gy(eqAt(nFreq(u)),g);if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);}
 const grad=ctx.createLinearGradient(g.x,0,g.x+g.w,0);grad.addColorStop(0,'#34d8ff');grad.addColorStop(.5,'#9b8cff');grad.addColorStop(1,'#86eaff');
 ctx.strokeStyle=grad;ctx.lineWidth=2.2;ctx.stroke();

 if(modules[selectedModule].id==='eq'){
   eqBands.forEach((b,i)=>{
     const x=fx(p(b.freq).raw,g),y=gy(p(b.gain).raw,g),active=i===selectedEqBand,hover=i===eqHover;
     ctx.beginPath();ctx.arc(x,y,active?8:6,0,Math.PI*2);ctx.fillStyle=active?b.color:'#081018';ctx.fill();
     ctx.strokeStyle=b.color;ctx.lineWidth=active||hover?2.2:1.5;ctx.stroke();
     if(active){ctx.beginPath();ctx.arc(x,y,14,0,Math.PI*2);ctx.strokeStyle=b.color+'55';ctx.lineWidth=1;ctx.stroke();}
   });
 }
 renderEqInspector();
}
function renderEqInspector(){
 const b=eqBands[selectedEqBand],root=$('#eqInspector'),f=p(b.freq).raw,g=p(b.gain).raw,q=b.q?p(b.q).raw:null;
 root.innerHTML='<div class="eq-chip"><span>BAND</span><b>'+b.name+'</b></div>'+
   '<div class="eq-chip"><span>FREQ</span><b>'+(f>=1000?(f/1000).toFixed(2)+' kHz':Math.round(f)+' Hz')+'</b></div>'+
   '<div class="eq-chip"><span>GAIN</span><b>'+(g>=0?'+':'')+g.toFixed(1)+' dB</b></div>'+
   (q!==null?'<div class="eq-chip"><span>Q</span><b>'+q.toFixed(2)+'</b></div>':'');
}
function hitEq(cx,cy){
 if(modules[selectedModule].id!=='eq')return-1;
 const rect=canvas.getBoundingClientRect(),g=graphRect(),x=cx-rect.left,y=cy-rect.top;
 let best=-1,dist=1e9;
 eqBands.forEach((b,i)=>{const d=Math.hypot(x-fx(p(b.freq).raw,g),y-gy(p(b.gain).raw,g));if(d<dist){dist=d;best=i;}});
 return dist<=20?best:-1;
}

canvas.onpointerdown=e=>{
 const i=hitEq(e.clientX,e.clientY);if(i<0)return;e.preventDefault();
 selectedEqBand=i;const b=eqBands[i];eqDrag={band:i,id:e.pointerId};canvas.setPointerCapture(e.pointerId);
 gesture(b.freq,'begin');gesture(b.gain,'begin');syncEqPanel();draw();
};
canvas.onpointermove=e=>{
 const i=hitEq(e.clientX,e.clientY);eqHover=i;
 if(eqDrag){
   const rect=canvas.getBoundingClientRect(),g=graphRect(),x=clamp(e.clientX-rect.left,g.x,g.x+g.w),y=clamp(e.clientY-rect.top,g.y,g.y+g.h),b=eqBands[eqDrag.band];
   setRaw(b.freq,nFreq((x-g.x)/g.w));setRaw(b.gain,clamp((.5-(y-g.y)/g.h)*24,-12,12));return;
 }
 if(i>=0)showEqTip(i,e.clientX,e.clientY);else hideTip();
 draw();
};
function endEq(){
 if(!eqDrag)return;const b=eqBands[eqDrag.band];gesture(b.freq,'end');gesture(b.gain,'end');eqDrag=null;
}
canvas.onpointerup=endEq;canvas.onpointercancel=endEq;
canvas.addEventListener('wheel',e=>{
 const i=hitEq(e.clientX,e.clientY);if(i<0)return;const b=eqBands[i];if(!b.q)return;
 e.preventDefault();selectedEqBand=i;setRaw(b.q,clamp((p(b.q).raw||.9)*(e.deltaY<0?1.10:.91),.25,4));syncEqPanel();draw();
},{passive:false});
canvas.ondblclick=e=>{
 const i=hitEq(e.clientX,e.clientY);if(i<0)return;const b=eqBands[i];
 setRaw(b.freq,p(b.freq).defRaw);setRaw(b.gain,p(b.gain).defRaw);if(b.q)setRaw(b.q,p(b.q).defRaw);
 selectedEqBand=i;syncEqPanel();draw();
};

document.addEventListener('pointermove',e=>{
 if(e.target===canvas)return;
 const paramEl=e.target.closest&&e.target.closest('[data-param-id]');
 if(paramEl&&paramEl.dataset.paramId){showParamTip(paramEl.dataset.paramId,e.clientX,e.clientY);return;}
 const eqEl=e.target.closest&&e.target.closest('[data-eq-band]');
 if(eqEl){showEqTip(Number(eqEl.dataset.eqBand),e.clientX,e.clientY);return;}
 const tipEl=e.target.closest&&e.target.closest('[data-tip]');
 if(tipEl&&tipEl.dataset.tip){showGenericTip(tipEl.dataset.tip,e.clientX,e.clientY);return;}
 hideTip();
});
document.addEventListener('pointerleave',hideTip);
document.addEventListener('click',e=>{if(!e.target.closest('#presetControl'))closePresetMenu();});

$('#presetButton').onclick=e=>{e.stopPropagation();togglePresetMenu();};
$('#bypassBtn').onclick=()=>setNorm('masterBypass',p('masterBypass').norm>=.5?0:1);
$('#modalClose').onclick=closeHelp;
$('.modal-backdrop').onclick=closeHelp;
document.addEventListener('keydown',e=>{if(e.key==='Escape'){closeHelp();closePresetMenu();}});

function applyState(s){
 if(!s)return;
 state=s;
 buildPresetMenu();
 syncPreset();
 updateBypass();
 updateMeters();
 if(!uiBuilt){
   buildChain();
   renderModule();
   uiBuilt=true;
 }else{
   syncChain();
   syncModule();
 }
 draw();
}

if(backend){
 backend.addEventListener('state',applyState);
 emit('uiReady',{ready:true});
}else{
 const ids=new Set(['masterBypass']);
 modules.forEach(m=>{if(m.toggle)ids.add(m.toggle);m.params.forEach(x=>ids.add(x.id));});
 eqBands.forEach(b=>{ids.add(b.freq);ids.add(b.gain);if(b.q)ids.add(b.q);});
 ids.forEach(id=>state.params[id]={norm:.5,raw:0,def:.5,defRaw:0});
 Object.assign(state.params,{
   lowShelfHz:{norm:.38,raw:105,def:.38,defRaw:105},lowShelf:{norm:.5,raw:0,def:.5,defRaw:0},
   lowMidHz:{norm:.4,raw:320,def:.4,defRaw:320},lowMid:{norm:.5,raw:-.6,def:.5,defRaw:-.6},lowMidQ:{norm:.2,raw:.85,def:.2,defRaw:.85},
   midHz:{norm:.48,raw:900,def:.48,defRaw:900},midGain:{norm:.5,raw:0,def:.5,defRaw:0},midQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
   presenceHz:{norm:.62,raw:3200,def:.62,defRaw:3200},presence:{norm:.55,raw:.7,def:.55,defRaw:.7},presenceQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
   highMidHz:{norm:.7,raw:6200,def:.7,defRaw:6200},highMidGain:{norm:.5,raw:0,def:.5,defRaw:0},highMidQ:{norm:.2,raw:.9,def:.2,defRaw:.9},
   airHz:{norm:.78,raw:10500,def:.78,defRaw:10500},air:{norm:.55,raw:.8,def:.55,defRaw:.8},
   smartGain:{norm:1,raw:1,def:1,defRaw:1},cleanEqOn:{norm:1,raw:1,def:1,defRaw:1}
 });
 state.presets=['Boom Bap - DENSE PUNCH','Boom Bap - DUSTY ANALOG','Hip-Hop - MODERN DENSE','INIT - EMPTY / ALL OFF'];
 state.meters={inputPeak:-12.4,outputPeak:-.8,inputRms:-18.2,lufs:-9.4,crest:8.7,correlation:.72,limiterGR:2.8,inputGain:1.4};
 state.pre=Array.from({length:64},(_,i)=>-42+9*Math.sin(i*.13)-i*.16);
 state.post=Array.from({length:64},(_,i)=>-35+7*Math.sin(i*.12)-i*.13);
 applyState(state);
}

if(window.ResizeObserver)new ResizeObserver(resizeCanvas).observe(canvas);
window.addEventListener('resize',resizeCanvas);
requestAnimationFrame(resizeCanvas);
})();
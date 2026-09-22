import fs from 'node:fs';
import path from 'node:path';
import { JSDOM } from 'jsdom';

const root = process.cwd();
const htmlPath = path.join(root, 'masterforge_one', 'WebUI', 'index.html');
const jsPath = path.join(root, 'masterforge_one', 'WebUI', 'app.js');

const html = fs.readFileSync(htmlPath, 'utf8').replace('<script src="/app.js"></script>', '');
const script = fs.readFileSync(jsPath, 'utf8');

const dom = new JSDOM(html, {
  runScripts: 'outside-only',
  pretendToBeVisual: true,
  url: 'https://masterforge.local/'
});

const { window } = dom;
const { document } = window;

window.ResizeObserver = class {
  constructor(cb){ this.cb = cb; }
  observe(){ }
  disconnect(){ }
};

window.requestAnimationFrame = fn => { fn(0); return 1; };
window.cancelAnimationFrame = () => {};
window.HTMLElement.prototype.scrollIntoView = function(){};
window.HTMLElement.prototype.setPointerCapture = function(){};
window.HTMLElement.prototype.releasePointerCapture = function(){};

const noop = () => {};
window.HTMLCanvasElement.prototype.getContext = function(){
  return {
    setTransform: noop,
    clearRect: noop,
    createLinearGradient: () => ({ addColorStop: noop }),
    fillRect: noop,
    beginPath: noop,
    moveTo: noop,
    lineTo: noop,
    stroke: noop,
    fill: noop,
    closePath: noop,
    arc: noop,
    fillText: noop,
    measureText: () => ({ width: 10 }),
    set font(v){},
    set textBaseline(v){},
    set textAlign(v){},
    set fillStyle(v){},
    set strokeStyle(v){},
    set lineWidth(v){}
  };
};

window.eval(script);

const assert = (condition, message) => {
  if (!condition) throw new Error(message);
};

const cards = [...document.querySelectorAll('.chain-item')];
assert(cards.length >= 10, 'module chain did not render');

const eqCard = document.querySelector('.chain-item[data-id="eq"]');
assert(eqCard, 'EQ card missing');
eqCard.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
assert(document.querySelector('#moduleTitle').textContent === 'PARAMETRIC EQ', 'top module card click does not select EQ');

const maxCard = document.querySelector('.chain-item[data-id="maximizer"]');
assert(maxCard, 'Maximizer card missing');
maxCard.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
assert(document.querySelector('#moduleTitle').textContent === 'MAXIMIZER', 'top module card click does not select Maximizer');

assert(document.querySelectorAll('.rotary').length > 0, 'rotary controls missing');
assert(document.querySelectorAll('.v-fader').length > 0, 'vertical faders missing');
assert(document.querySelectorAll('.h-fader').length === 0, 'horizontal faders must not exist');

const chainScroll = document.querySelector('#chainScroll');
chainScroll.scrollLeft = 0;
chainScroll.dispatchEvent(new window.WheelEvent('wheel', { bubbles: true, cancelable: true, deltaY: 120 }));
assert(chainScroll.scrollLeft > 0, 'mouse wheel does not scroll top module chain horizontally');

const compCard = document.querySelector('.chain-item[data-id="comp"]');
assert(compCard, 'compressor card missing');
const remove = compCard.querySelector('.chain-remove');
remove.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
assert(!document.querySelector('.chain-item[data-id="comp"]'), 'remove module did not update chain');

document.querySelector('#addModuleBtn').dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
const picks = [...document.querySelectorAll('.module-pick')].map(x => x.textContent);
assert(picks.some(x => x.includes('VINTAGE COMP')), 'removed module is not available in add-module menu');

const inputCard = document.querySelector('.input-card');
inputCard.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
assert(document.querySelector('#moduleTitle').textContent === 'INPUT / LEVEL', 'fixed INPUT card does not work');

const outputCard = document.querySelector('.output-card');
outputCard.dispatchEvent(new window.MouseEvent('click', { bubbles: true }));
assert(document.querySelector('#moduleTitle').textContent === 'OUTPUT', 'fixed OUTPUT card does not work');

console.log('WEBUI_SMOKE_OK');

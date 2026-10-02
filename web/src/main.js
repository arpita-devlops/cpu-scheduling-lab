import './styles.css';
import { loadEngine } from './engine.js';
import { initHero } from './hero.js';
import { initSimulator } from './simulator.js';
import { initCompare } from './compare.js';
import { initBenchmark } from './benchmark.js';

const status = document.getElementById('engine-status');

loadEngine().then(start).catch(fail);

function start(engine) {
  status.classList.add('ready');
  status.lastChild.textContent = 'C++ engine · WebAssembly';
  document.getElementById('stat-algorithms').textContent = engine.catalog.length;

  const hero = initHero(engine);
  const simulator = initSimulator(engine);
  const compare = initCompare(engine, simulator);
  initBenchmark(engine);

  let resizeFrame = 0;
  window.addEventListener('resize', () => {
    cancelAnimationFrame(resizeFrame);
    resizeFrame = requestAnimationFrame(() => {
      hero.relayout();
      simulator.relayout();
      compare.relayout();
    });
  });
}

function fail(err) {
  console.error(err);
  status.classList.add('failed');
  status.lastChild.textContent = 'Engine failed to load';
  document.getElementById('hero-why').textContent = 'Your browser could not start the WebAssembly engine. Try a recent Chrome, Edge, Firefox or Safari.';
}

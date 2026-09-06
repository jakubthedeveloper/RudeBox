'use strict';

initializeVelocityExplorer();
initializeReadingNavigation();

function initializeVelocityExplorer() {
  const lab = document.querySelector('#velocity-lab');
  const sensitivity = lab.querySelector('#sensitivity');
  const peak = lab.querySelector('#peak');
  const update = () => renderVelocityExplorer(lab, Number(peak.value), Number(sensitivity.value) / 100);
  sensitivity.addEventListener('input', update);
  peak.addEventListener('input', update);
  update();
  lab.hidden = false;
}

// Matches HitDetector::mapVelocity at firmware snapshot 881fcd1.
function mapVelocity(peak, sensitivity) {
  const effectiveMaximum = 11500 + (8200 - 11500) * sensitivity;
  const exponent = 2.2 + (0.55 - 2.2) * sensitivity;
  const normalized = Math.max(0, Math.min(1, (peak - 120) / (effectiveMaximum - 120)));
  return Math.pow(normalized, exponent);
}

function graphPoint(peak, velocity) {
  return { x: 55 + peak / 12000 * 560, y: 225 - velocity * 200 };
}

function renderVelocityExplorer(lab, peak, sensitivity) {
  const curve = [];
  for (let value = 0; value <= 12000; value += 50) {
    const point = graphPoint(value, mapVelocity(value, sensitivity));
    curve.push(`${value === 0 ? 'M' : 'L'}${point.x.toFixed(2)} ${point.y.toFixed(2)}`);
  }
  const velocity = mapVelocity(peak, sensitivity);
  const point = graphPoint(peak, velocity);
  lab.querySelector('#velocity-curve').setAttribute('d', curve.join(' '));
  lab.querySelector('#velocity-dot').setAttribute('cx', point.x);
  lab.querySelector('#velocity-dot').setAttribute('cy', point.y);
  lab.querySelector('#sensitivity-value').textContent = `${Math.round(sensitivity * 100)}%`;
  lab.querySelector('#peak-value').textContent = peak;
  lab.querySelector('#velocity-result').value = velocity.toFixed(3);
}

function initializeReadingNavigation() {
  if (!('IntersectionObserver' in window)) return;
  const links = [...document.querySelectorAll('.toc a')];
  const observer = new IntersectionObserver(entries => {
    const active = entries.find(entry => entry.isIntersecting);
    if (!active) return;
    for (const link of links) {
      if (link.hash === `#${active.target.id}`) link.setAttribute('aria-current', 'location');
      else link.removeAttribute('aria-current');
    }
  }, { rootMargin: '-10% 0px -65% 0px' });
  document.querySelectorAll('article section').forEach(section => observer.observe(section));
}

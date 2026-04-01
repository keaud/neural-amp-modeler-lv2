<template>
  <canvas ref="canvas"></canvas>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, watch } from "vue";

const props = defineProps({
  width: { type: Number, default: 300 },
  height: { type: Number, default: 160 },
  rate: { type: Number, default: 0 }
});

const canvas = ref(null);
let ctx;
let raf;
let last = performance.now();
let connectTimer = 0;

const DPR = window.devicePixelRatio || 1;

const config = {
  particleCount: 160,
  speed: 0.1,
  swirl: 0.3,
  fadeIn: 0.08,
  fadeOut: 0.015,
  radius: 0.45,
  color: [80, 190, 255],
  maxConnections: 2,
  connectionDistance: 40,
  connectionAlpha: 0.6
};

function adjustedColor(rate) {
  const t = Math.min(rate, 1) * 0.75; // Rate shifts color 75% toward white at rate 1
  return [
    Math.round(config.color[0] + (255 - config.color[0]) * t),
    Math.round(config.color[1] + (255 - config.color[1]) * t),
    Math.round(config.color[2] + (255 - config.color[2]) * t)
  ];
}

class Particle {
  constructor() {
    this.connections = [];
    this.reset();
  }

  adjustedSpeed() {
    return this.speed + (2 * props.rate);
  }

  adjustedSwirl() {
    return this.swirlSpeed + (2 * props.rate);
  }

  reset() {
    this.z = Math.random();
    this.angle = Math.random() * Math.PI * 2;
    this.speed = (Math.random() * 0.6 + 0.4) * config.speed;
    this.swirlSpeed = (Math.random() * 0.6 + 0.4) * config.swirl;
    this.alpha = 0;
    this.size = Math.random() * 1.8 + 0.6;
    this.yOffset = Math.random() * 2 - 1;
    this.yDrift =
      (Math.random() * 0.4 + 0.1) * (Math.random() < 0.5 ? -1 : 1);
    this.connections.length = 0;
  }

  update(dt) {
    this.z -= this.adjustedSpeed() * dt;
    this.angle += this.adjustedSwirl() * dt;
    this.yOffset += this.yDrift * dt * 0.15;

    if (this.z < 0) {
      this.reset();
      this.z = 1;
      this.alpha = 0;
    }

    if (this.yOffset > 1) this.yOffset = -1;
    if (this.yOffset < -1) this.yOffset = 1;

    this.alpha += this.z > 0.25 ? config.fadeIn : -config.fadeOut;
    this.alpha = Math.max(0, Math.min(1, this.alpha));
  }

  position(w, h) {
    const depth = this.z;
    const r = config.radius * h * depth;

    return {
      x: w / 2 + Math.cos(this.angle) * r,
      y:
        h / 2 +
        Math.sin(this.angle) * r * 0.35 +
        this.yOffset * h * 0.35 * depth,
      depth
    };
  }

  draw(w, h) {
    const { x, y, depth } = this.position(w, h);
    const a = this.alpha * (1 - Math.abs(this.yOffset));
    if (a <= 0) return;

    const color = adjustedColor(props.rate);
    ctx.beginPath();
    ctx.fillStyle = `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${a})`;
    ctx.shadowColor = ctx.fillStyle;
    ctx.shadowBlur = 10 * a;
    ctx.arc(x, y, this.size * depth * 1.6, 0, Math.PI * 2);
    ctx.fill();
  }
}

let particles = [];

function resize() {
  if (!canvas.value) return;
  canvas.value.width = props.width * DPR;
  canvas.value.height = props.height * DPR;
  canvas.value.style.width = props.width + "px";
  canvas.value.style.height = props.height + "px";
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
}

function buildConnections() {
  particles.forEach(p => (p.connections.length = 0));

  for (let i = 0; i < particles.length; i++) {
    const p = particles[i];
    const pPos = p.position(props.width, props.height);

    for (let j = i + 1; j < particles.length; j++) {
      const o = particles[j];
      const oPos = o.position(props.width, props.height);

      const dx = pPos.x - oPos.x;
      const dy = pPos.y - oPos.y;
      const dist = Math.sqrt(dx * dx + dy * dy);

      if (dist > config.connectionDistance) continue;

      if (p.connections.length < config.maxConnections) p.connections.push(o);
      if (o.connections.length < config.maxConnections) o.connections.push(p);
    }
  }
}

function frame(now) {
  const dt = Math.min(0.05, (now - last) / 1000);
  last = now;

  connectTimer += dt;
  if (connectTimer > 0.2) {
    buildConnections();
    connectTimer = 0;
  }

  ctx.clearRect(0, 0, props.width, props.height);

  ctx.save();
  ctx.beginPath();
  ctx.ellipse(
    props.width / 2,
    props.height / 2,
    props.width * 0.48,
    props.height * 0.48,
    0,
    0,
    Math.PI * 2
  );
  ctx.clip();

  ctx.shadowBlur = 0;

  const color = adjustedColor(props.rate);
  particles.forEach(p => {
    const pPos = p.position(props.width, props.height);

    p.connections.forEach(o => {
      const oPos = o.position(props.width, props.height);
      const dx = pPos.x - oPos.x;
      const dy = pPos.y - oPos.y;
      const dist = Math.sqrt(dx * dx + dy * dy);

      const a =
        (1 - dist / config.connectionDistance) *
        config.connectionAlpha *
        Math.min(p.alpha, o.alpha) *
        Math.min(pPos.depth, oPos.depth);

      if (a <= 0.02) return;

      ctx.beginPath();
      ctx.strokeStyle = `rgba(${color[0]}, ${color[1]}, ${color[2]}, ${a})`;
      ctx.lineWidth = 0.6 + pPos.depth * 0.4;
      ctx.moveTo(pPos.x, pPos.y);
      ctx.lineTo(oPos.x, oPos.y);
      ctx.stroke();
    });
  });

  particles.forEach(p => {
    p.update(dt);
    p.draw(props.width, props.height);
  });

  ctx.restore();
  raf = requestAnimationFrame(frame);
}

onMounted(() => {
  ctx = canvas.value.getContext("2d");
  resize();
  particles = Array.from({ length: config.particleCount }, () => new Particle());
  raf = requestAnimationFrame(frame);
});

onBeforeUnmount(() => {
  cancelAnimationFrame(raf);
});

watch(() => [props.width, props.height], resize);
</script>

<style scoped>
canvas {
  display: block;
}
</style>

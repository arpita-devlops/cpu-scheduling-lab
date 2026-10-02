import { defineConfig } from 'vite';

export default defineConfig({
  // Relative base so the static build works under any GitHub Pages repository path.
  base: './',
  server: {
    watch: process.env.DOCKER ? { usePolling: true, interval: 300 } : undefined,
  },
});

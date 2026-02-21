import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

export default defineConfig({
  plugins: [vue()],
  server: {
    port: 8868
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rollupOptions: {
      output: {
        manualChunks: {
          vue: ['vue'],
          vuetify: ['vuetify'],
          charts: ['apexcharts', 'vue3-apexcharts'],
          three: ['three', 'three-svg-loader'],
          vendor: ['axios', 'luxon', 'asmcrypto-lite']
        }
      }
    }
  }
});

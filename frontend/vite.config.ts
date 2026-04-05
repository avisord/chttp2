import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import path from 'path'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 5173,
    proxy: {
      '/login': { target: 'http://localhost:8080' },
      '/logout': { target: 'http://localhost:8080' },
      '/whoami': { target: 'http://localhost:8080' },
      '/sessions': { target: 'http://localhost:8080' },
      '/fs': { target: 'http://localhost:8080' },
    },
  },
})

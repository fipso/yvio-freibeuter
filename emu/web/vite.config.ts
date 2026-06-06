import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// The frontend is served separately and talks to the C backend over WebSocket.
// Configure the backend URL with VITE_WS_URL (see .env), default ws://<host>:8080/.
export default defineConfig({
  plugins: [vue()],
  server: { host: true, port: 5173 },
})

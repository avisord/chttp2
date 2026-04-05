import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { Provider } from 'react-redux'
import { Toaster } from '@/components/ui/sonner'
import { TooltipProvider } from '@/components/ui/tooltip'
import { store } from '@/store'
import App from './App'
import './index.css'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <Provider store={store}>
      <TooltipProvider>
        <App />
        <Toaster position="bottom-left" richColors />
      </TooltipProvider>
    </Provider>
  </StrictMode>,
)

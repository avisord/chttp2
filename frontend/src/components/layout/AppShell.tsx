import { Outlet } from 'react-router-dom'
import TopBar from './TopBar'
import Sidebar from './Sidebar'
import UploadManager from '@/components/uploads/UploadManager'

export default function AppShell() {
  return (
    <div className="flex h-screen flex-col bg-slate-50">
      <TopBar />
      <div className="flex flex-1 overflow-hidden">
        <Sidebar />
        <main className="flex-1 overflow-auto bg-slate-50">
          <Outlet />
        </main>
      </div>
      <UploadManager />
    </div>
  )
}

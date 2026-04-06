import { Outlet } from 'react-router-dom'
import { ShieldAlert } from 'lucide-react'
import TopBar from './TopBar'
import Sidebar from './Sidebar'
import UploadManager from '@/components/uploads/UploadManager'
import { useAppSelector } from '@/store/hooks'

export default function AppShell() {
  const user = useAppSelector((s) => s.auth.user)
  const isRoot = user?.uid === 0

  return (
    <div className="flex h-screen flex-col bg-slate-50">
      <TopBar />
      {isRoot && (
        <div className="flex items-center gap-2 border-b border-red-200 bg-red-50 px-5 py-2 text-xs text-red-700">
          <ShieldAlert className="h-3.5 w-3.5 shrink-0" />
          <span>
            <strong>You are logged in as root.</strong> You have full superuser privileges — any action (file deletion, user management, system changes) is irreversible and unrestricted. Switch to a regular user account for everyday tasks.
          </span>
        </div>
      )}
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

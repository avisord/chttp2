import { Trash2, ArrowRightLeft } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Avatar, AvatarFallback } from '@/components/ui/avatar'
import type { SessionInfo } from '@/types/api'

function timeAgo(ts: number): string {
  const diff = Date.now() / 1000 - ts
  if (diff < 60) return 'just now'
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`
  return `${Math.floor(diff / 86400)}d ago`
}

interface SessionCardProps {
  session: SessionInfo
  isActive: boolean
  onSwitch: () => void
  onDelete: () => void
}

export default function SessionCard({ session, isActive, onSwitch, onDelete }: SessionCardProps) {
  return (
    <div className={`flex items-center gap-3 rounded-xl border p-3.5 transition-all duration-200 ${
      isActive
        ? 'border-indigo-200 bg-indigo-50/50 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)] dark:border-indigo-500/30 dark:bg-indigo-500/10'
        : 'border-slate-100 bg-white shadow-[0_4px_20px_-2px_rgba(79,70,229,0.04)] hover:shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)] dark:border-slate-700 dark:bg-white/5'
    }`}>
      <Avatar className="h-9 w-9">
        <AvatarFallback className="bg-indigo-50 text-sm font-semibold text-indigo-600">
          {session.username.charAt(0).toUpperCase()}
        </AvatarFallback>
      </Avatar>
      <div className="flex-1">
        <div className="flex items-center gap-2">
          <span className="text-sm font-semibold text-slate-900 dark:text-slate-100">{session.username}</span>
          {isActive && (
            <span className="rounded-full bg-emerald-50 px-2 py-0.5 text-xs font-medium text-emerald-600">
              Active
            </span>
          )}
        </div>
        <span className="text-xs text-slate-400">Last active {timeAgo(session.last_access_time)}</span>
      </div>
      <div className="flex gap-1">
        {!isActive && (
          <Button variant="ghost" size="icon" className="h-8 w-8 text-slate-400 hover:bg-indigo-50 hover:text-indigo-600 dark:hover:bg-indigo-500/10 dark:hover:text-indigo-400" onClick={onSwitch} title="Switch to this session">
            <ArrowRightLeft className="h-4 w-4" />
          </Button>
        )}
        <Button variant="ghost" size="icon" className="h-8 w-8 text-slate-400 hover:bg-red-50 hover:text-red-500 dark:hover:bg-red-500/10 dark:hover:text-red-400" onClick={onDelete} title="Delete session">
          <Trash2 className="h-4 w-4" />
        </Button>
      </div>
    </div>
  )
}

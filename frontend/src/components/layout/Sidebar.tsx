import { useEffect } from 'react'
import { useTheme } from 'next-themes'
import { FolderOpen, Settings, HardDrive, Users, UserCircle, Moon, Sun } from 'lucide-react'
import { NavLink, useNavigate } from 'react-router-dom'
import { toast } from 'sonner'
import { cn } from '@/lib/utils'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { fetchSessionsThunk, switchSessionThunk } from '@/store/slices/sessionsSlice'
import { fetchBookmarksThunk } from '@/store/slices/bookmarksSlice'
import { listDirThunk, setCurrentPath } from '@/store/slices/fileSystemSlice'
import { fetchSettingsThunk } from '@/store/slices/settingsSlice'
import PlacesPanel from '@/components/files/PlacesPanel'

const adminItems = [
  { to: '/users', icon: Users, label: 'Users' },
  { to: '/disks', icon: HardDrive, label: 'Disks' },
]

function NavItem({ to, icon: Icon, label }: { to: string; icon: React.ComponentType<{ className?: string }>; label: string }) {
  return (
    <NavLink
      to={to}
      end={to === '/'}
      className={({ isActive }) =>
        cn(
          'flex items-center gap-3 rounded-lg px-3 py-2.5 text-sm font-medium transition-all duration-200',
          isActive
            ? 'bg-indigo-50 text-indigo-600 shadow-[0_1px_3px_rgba(79,70,229,0.1)] dark:bg-indigo-950 dark:text-indigo-400'
            : 'text-slate-500 hover:bg-slate-50 hover:text-slate-900 dark:text-slate-400 dark:hover:bg-slate-800 dark:hover:text-slate-200',
        )
      }
    >
      <Icon className="h-[18px] w-[18px]" />
      {label}
    </NavLink>
  )
}

export default function Sidebar() {
  const dispatch = useAppDispatch()
  const navigate = useNavigate()
  const user = useAppSelector((s) => s.auth.user)
  const { sessions } = useAppSelector((s) => s.sessions)
  const isRoot = user?.uid === 0 && user?.gid === 0
  const { theme, setTheme } = useTheme()

  useEffect(() => {
    dispatch(fetchSessionsThunk())
  }, [dispatch])

  const handleSwitch = async (sessionId: string) => {
    try {
      await dispatch(switchSessionThunk(sessionId)).unwrap()
      dispatch(fetchBookmarksThunk())
      dispatch(fetchSettingsThunk())
      dispatch(setCurrentPath('.'))
      dispatch(listDirThunk('.'))
      navigate('/')
      toast.success('Switched session')
    } catch {
      toast.error('Failed to switch session')
    }
  }

  return (
    <aside className="flex w-56 flex-col border-r border-slate-200 bg-white dark:border-slate-700 dark:bg-slate-900">
      <nav className="flex flex-1 flex-col gap-1 overflow-auto p-3 pt-4">
        <NavItem to="/" icon={FolderOpen} label="Files" />

        <div className="mx-0 my-1.5 border-t border-slate-100" />
        <span className="px-3 text-[11px] font-semibold uppercase tracking-wider text-slate-400">
          Places
        </span>
        <PlacesPanel />

        <div className="mx-0 my-1.5 border-t border-slate-100" />
        <span className="px-3 text-[11px] font-semibold uppercase tracking-wider text-slate-400">
          Sessions
        </span>
        <div className="flex flex-col gap-0.5">
          {sessions.map((session) => {
            const isActive = session.username === user?.username
            return (
              <button
                key={session.session_id}
                onClick={() => !isActive && handleSwitch(session.session_id)}
                className={cn(
                  'group flex w-full items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-all duration-150',
                  isActive
                    ? 'bg-indigo-50 text-indigo-600'
                    : 'text-slate-600 hover:bg-slate-50 hover:text-slate-900 cursor-pointer',
                )}
              >
                <UserCircle className={cn('h-4 w-4 shrink-0', session.username === 'root' && 'text-red-500')} />
                <span className={cn('truncate', session.username === 'root' && 'font-semibold text-red-600')}>{session.username}</span>
                {isActive && (
                  <span className="ml-auto text-[10px] font-semibold uppercase text-indigo-400">
                    active
                  </span>
                )}
              </button>
            )
          })}
        </div>

        <hr className="mx-0 my-1.5 border-slate-200" />
        <NavItem to="/settings" icon={Settings} label="Settings" />

        {isRoot && (
          <>
            <div className="my-2 border-t border-slate-100" />
            <span className="mb-1 px-3 text-xs font-semibold uppercase tracking-wider text-slate-400">Admin</span>
            {adminItems.map((item) => (
              <NavItem key={item.to} {...item} />
            ))}
          </>
        )}
      </nav>

      {/* Footer — theme toggle & storage */}
      <div className="border-t border-slate-100 p-4 dark:border-slate-700">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2 text-xs text-slate-400">
            <HardDrive className="h-3.5 w-3.5" />
            <span>NAS Storage</span>
          </div>
          <button
            onClick={() => setTheme(theme === 'dark' ? 'light' : 'dark')}
            className="rounded-lg p-1.5 text-slate-400 transition-colors hover:bg-slate-100 hover:text-slate-600 dark:hover:bg-slate-800 dark:hover:text-slate-300"
            aria-label="Toggle theme"
          >
            {theme === 'dark' ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
          </button>
        </div>
      </div>
    </aside>
  )
}

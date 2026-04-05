import { FolderOpen, Settings, HardDrive } from 'lucide-react'
import { NavLink } from 'react-router-dom'
import { cn } from '@/lib/utils'

const navItems = [
  { to: '/', icon: FolderOpen, label: 'Files' },
  { to: '/settings', icon: Settings, label: 'Settings' },
]

export default function Sidebar() {
  return (
    <aside className="flex w-56 flex-col border-r border-slate-200 bg-white">
      <nav className="flex flex-1 flex-col gap-1 p-3 pt-4">
        {navItems.map(({ to, icon: Icon, label }) => (
          <NavLink
            key={to}
            to={to}
            end={to === '/'}
            className={({ isActive }) =>
              cn(
                'flex items-center gap-3 rounded-lg px-3 py-2.5 text-sm font-medium transition-all duration-200',
                isActive
                  ? 'bg-indigo-50 text-indigo-600 shadow-[0_1px_3px_rgba(79,70,229,0.1)]'
                  : 'text-slate-500 hover:bg-slate-50 hover:text-slate-900',
              )
            }
          >
            <Icon className="h-[18px] w-[18px]" />
            {label}
          </NavLink>
        ))}
      </nav>

      {/* Storage indicator */}
      <div className="border-t border-slate-100 p-4">
        <div className="flex items-center gap-2 text-xs text-slate-400">
          <HardDrive className="h-3.5 w-3.5" />
          <span>NAS Storage</span>
        </div>
      </div>
    </aside>
  )
}

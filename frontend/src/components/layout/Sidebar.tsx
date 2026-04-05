import { FolderOpen, Settings, HardDrive, Users } from 'lucide-react'
import { NavLink } from 'react-router-dom'
import { cn } from '@/lib/utils'
import { useAppSelector } from '@/store/hooks'

const navItems = [
  { to: '/', icon: FolderOpen, label: 'Files' },
  { to: '/settings', icon: Settings, label: 'Settings' },
]

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
            ? 'bg-indigo-50 text-indigo-600 shadow-[0_1px_3px_rgba(79,70,229,0.1)]'
            : 'text-slate-500 hover:bg-slate-50 hover:text-slate-900',
        )
      }
    >
      <Icon className="h-[18px] w-[18px]" />
      {label}
    </NavLink>
  )
}

export default function Sidebar() {
  const user = useAppSelector((s) => s.auth.user)
  const isRoot = user?.uid === 0 && user?.gid === 0

  return (
    <aside className="flex w-56 flex-col border-r border-slate-200 bg-white">
      <nav className="flex flex-1 flex-col gap-1 p-3 pt-4">
        {navItems.map((item) => (
          <NavItem key={item.to} {...item} />
        ))}

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

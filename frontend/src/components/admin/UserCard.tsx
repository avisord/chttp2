import { Pencil, Trash2, Shield } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import { Avatar, AvatarFallback } from '@/components/ui/avatar'
import type { SystemUser } from '@/types/api'

interface UserCardProps {
  user: SystemUser
  onEdit: () => void
  onDelete: () => void
}

export default function UserCard({ user, onEdit, onDelete }: UserCardProps) {
  const isRoot = user.uid === 0

  return (
    <div className="flex items-center gap-3 rounded-xl border border-slate-100 bg-white p-3.5 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.04)] transition-all duration-200 hover:shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)]">
      <Avatar className="h-9 w-9">
        <AvatarFallback className={isRoot ? 'bg-amber-50 text-sm font-semibold text-amber-600' : 'bg-indigo-50 text-sm font-semibold text-indigo-600'}>
          {user.username.charAt(0).toUpperCase()}
        </AvatarFallback>
      </Avatar>
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2">
          <span className="text-sm font-semibold text-slate-900 truncate">{user.username}</span>
          {isRoot && (
            <Badge variant="outline" className="gap-1 border-amber-200 bg-amber-50 text-amber-700">
              <Shield className="h-3 w-3" />
              root
            </Badge>
          )}
        </div>
        <div className="flex items-center gap-3 text-xs text-slate-400">
          <span>UID {user.uid}</span>
          <span>{user.shell}</span>
          <span className="truncate">{user.home}</span>
        </div>
      </div>
      <div className="flex gap-1">
        <Button variant="ghost" size="icon" className="h-8 w-8 text-slate-400 hover:bg-indigo-50 hover:text-indigo-600" onClick={onEdit} title="Edit user">
          <Pencil className="h-4 w-4" />
        </Button>
        {!isRoot && (
          <Button variant="ghost" size="icon" className="h-8 w-8 text-slate-400 hover:bg-red-50 hover:text-red-500" onClick={onDelete} title="Delete user">
            <Trash2 className="h-4 w-4" />
          </Button>
        )}
      </div>
    </div>
  )
}

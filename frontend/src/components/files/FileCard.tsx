import FileIcon from './FileIcon'
import { cn } from '@/lib/utils'
import type { FileEntry } from '@/types/api'

function formatSize(bytes: number): string {
  if (bytes === 0) return '-'
  const units = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(1024))
  return `${(bytes / Math.pow(1024, i)).toFixed(i > 0 ? 1 : 0)} ${units[i]}`
}

interface FileCardProps {
  entry: FileEntry
  selected: boolean
  onClick: (e: React.MouseEvent) => void
  onDoubleClick: () => void
  onContextMenu: (e: React.MouseEvent) => void
}

export default function FileCard({ entry, selected, onClick, onDoubleClick, onContextMenu }: FileCardProps) {
  return (
    <div
      onClick={onClick}
      onDoubleClick={onDoubleClick}
      onContextMenu={onContextMenu}
      className={cn(
        'group flex cursor-pointer flex-col items-center gap-2.5 rounded-xl border bg-white p-4 transition-all duration-200',
        'hover:-translate-y-1 hover:shadow-[0_10px_25px_-5px_rgba(79,70,229,0.15),0_8px_10px_-6px_rgba(79,70,229,0.1)]',
        selected
          ? 'border-indigo-300 bg-indigo-50/50 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.15)]'
          : 'border-slate-100 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.06)] hover:border-slate-200',
      )}
    >
      <div className={cn(
        'flex h-12 w-12 items-center justify-center rounded-xl transition-colors',
        entry.type === 'dir' ? 'bg-indigo-50' : 'bg-slate-50',
      )}>
        <FileIcon name={entry.name} type={entry.type} className="h-6 w-6" />
      </div>
      <span className="w-full truncate text-center text-sm font-semibold text-slate-900">{entry.name}</span>
      <span className="text-xs text-slate-400">
        {entry.type === 'dir' ? 'Folder' : formatSize(entry.size)}
      </span>
    </div>
  )
}

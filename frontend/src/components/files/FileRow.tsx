import FileIcon from './FileIcon'
import { cn } from '@/lib/utils'
import type { FileEntry } from '@/types/api'

function formatSize(bytes: number): string {
  if (bytes === 0) return '-'
  const units = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(1024))
  return `${(bytes / Math.pow(1024, i)).toFixed(i > 0 ? 1 : 0)} ${units[i]}`
}

function formatDate(iso: string): string {
  const d = new Date(iso)
  return d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric', hour: '2-digit', minute: '2-digit' })
}

interface FileRowProps {
  entry: FileEntry
  selected: boolean
  onClick: (e: React.MouseEvent) => void
  onDoubleClick: () => void
  onContextMenu: (e: React.MouseEvent) => void
}

export default function FileRow({ entry, selected, onClick, onDoubleClick, onContextMenu }: FileRowProps) {
  return (
    <div
      onClick={onClick}
      onDoubleClick={onDoubleClick}
      onContextMenu={onContextMenu}
      className={cn(
        'group flex cursor-pointer items-center gap-3 rounded-lg border border-transparent px-3 py-2.5 transition-all duration-150',
        selected
          ? 'border-indigo-200 bg-indigo-50/60'
          : 'hover:bg-slate-50',
      )}
    >
      <FileIcon name={entry.name} type={entry.type} />
      <span className="flex-1 truncate text-sm font-medium text-slate-900">{entry.name}</span>
      <span className="w-20 text-right text-xs text-slate-400">
        {entry.type === 'dir' ? '-' : formatSize(entry.size)}
      </span>
      <span className="w-40 text-right text-xs text-slate-400">
        {formatDate(entry.modified)}
      </span>
    </div>
  )
}

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
  focused: boolean
  onClick: (e: React.MouseEvent) => void
  onDoubleClick: () => void
  onContextMenu: (e: React.MouseEvent) => void
  onFocus: () => void
  onKeyDown: (e: React.KeyboardEvent) => void
  itemRef?: (node: HTMLDivElement | null) => void
}

export default function FileRow({
  entry,
  selected,
  focused,
  onClick,
  onDoubleClick,
  onContextMenu,
  onFocus,
  onKeyDown,
  itemRef,
}: FileRowProps) {
  return (
    <div
      ref={itemRef}
      role="option"
      aria-selected={selected}
      tabIndex={focused ? 0 : -1}
      onClick={onClick}
      onDoubleClick={onDoubleClick}
      onContextMenu={onContextMenu}
      onFocus={onFocus}
      onKeyDown={onKeyDown}
      className={cn(
        'group flex cursor-pointer items-center gap-3 rounded-lg border border-transparent px-3 py-2.5 outline-none transition-all duration-150',
        selected
          ? 'border-indigo-200 bg-indigo-50/60'
          : 'hover:bg-slate-50',
        focused && 'ring-2 ring-indigo-400 ring-offset-2 ring-offset-white',
      )}
    >
      <FileIcon name={entry.name} type={entry.type} mime={entry.mime} />
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

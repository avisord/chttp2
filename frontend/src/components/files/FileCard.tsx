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
  focused: boolean
  iconSize: number
  onClick: (e: React.MouseEvent) => void
  onDoubleClick: () => void
  onContextMenu: (e: React.MouseEvent) => void
  onFocus: () => void
  onKeyDown: (e: React.KeyboardEvent) => void
  itemRef?: (node: HTMLDivElement | null) => void
}

export default function FileCard({
  entry,
  selected,
  focused,
  iconSize,
  onClick,
  onDoubleClick,
  onContextMenu,
  onFocus,
  onKeyDown,
  itemRef,
}: FileCardProps) {
  const sizeMap: Record<number, { icon: string; container: string; text: string; subtext: string; padding: string; gap: string }> = {
    1: { icon: 'h-10 w-10', container: 'h-12', text: 'text-xs', subtext: 'text-[0.65rem]', padding: 'px-1.5 py-1.5', gap: 'gap-0.5' },
    2: { icon: 'h-[4.25rem] w-[4.25rem]', container: 'h-20', text: 'text-sm', subtext: 'text-xs', padding: 'px-2 py-2', gap: 'gap-1' },
    3: { icon: 'h-24 w-24', container: 'h-28', text: 'text-base', subtext: 'text-sm', padding: 'px-2.5 py-2.5', gap: 'gap-1' },
    4: { icon: 'h-32 w-32', container: 'h-36', text: 'text-lg', subtext: 'text-sm', padding: 'px-3 py-3', gap: 'gap-1.5' },
  }
  const sizes = sizeMap[iconSize] ?? sizeMap[2]
  const iconSizeClass = sizes.icon

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
        'group flex cursor-pointer flex-col items-center rounded-xl outline-none transition-all duration-150',
        sizes.padding,
        sizes.gap,
        selected
          ? 'bg-indigo-50/70'
          : 'hover:bg-slate-50/80',
        focused && 'ring-2 ring-indigo-400 ring-offset-2 ring-offset-white',
      )}
    >
      <div className={cn(
        'flex w-full items-center justify-center rounded-lg transition-transform duration-150 group-hover:scale-[1.03]',
        sizes.container,
      )}>
        <FileIcon
          name={entry.name}
          type={entry.type}
          mime={entry.mime}
          className={iconSizeClass}
        />
      </div>
      <span className={cn('w-full break-words text-center leading-5 font-medium text-slate-800', sizes.text)}>
        {entry.name}
      </span>
      <span className={cn('text-slate-400', sizes.subtext)}>
        {entry.type === 'dir' ? 'Folder' : formatSize(entry.size)}
      </span>
    </div>
  )
}

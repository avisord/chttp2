import FileRow from './FileRow'
import type { FileEntry } from '@/types/api'

interface FileListProps {
  entries: FileEntry[]
  selectedPaths: string[]
  onItemClick: (entry: FileEntry, e: React.MouseEvent) => void
  onItemDoubleClick: (entry: FileEntry) => void
  onItemContextMenu: (entry: FileEntry, e: React.MouseEvent) => void
}

export default function FileList({
  entries,
  selectedPaths,
  onItemClick,
  onItemDoubleClick,
  onItemContextMenu,
}: FileListProps) {
  return (
    <div className="flex flex-col gap-0.5">
      <div className="flex items-center gap-3 px-3 py-2 text-xs font-semibold uppercase tracking-wider text-slate-400">
        <span className="w-5" />
        <span className="flex-1">Name</span>
        <span className="w-20 text-right">Size</span>
        <span className="w-40 text-right">Modified</span>
      </div>
      {entries.map((entry) => (
        <FileRow
          key={entry.name}
          entry={entry}
          selected={selectedPaths.includes(entry.name)}
          onClick={(e) => onItemClick(entry, e)}
          onDoubleClick={() => onItemDoubleClick(entry)}
          onContextMenu={(e) => onItemContextMenu(entry, e)}
        />
      ))}
    </div>
  )
}

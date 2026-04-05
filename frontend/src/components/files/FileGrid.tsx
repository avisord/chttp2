import FileCard from './FileCard'
import type { FileEntry } from '@/types/api'

interface FileGridProps {
  entries: FileEntry[]
  selectedPaths: string[]
  onItemClick: (entry: FileEntry, e: React.MouseEvent) => void
  onItemDoubleClick: (entry: FileEntry) => void
  onItemContextMenu: (entry: FileEntry, e: React.MouseEvent) => void
}

export default function FileGrid({
  entries,
  selectedPaths,
  onItemClick,
  onItemDoubleClick,
  onItemContextMenu,
}: FileGridProps) {
  return (
    <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6">
      {entries.map((entry) => (
        <FileCard
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

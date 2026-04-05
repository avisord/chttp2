import { useEffect, useRef } from 'react'
import FileCard from './FileCard'
import type { FileEntry } from '@/types/api'

interface FileGridProps {
  entries: FileEntry[]
  selectedPaths: string[]
  activePath: string | null
  iconSize: number
  onItemClick: (entry: FileEntry, e: React.MouseEvent) => void
  onItemDoubleClick: (entry: FileEntry) => void
  onItemContextMenu: (entry: FileEntry, e: React.MouseEvent) => void
  onItemFocus: (entry: FileEntry) => void
  onItemKeyDown: (entry: FileEntry, e: React.KeyboardEvent) => void
  onColumnCountChange: (count: number) => void
  getItemRef: (name: string) => (node: HTMLDivElement | null) => void
}

export default function FileGrid({
  entries,
  selectedPaths,
  activePath,
  iconSize,
  onItemClick,
  onItemDoubleClick,
  onItemContextMenu,
  onItemFocus,
  onItemKeyDown,
  onColumnCountChange,
  getItemRef,
}: FileGridProps) {
  const gridRef = useRef<HTMLDivElement | null>(null)

  useEffect(() => {
    const node = gridRef.current
    if (!node) return

    const updateColumns = () => {
      const template = window.getComputedStyle(node).gridTemplateColumns
      const count = template ? template.split(' ').filter(Boolean).length : 1
      onColumnCountChange(Math.max(count, 1))
    }

    updateColumns()
    const observer = new ResizeObserver(updateColumns)
    observer.observe(node)
    window.addEventListener('resize', updateColumns)

    return () => {
      observer.disconnect()
      window.removeEventListener('resize', updateColumns)
    }
  }, [onColumnCountChange])

  const sizeToGrid: Record<number, string> = {
    1: 'grid grid-cols-3 gap-1 sm:grid-cols-4 md:grid-cols-6 lg:grid-cols-8 xl:grid-cols-10',
    2: 'grid grid-cols-2 gap-1 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6',
    3: 'grid grid-cols-2 gap-2 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5',
    4: 'grid grid-cols-1 gap-2 sm:grid-cols-2 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4',
  }
  const gridClasses = sizeToGrid[iconSize] ?? sizeToGrid[2]

  return (
    <div
      ref={gridRef}
      role="listbox"
      aria-label="Files"
      className={gridClasses}
    >
      {entries.map((entry) => (
        <FileCard
          key={entry.name}
          entry={entry}
          selected={selectedPaths.includes(entry.name)}
          focused={activePath === entry.name}
          iconSize={iconSize}
          onClick={(e) => onItemClick(entry, e)}
          onDoubleClick={() => onItemDoubleClick(entry)}
          onContextMenu={(e) => onItemContextMenu(entry, e)}
          onFocus={() => onItemFocus(entry)}
          onKeyDown={(e) => onItemKeyDown(entry, e)}
          itemRef={getItemRef(entry.name)}
        />
      ))}
    </div>
  )
}

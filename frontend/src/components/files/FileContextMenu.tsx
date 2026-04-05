import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuTrigger,
} from '@/components/ui/context-menu'
import { FolderOpen, Download, Pencil, Copy, Trash2, Info, FolderPlus, Upload, RefreshCw } from 'lucide-react'
import type { FileEntry } from '@/types/api'

interface FileContextMenuProps {
  children: React.ReactNode
  entry?: FileEntry
  onOpen?: () => void
  onDownload?: () => void
  onRename?: () => void
  onCopy?: () => void
  onDelete?: () => void
  onInfo?: () => void
  onNewFolder?: () => void
  onUpload?: () => void
  onRefresh?: () => void
}

export default function FileContextMenu({
  children,
  entry,
  onOpen,
  onDownload,
  onRename,
  onCopy,
  onDelete,
  onInfo,
  onNewFolder,
  onUpload,
  onRefresh,
}: FileContextMenuProps) {
  return (
    <ContextMenu>
      <ContextMenuTrigger asChild>{children}</ContextMenuTrigger>
      <ContextMenuContent className="w-48">
        {entry ? (
          <>
            <ContextMenuItem onClick={onOpen}>
              <FolderOpen className="mr-2 h-4 w-4" /> Open
            </ContextMenuItem>
            {entry.type === 'file' && (
              <ContextMenuItem onClick={onDownload}>
                <Download className="mr-2 h-4 w-4" /> Download
              </ContextMenuItem>
            )}
            <ContextMenuSeparator />
            <ContextMenuItem onClick={onRename}>
              <Pencil className="mr-2 h-4 w-4" /> Rename
            </ContextMenuItem>
            <ContextMenuItem onClick={onCopy}>
              <Copy className="mr-2 h-4 w-4" /> Copy
            </ContextMenuItem>
            <ContextMenuSeparator />
            <ContextMenuItem onClick={onDelete} className="text-destructive">
              <Trash2 className="mr-2 h-4 w-4" /> Delete
            </ContextMenuItem>
            <ContextMenuSeparator />
            <ContextMenuItem onClick={onInfo}>
              <Info className="mr-2 h-4 w-4" /> Properties
            </ContextMenuItem>
          </>
        ) : (
          <>
            <ContextMenuItem onClick={onNewFolder}>
              <FolderPlus className="mr-2 h-4 w-4" /> New Folder
            </ContextMenuItem>
            <ContextMenuItem onClick={onUpload}>
              <Upload className="mr-2 h-4 w-4" /> Upload
            </ContextMenuItem>
            <ContextMenuSeparator />
            <ContextMenuItem onClick={onRefresh}>
              <RefreshCw className="mr-2 h-4 w-4" /> Refresh
            </ContextMenuItem>
          </>
        )}
      </ContextMenuContent>
    </ContextMenu>
  )
}

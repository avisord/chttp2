import { useRef } from 'react'
import { Grid3x3, List, FolderPlus, Upload, ArrowUpDown, Trash2, Download } from 'lucide-react'
import { Button } from '@/components/ui/button'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { setViewMode, setSortBy } from '@/store/slices/fileSystemSlice'

interface ToolbarProps {
  onNewFolder: () => void
  onUpload: (files: FileList) => void
  onBulkDelete: () => void
  onBulkDownload: () => void
}

export default function Toolbar({ onNewFolder, onUpload, onBulkDelete, onBulkDownload }: ToolbarProps) {
  const dispatch = useAppDispatch()
  const { viewMode, sortBy, sortOrder, selectedPaths } = useAppSelector((s) => s.fileSystem)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const hasSelection = selectedPaths.length > 0

  return (
    <div className="flex items-center justify-between gap-2">
      <div className="flex items-center gap-1.5">
        {hasSelection ? (
          <>
            <span className="mr-1 text-sm font-medium text-slate-500">
              {selectedPaths.length} selected
            </span>
            <Button variant="ghost" size="sm" onClick={onBulkDelete} className="text-red-500 hover:bg-red-50 hover:text-red-600">
              <Trash2 className="mr-1 h-4 w-4" /> Delete
            </Button>
            <Button variant="ghost" size="sm" onClick={onBulkDownload} className="text-slate-600 hover:bg-slate-100">
              <Download className="mr-1 h-4 w-4" /> Download
            </Button>
          </>
        ) : (
          <>
            <Button variant="ghost" size="sm" onClick={onNewFolder} className="text-slate-600 hover:bg-slate-100 hover:text-slate-900">
              <FolderPlus className="mr-1.5 h-4 w-4" /> New Folder
            </Button>
            <Button
              size="sm"
              onClick={() => fileInputRef.current?.click()}
              className="rounded-lg bg-gradient-to-r from-indigo-600 to-violet-600 text-white shadow-[0_4px_14px_0_rgba(79,70,229,0.3)] transition-all duration-200 hover:-translate-y-0.5 hover:from-indigo-500 hover:to-violet-500 hover:shadow-[0_6px_20px_0_rgba(79,70,229,0.4)]"
            >
              <Upload className="mr-1.5 h-4 w-4" /> Upload
            </Button>
            <input
              ref={fileInputRef}
              type="file"
              multiple
              className="hidden"
              onChange={(e) => {
                if (e.target.files?.length) onUpload(e.target.files)
                e.target.value = ''
              }}
            />
          </>
        )}
      </div>

      <div className="flex items-center gap-1">
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button variant="ghost" size="sm" className="text-slate-500 hover:bg-slate-100 hover:text-slate-700">
              <ArrowUpDown className="mr-1 h-4 w-4" />
              {sortBy}
              {sortOrder === 'asc' ? ' \u2191' : ' \u2193'}
            </Button>
          </DropdownMenuTrigger>
          <DropdownMenuContent align="end">
            <DropdownMenuItem onClick={() => dispatch(setSortBy('name'))}>Name</DropdownMenuItem>
            <DropdownMenuItem onClick={() => dispatch(setSortBy('size'))}>Size</DropdownMenuItem>
            <DropdownMenuItem onClick={() => dispatch(setSortBy('modified'))}>Modified</DropdownMenuItem>
          </DropdownMenuContent>
        </DropdownMenu>

        <div className="ml-1 flex rounded-lg border border-slate-200 p-0.5">
          <button
            onClick={() => dispatch(setViewMode('grid'))}
            className={`rounded-md p-1.5 transition-all ${viewMode === 'grid' ? 'bg-indigo-50 text-indigo-600 shadow-sm' : 'text-slate-400 hover:text-slate-600'}`}
          >
            <Grid3x3 className="h-4 w-4" />
          </button>
          <button
            onClick={() => dispatch(setViewMode('list'))}
            className={`rounded-md p-1.5 transition-all ${viewMode === 'list' ? 'bg-indigo-50 text-indigo-600 shadow-sm' : 'text-slate-400 hover:text-slate-600'}`}
          >
            <List className="h-4 w-4" />
          </button>
        </div>
      </div>
    </div>
  )
}

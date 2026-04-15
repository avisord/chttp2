import { useRef, type RefObject } from 'react'
import { Grid3x3, List, FolderPlus, Upload, ArrowUpDown, Trash2, Download, Search, X, Minus, Plus, RotateCcw } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { setSortBy } from '@/store/slices/fileSystemSlice'
import { setViewMode, setIconSize } from '@/store/slices/settingsSlice'

interface ToolbarProps {
  onNewFolder: () => void
  onUpload: (files: FileList) => void
  onBulkDelete: () => void
  onBulkDownload: () => void
  searchQuery: string
  onSearchChange: (value: string) => void
  onSearchKeyDown: (e: React.KeyboardEvent<HTMLInputElement>) => void
  searchInputRef: RefObject<HTMLInputElement | null>
  trashMode?: boolean
  onBulkRestore?: () => void
  onEmptyTrash?: () => void
}

export default function Toolbar({
  onNewFolder,
  onUpload,
  onBulkDelete,
  onBulkDownload,
  searchQuery,
  onSearchChange,
  onSearchKeyDown,
  searchInputRef,
  trashMode,
  onBulkRestore,
  onEmptyTrash,
}: ToolbarProps) {
  const dispatch = useAppDispatch()
  const { sortBy, sortOrder, selectedPaths } = useAppSelector((s) => s.fileSystem)
  const { viewMode, iconSize } = useAppSelector((s) => s.settings.values)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const hasSelection = selectedPaths.length > 0

  return (
    <div className="flex flex-col gap-2 lg:flex-row lg:items-center lg:justify-between">
      <div className="flex flex-1 flex-wrap items-center gap-2">
        <div className="relative min-w-[220px] flex-1 lg:max-w-sm">
          <Search className="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-slate-400" />
          <Input
            ref={searchInputRef}
            value={searchQuery}
            onChange={(e) => onSearchChange(e.target.value)}
            onKeyDown={onSearchKeyDown}
            placeholder="Search in this folder"
            className="h-9 border-slate-200 bg-slate-50 pl-9 pr-9 text-sm focus-visible:bg-white"
            aria-label="Search files in current folder"
          />
          {searchQuery && (
            <button
              type="button"
              onClick={() => onSearchChange('')}
              className="absolute right-2 top-1/2 flex h-6 w-6 -translate-y-1/2 items-center justify-center rounded-md text-slate-400 transition hover:bg-slate-200 hover:text-slate-600"
              aria-label="Clear search"
            >
              <X className="h-4 w-4" />
            </button>
          )}
        </div>

        {trashMode ? (
          <>
            {hasSelection && (
              <>
                <span className="mr-1 text-sm font-medium text-slate-500">
                  {selectedPaths.length} selected
                </span>
                <Button variant="ghost" size="sm" onClick={onBulkRestore} className="text-slate-600 hover:bg-slate-100">
                  <RotateCcw className="mr-1 h-4 w-4" /> Restore
                </Button>
                <Button variant="ghost" size="sm" onClick={onBulkDelete} className="text-red-500 hover:bg-red-50 hover:text-red-600">
                  <Trash2 className="mr-1 h-4 w-4" /> Delete Permanently
                </Button>
              </>
            )}
            {!hasSelection && (
              <Button variant="ghost" size="sm" onClick={onEmptyTrash} className="text-red-500 hover:bg-red-50 hover:text-red-600">
                <Trash2 className="mr-1 h-4 w-4" /> Empty Trash
              </Button>
            )}
          </>
        ) : hasSelection ? (
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

      <div className="flex items-center justify-end gap-1">
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

        {viewMode === 'grid' && (
          <div className="flex items-center gap-0.5 rounded-lg border border-slate-200 p-0.5">
            <button
              onClick={() => dispatch(setIconSize(iconSize - 1))}
              disabled={iconSize <= 1}
              className="rounded-md p-1.5 text-slate-400 transition-all hover:text-slate-600 disabled:opacity-30 disabled:hover:text-slate-400"
              aria-label="Decrease icon size"
            >
              <Minus className="h-3.5 w-3.5" />
            </button>
            <span className="min-w-[1.25rem] text-center text-xs font-medium text-slate-500">{iconSize}</span>
            <button
              onClick={() => dispatch(setIconSize(iconSize + 1))}
              disabled={iconSize >= 4}
              className="rounded-md p-1.5 text-slate-400 transition-all hover:text-slate-600 disabled:opacity-30 disabled:hover:text-slate-400"
              aria-label="Increase icon size"
            >
              <Plus className="h-3.5 w-3.5" />
            </button>
          </div>
        )}

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

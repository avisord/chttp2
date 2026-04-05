import { useEffect, useState, useCallback, useRef, useMemo } from 'react'
import { useSearchParams } from 'react-router-dom'
import { FolderOpen } from 'lucide-react'
import { toast } from 'sonner'

import { useAppDispatch, useAppSelector } from '@/store/hooks'
import {
  listDirThunk,
  setCurrentPath,
  setSelectedPaths,
  toggleSelect,
  clearSelection,
  setPreviewFile,
  setClipboard,
  deleteFileThunk,
  deleteDirThunk,
  copyFileThunk,
  moveFileThunk,
} from '@/store/slices/fileSystemSlice'
import { addUpload, updateProgress, setUploadStatus } from '@/store/slices/uploadsSlice'
import { simpleUpload, downloadFile } from '@/api/filesystem'
import { Skeleton } from '@/components/ui/skeleton'
import type { FileEntry } from '@/types/api'

import Breadcrumbs from './Breadcrumbs'
import Toolbar from './Toolbar'
import FileGrid from './FileGrid'
import FileList from './FileList'
import FilePreview from './FilePreview'
import FileContextMenu from './FileContextMenu'
import NewFolderDialog from './NewFolderDialog'
import RenameDialog from './RenameDialog'
import DeleteConfirmDialog from './DeleteConfirmDialog'

export default function FileBrowser() {
  const dispatch = useAppDispatch()
  const [searchParams, setSearchParams] = useSearchParams()
  const {
    currentPath,
    entries,
    status,
    viewMode,
    sortBy,
    sortOrder,
    selectedPaths,
    previewFile,
    clipboard,
  } = useAppSelector((s) => s.fileSystem)

  const [newFolderOpen, setNewFolderOpen] = useState(false)
  const [renameOpen, setRenameOpen] = useState(false)
  const [renameTarget, setRenameTarget] = useState('')
  const [deleteOpen, setDeleteOpen] = useState(false)
  const [deleteTargets, setDeleteTargets] = useState<string[]>([])
  const [dragging, setDragging] = useState(false)
  const [searchQuery, setSearchQuery] = useState('')
  const [searchOriginPath, setSearchOriginPath] = useState<string | null>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const searchInputRef = useRef<HTMLInputElement>(null)
  const lastClickedRef = useRef<string | null>(null)

  // Sync path from URL on mount
  useEffect(() => {
    const path = searchParams.get('path') || '.'
    dispatch(setCurrentPath(path))
    dispatch(listDirThunk(path))
  }, []) // eslint-disable-line react-hooks/exhaustive-deps

  // Sort entries
  const sortedEntries = useMemo(() => {
    const sorted = [...entries].sort((a, b) => {
      if (a.type !== b.type) return a.type === 'dir' ? -1 : 1
      let cmp = 0
      if (sortBy === 'name') cmp = a.name.localeCompare(b.name)
      else if (sortBy === 'size') cmp = a.size - b.size
      else cmp = new Date(a.modified).getTime() - new Date(b.modified).getTime()
      return sortOrder === 'asc' ? cmp : -cmp
    })
    return sorted
  }, [entries, sortBy, sortOrder])

  const resolvePath = useCallback(
    (name: string) => (currentPath === '.' ? name : `${currentPath}/${name}`),
    [currentPath],
  )

  const filteredEntries = useMemo(() => {
    const query = searchQuery.trim().toLowerCase()
    if (!query) return sortedEntries
    return sortedEntries.filter((entry) => entry.name.toLowerCase().includes(query))
  }, [searchQuery, sortedEntries])

  const navigateTo = useCallback(
    (path: string) => {
      dispatch(setCurrentPath(path))
      dispatch(listDirThunk(path))
      setSearchParams(path === '.' ? {} : { path })
    },
    [dispatch, setSearchParams],
  )

  const updateSearchQuery = useCallback(
    (value: string) => {
      if (value && !searchQuery) setSearchOriginPath(currentPath)
      if (!value) setSearchOriginPath(null)
      setSearchQuery(value)
    },
    [currentPath, searchQuery],
  )

  const startSearch = useCallback(
    (value: string) => {
      if (!value) return
      if (!searchQuery) setSearchOriginPath(currentPath)
      setSearchQuery((prev) => `${prev}${value}`)
      requestAnimationFrame(() => {
        searchInputRef.current?.focus()
        searchInputRef.current?.setSelectionRange(searchInputRef.current.value.length, searchInputRef.current.value.length)
      })
    },
    [currentPath, searchQuery],
  )

  const clearSearch = useCallback(
    (restoreOriginPath = false) => {
      const originPath = searchOriginPath
      setSearchQuery('')
      setSearchOriginPath(null)
      if (restoreOriginPath && originPath && originPath !== currentPath) {
        navigateTo(originPath)
      }
    },
    [currentPath, navigateTo, searchOriginPath],
  )

  const handleItemClick = useCallback(
    (entry: FileEntry, e: React.MouseEvent) => {
      if (e.ctrlKey || e.metaKey) {
        dispatch(toggleSelect(entry.name))
      } else if (e.shiftKey && lastClickedRef.current) {
        const names = filteredEntries.map((e) => e.name)
        const start = names.indexOf(lastClickedRef.current)
        const end = names.indexOf(entry.name)
        const range = names.slice(Math.min(start, end), Math.max(start, end) + 1)
        dispatch(setSelectedPaths(range))
      } else {
        dispatch(setSelectedPaths([entry.name]))
        dispatch(setPreviewFile(entry))
      }
      lastClickedRef.current = entry.name
    },
    [dispatch, filteredEntries],
  )

  const handleItemDoubleClick = useCallback(
    (entry: FileEntry) => {
      if (entry.type === 'dir') {
        navigateTo(resolvePath(entry.name))
      } else {
        dispatch(setPreviewFile(entry))
      }
    },
    [dispatch, navigateTo, resolvePath],
  )

  const handleUploadFiles = useCallback(
    (files: FileList) => {
      Array.from(files).forEach((file) => {
        const id = crypto.randomUUID()
        dispatch(addUpload({ id, filename: file.name, totalSize: file.size, uploadedSize: 0, status: 'pending' }))
        dispatch(setUploadStatus({ id, status: 'uploading' }))
        simpleUpload(currentPath, file, (pct) => {
          dispatch(updateProgress({ id, uploadedSize: (pct / 100) * file.size }))
        })
          .then(() => {
            dispatch(setUploadStatus({ id, status: 'completed' }))
            dispatch(listDirThunk(currentPath))
            toast.success(`Uploaded "${file.name}"`)
          })
          .catch((err) => {
            dispatch(setUploadStatus({ id, status: 'error', error: err.message }))
            toast.error(`Upload failed: ${file.name}`)
          })
      })
    },
    [dispatch, currentPath],
  )

  const handleDelete = useCallback(
    async (names: string[]) => {
      for (const name of names) {
        const entry = entries.find((e) => e.name === name)
        if (!entry) continue
        const fullPath = resolvePath(name)
        try {
          if (entry.type === 'dir') await dispatch(deleteDirThunk(fullPath)).unwrap()
          else await dispatch(deleteFileThunk(fullPath)).unwrap()
        } catch (err: unknown) {
          toast.error(`Failed to delete ${name}: ${err instanceof Error ? err.message : 'Unknown error'}`)
        }
      }
      dispatch(clearSelection())
      toast.success(`Deleted ${names.length} item${names.length > 1 ? 's' : ''}`)
    },
    [dispatch, entries, resolvePath],
  )

  const handlePaste = useCallback(async () => {
    if (!clipboard) return
    for (const sourcePath of clipboard.paths) {
      const name = sourcePath.split('/').pop() || sourcePath
      const destPath = resolvePath(name)
      try {
        if (clipboard.operation === 'copy') {
          await dispatch(copyFileThunk({ from: sourcePath, to: destPath })).unwrap()
        } else {
          await dispatch(moveFileThunk({ from: sourcePath, to: destPath })).unwrap()
        }
      } catch (err: unknown) {
        toast.error(`${clipboard.operation} failed: ${err instanceof Error ? err.message : 'Unknown error'}`)
      }
    }
    if (clipboard.operation === 'cut') dispatch(setClipboard(null))
    toast.success(`${clipboard.operation === 'copy' ? 'Copied' : 'Moved'} ${clipboard.paths.length} item${clipboard.paths.length > 1 ? 's' : ''}`)
  }, [clipboard, dispatch, resolvePath])

  // Keyboard shortcuts
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const target = e.target
      const isEditableTarget =
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)

      if (e.key === 'Escape' && searchQuery) {
        e.preventDefault()
        clearSearch(true)
        searchInputRef.current?.blur()
        dispatch(clearSelection())
        dispatch(setPreviewFile(null))
        return
      }

      if (isEditableTarget) return

      if (!e.ctrlKey && !e.metaKey && !e.altKey && e.key.length === 1) {
        e.preventDefault()
        startSearch(e.key)
        return
      }

      if (e.key === 'Delete' && selectedPaths.length) {
        e.preventDefault()
        setDeleteTargets(selectedPaths)
        setDeleteOpen(true)
      }
      if (e.key === 'F2' && selectedPaths.length === 1) {
        e.preventDefault()
        setRenameTarget(selectedPaths[0])
        setRenameOpen(true)
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'a') {
        e.preventDefault()
        dispatch(setSelectedPaths(filteredEntries.map((e) => e.name)))
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'c' && selectedPaths.length) {
        e.preventDefault()
        dispatch(setClipboard({ operation: 'copy', paths: selectedPaths.map(resolvePath) }))
        toast('Copied to clipboard')
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'x' && selectedPaths.length) {
        e.preventDefault()
        dispatch(setClipboard({ operation: 'cut', paths: selectedPaths.map(resolvePath) }))
        toast('Cut to clipboard')
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'v' && clipboard) {
        e.preventDefault()
        handlePaste()
      }
      if (e.key === 'Escape') {
        dispatch(clearSelection())
        dispatch(setPreviewFile(null))
      }
      if (e.key === 'Backspace' && !selectedPaths.length && currentPath !== '.') {
        e.preventDefault()
        const parent = currentPath.includes('/') ? currentPath.split('/').slice(0, -1).join('/') || '.' : '.'
        navigateTo(parent)
      }
    }
    window.addEventListener('keydown', handler)
    return () => window.removeEventListener('keydown', handler)
  }, [
    selectedPaths,
    filteredEntries,
    clipboard,
    currentPath,
    dispatch,
    resolvePath,
    navigateTo,
    handlePaste,
    searchQuery,
    startSearch,
    clearSearch,
  ])

  // Drag and drop
  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault()
    setDragging(true)
  }
  const handleDragLeave = () => setDragging(false)
  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault()
    setDragging(false)
    if (e.dataTransfer.files.length) {
      handleUploadFiles(e.dataTransfer.files)
    }
  }

  return (
    <div
      className="relative flex h-full flex-col"
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
    >
      {/* Drop zone overlay */}
      {dragging && (
        <div className="absolute inset-0 z-50 flex items-center justify-center rounded-xl border-2 border-dashed border-indigo-400 bg-indigo-50/80 backdrop-blur-sm">
          <div className="text-center">
            <div className="mx-auto mb-3 flex h-14 w-14 items-center justify-center rounded-xl bg-indigo-100">
              <FolderOpen className="h-7 w-7 text-indigo-600" />
            </div>
            <p className="text-base font-semibold text-indigo-600">Drop files to upload</p>
            <p className="mt-1 text-sm text-slate-500">Files will be added to the current folder</p>
          </div>
        </div>
      )}

      <div className="flex items-center justify-between border-b border-slate-200 bg-white px-5 py-3">
        <Breadcrumbs />
      </div>

      <div className="border-b border-slate-100 bg-white px-5 py-2">
        <Toolbar
          onNewFolder={() => setNewFolderOpen(true)}
          onUpload={handleUploadFiles}
          onBulkDelete={() => {
            setDeleteTargets(selectedPaths)
            setDeleteOpen(true)
          }}
          onBulkDownload={() => {
            selectedPaths.forEach((name) => {
              const entry = entries.find((e) => e.name === name)
              if (entry?.type === 'file') downloadFile(resolvePath(name), name)
            })
          }}
          searchQuery={searchQuery}
          onSearchChange={updateSearchQuery}
          searchInputRef={searchInputRef}
        />
      </div>

      <div className="flex flex-1 overflow-hidden">
        <div
          className="flex-1 overflow-auto p-5"
          onClick={(e) => {
            if (e.target === e.currentTarget) dispatch(clearSelection())
          }}
        >
          {status === 'loading' && !entries.length ? (
            <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5">
              {Array.from({ length: 8 }).map((_, i) => (
                <Skeleton key={i} className="h-32 rounded-xl" />
              ))}
            </div>
          ) : filteredEntries.length === 0 ? (
            <div className="flex flex-col items-center justify-center py-24 text-slate-400">
              <div className="mb-4 flex h-16 w-16 items-center justify-center rounded-xl bg-slate-100">
                <FolderOpen className="h-8 w-8 text-slate-300" />
              </div>
              <p className="text-sm font-semibold text-slate-500">
                {searchQuery ? 'No matches found' : 'This folder is empty'}
              </p>
              <p className="mt-1 text-xs text-slate-400">
                {searchQuery ? `No files or folders match "${searchQuery}"` : 'Drop files here or click Upload to get started'}
              </p>
            </div>
          ) : (
            <FileContextMenu
              onNewFolder={() => setNewFolderOpen(true)}
              onUpload={() => fileInputRef.current?.click()}
              onRefresh={() => dispatch(listDirThunk(currentPath))}
            >
              <div>
                {viewMode === 'grid' ? (
                  <FileGrid
                    entries={filteredEntries}
                    selectedPaths={selectedPaths}
                    onItemClick={handleItemClick}
                    onItemDoubleClick={handleItemDoubleClick}
                    onItemContextMenu={(entry, e) => {
                      e.preventDefault()
                      dispatch(setSelectedPaths([entry.name]))
                    }}
                  />
                ) : (
                  <FileList
                    entries={filteredEntries}
                    selectedPaths={selectedPaths}
                    onItemClick={handleItemClick}
                    onItemDoubleClick={handleItemDoubleClick}
                    onItemContextMenu={(entry, e) => {
                      e.preventDefault()
                      dispatch(setSelectedPaths([entry.name]))
                    }}
                  />
                )}
              </div>
            </FileContextMenu>
          )}

          <input
            ref={fileInputRef}
            type="file"
            multiple
            className="hidden"
            onChange={(e) => {
              if (e.target.files?.length) handleUploadFiles(e.target.files)
              e.target.value = ''
            }}
          />
        </div>

        {previewFile && <FilePreview />}
      </div>

      {/* Dialogs */}
      <NewFolderDialog open={newFolderOpen} onOpenChange={setNewFolderOpen} />
      <RenameDialog open={renameOpen} onOpenChange={setRenameOpen} currentName={renameTarget} />
      <DeleteConfirmDialog
        open={deleteOpen}
        onOpenChange={setDeleteOpen}
        names={deleteTargets}
        onConfirm={() => {
          handleDelete(deleteTargets)
          setDeleteOpen(false)
        }}
      />
    </div>
  )
}

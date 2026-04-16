import { useEffect, useState, useCallback } from 'react'
import { ChevronDown, ChevronUp, X, Upload, Check, AlertCircle, Loader2, Pause, Play } from 'lucide-react'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { upsertItem, clearDone, removeItem, toggleCollapsed } from '@/store/slices/uploadsSlice'
import { uploadEngine, uploadPercent, fmtBytes, fmtSecs } from '@/lib/uploadEngine'
import type { UploadItemState } from '@/lib/uploadEngine'
import { cn } from '@/lib/utils'

function PhaseIcon({ phase }: { phase: UploadItemState['phase'] }) {
  switch (phase) {
    case 'complete':
      return (
        <div className="flex h-5 w-5 shrink-0 items-center justify-center rounded-full bg-emerald-100 dark:bg-emerald-900/40">
          <Check className="h-3 w-3 text-emerald-600 dark:text-emerald-400" />
        </div>
      )
    case 'error':
      return (
        <div className="flex h-5 w-5 shrink-0 items-center justify-center rounded-full bg-red-100 dark:bg-red-900/40">
          <AlertCircle className="h-3 w-3 text-red-500 dark:text-red-400" />
        </div>
      )
    case 'hashing':
    case 'uploading':
      return (
        <div className="flex h-5 w-5 shrink-0 items-center justify-center">
          <Loader2 className="h-4 w-4 animate-spin text-indigo-500" />
        </div>
      )
    default:
      return (
        <div className="flex h-5 w-5 shrink-0 items-center justify-center rounded-full bg-slate-100 dark:bg-slate-700">
          <Upload className="h-3 w-3 text-slate-400" />
        </div>
      )
  }
}

function statusText(item: UploadItemState): string {
  const pct = uploadPercent(item)
  switch (item.phase) {
    case 'pending':
      return 'Waiting...'
    case 'hashing':
      return `Preparing... ${item.hashDone}/${item.chunkCount} chunks`
    case 'uploading': {
      let text = `${pct}%`
      if (item.speed > 0) text += ` · ${fmtBytes(item.speed)}/s`
      if (item.eta < Infinity && item.eta > 0) text += ` · ${fmtSecs(item.eta)} left`
      return text
    }
    case 'complete':
      return 'Complete'
    case 'error':
      return item.error || 'Upload failed'
    default:
      return ''
  }
}

function UploadItemRow({ item }: { item: UploadItemState }) {
  const dispatch = useAppDispatch()
  const pct = uploadPercent(item)
  const isActive = item.phase === 'hashing' || item.phase === 'uploading'
  const isDone = item.phase === 'complete'
  const isError = item.phase === 'error'

  const handleRemove = (e: React.MouseEvent) => {
    e.stopPropagation()
    uploadEngine.abort(item.id)
    dispatch(removeItem(item.id))
  }

  return (
    <div
      className={cn(
        'border-b border-slate-100 px-4 py-2.5 last:border-b-0 dark:border-slate-700',
        isDone && 'opacity-70',
      )}
    >
      <div className="flex items-center gap-2.5">
        <PhaseIcon phase={item.phase} />
        <div className="min-w-0 flex-1">
          <div className="flex items-center justify-between gap-2">
            <span className="truncate text-[13px] font-medium text-slate-800 dark:text-slate-200">
              {item.filename}
            </span>
            <button
              onClick={handleRemove}
              className="shrink-0 rounded p-0.5 text-slate-400 transition-colors hover:text-slate-600 dark:hover:text-slate-300"
              title={isActive ? 'Cancel' : 'Remove'}
            >
              <X className="h-3.5 w-3.5" />
            </button>
          </div>
          <div className="mt-0.5 flex items-center gap-2">
            <span
              className={cn(
                'text-[11px]',
                isDone && 'text-emerald-600 dark:text-emerald-400',
                isError && 'text-red-500 dark:text-red-400',
                isActive && 'text-slate-500 dark:text-slate-400',
                !isDone && !isError && !isActive && 'text-slate-400 dark:text-slate-500',
              )}
            >
              {statusText(item)}
            </span>
            {!isDone && !isError && item.phase !== 'pending' && (
              <span className="text-[11px] text-slate-400 dark:text-slate-500">
                {fmtBytes(item.totalSize)}
              </span>
            )}
          </div>
        </div>
      </div>
      {(isActive || isDone) && (
        <div className="ml-[30px] mt-1.5 h-1 overflow-hidden rounded-full bg-slate-100 dark:bg-slate-700">
          <div
            className={cn(
              'h-full rounded-full transition-[width] duration-300',
              item.phase === 'hashing' && 'bg-blue-500',
              item.phase === 'uploading' && 'bg-indigo-500',
              isDone && 'bg-emerald-500',
            )}
            style={{ width: `${pct}%` }}
          />
        </div>
      )}
    </div>
  )
}

export default function UploadManager() {
  const dispatch = useAppDispatch()
  const { items, visible, collapsed } = useAppSelector((s) => s.uploads)
  const [paused, setPaused] = useState(false)

  /* Bridge engine events → Redux */
  useEffect(() => {
    const unsub = uploadEngine.onChange((item) => {
      dispatch(upsertItem(item))
    })
    return unsub
  }, [dispatch])

  const handlePause = useCallback((e: React.MouseEvent) => {
    e.stopPropagation()
    uploadEngine.togglePause()
    setPaused(uploadEngine.isPaused)
  }, [])

  const handleClearDone = useCallback((e: React.MouseEvent) => {
    e.stopPropagation()
    uploadEngine.clearDone()
    dispatch(clearDone())
  }, [dispatch])

  if (!visible || items.length === 0) return null

  const activeCount = items.filter(
    (i) => i.phase === 'uploading' || i.phase === 'hashing' || i.phase === 'pending',
  ).length
  const completedCount = items.filter((i) => i.phase === 'complete').length
  const totalCount = items.length

  const headerText =
    activeCount > 0
      ? `Uploading ${activeCount} item${activeCount > 1 ? 's' : ''}`
      : completedCount === totalCount
        ? `${completedCount} upload${completedCount > 1 ? 's' : ''} complete`
        : 'Uploads'

  return (
    <div
      style={{
        position: 'fixed',
        bottom: '16px',
        right: '16px',
        width: '360px',
        zIndex: 9999,
      }}
      className="overflow-hidden rounded-lg border border-slate-200 bg-white shadow-xl dark:border-slate-700 dark:bg-slate-900"
    >
      {/* Header — always visible, click to collapse */}
      <div
        className="flex cursor-pointer items-center justify-between px-4 py-2.5"
        onClick={() => dispatch(toggleCollapsed())}
      >
        <span className="text-[13px] font-semibold text-slate-900 dark:text-slate-100">
          {headerText}
        </span>
        <div className="flex items-center gap-1">
          {completedCount > 0 && !collapsed && (
            <button
              className="mr-1 text-[11px] font-medium text-indigo-500 hover:text-indigo-600 dark:text-indigo-400"
              onClick={handleClearDone}
            >
              Clear done
            </button>
          )}
          {/* Pause / Resume button — only when uploads are active */}
          {activeCount > 0 && (
            <button
              onClick={handlePause}
              className={cn(
                'mr-0.5 rounded p-1 transition-colors',
                paused
                  ? 'text-amber-500 hover:bg-amber-50 dark:hover:bg-amber-950'
                  : 'text-slate-400 hover:bg-slate-100 hover:text-slate-600 dark:hover:bg-slate-800 dark:hover:text-slate-300',
              )}
              title={paused ? 'Resume' : 'Pause'}
            >
              {paused ? <Play className="h-3.5 w-3.5" /> : <Pause className="h-3.5 w-3.5" />}
            </button>
          )}
          {collapsed ? (
            <ChevronUp className="h-4 w-4 text-slate-400" />
          ) : (
            <ChevronDown className="h-4 w-4 text-slate-400" />
          )}
        </div>
      </div>

      {/* Paused banner */}
      {paused && !collapsed && (
        <div className="border-t border-amber-200 bg-amber-50 px-4 py-1.5 text-[11px] font-medium text-amber-700 dark:border-amber-800 dark:bg-amber-950 dark:text-amber-400">
          Uploads paused
        </div>
      )}

      {/* Items list — collapsible */}
      {!collapsed && (
        <div className="max-h-72 overflow-auto border-t border-slate-100 dark:border-slate-700">
          {items.map((item) => (
            <UploadItemRow key={item.id} item={item} />
          ))}
        </div>
      )}
    </div>
  )
}

import { useState, useRef, useEffect, useCallback } from 'react'
import { Home, ChevronRight, CornerDownLeft, X, EllipsisVertical } from 'lucide-react'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { listDirThunk, setCurrentPath } from '@/store/slices/fileSystemSlice'
import { useSearchParams } from 'react-router-dom'

export default function Breadcrumbs() {
  const dispatch = useAppDispatch()
  const currentPath = useAppSelector((s) => s.fileSystem.currentPath)
  const user = useAppSelector((s) => s.auth.user)
  const [, setSearchParams] = useSearchParams()

  const [editing, setEditing] = useState(false)
  const [inputValue, setInputValue] = useState('')
  const inputRef = useRef<HTMLInputElement>(null)
  const previousFocusRef = useRef<Element | null>(null)

  const segments = currentPath === '.' ? [] : currentPath.split('/').filter(Boolean)

  const toAbsolute = useCallback(
    (relPath: string) => {
      const home = user?.home || '/home'
      if (relPath === '.' || relPath === '') return home
      if (relPath.startsWith('/')) return relPath
      return `${home}/${relPath}`
    },
    [user?.home],
  )

  const toRelative = useCallback(
    (absPath: string) => {
      const home = user?.home || '/home'
      if (absPath === home || absPath === home + '/') return '.'
      if (absPath.startsWith(home + '/')) return absPath.slice(home.length + 1)
      // Absolute path outside home — prefix with /
      return absPath
    },
    [user?.home],
  )

  const navigateTo = useCallback(
    (path: string) => {
      dispatch(setCurrentPath(path))
      dispatch(listDirThunk(path))
      setSearchParams(path === '.' ? {} : { path })
    },
    [dispatch, setSearchParams],
  )

  const startEditing = useCallback(() => {
    previousFocusRef.current = document.activeElement
    setInputValue(toAbsolute(currentPath))
    setEditing(true)
  }, [currentPath, toAbsolute])

  const stopEditing = useCallback(
    (restoreFocus = true) => {
      setEditing(false)
      if (restoreFocus && previousFocusRef.current instanceof HTMLElement) {
        previousFocusRef.current.focus()
      }
      previousFocusRef.current = null
    },
    [],
  )

  const submitPath = useCallback(() => {
    const trimmed = inputValue.trim().replace(/\/+$/, '')
    if (!trimmed) {
      stopEditing()
      return
    }
    const rel = trimmed.startsWith('/') ? toRelative(trimmed) : trimmed
    navigateTo(rel)
    stopEditing(false)
  }, [inputValue, navigateTo, stopEditing, toRelative])

  // Focus input when entering edit mode
  useEffect(() => {
    if (editing && inputRef.current) {
      inputRef.current.focus()
      inputRef.current.select()
    }
  }, [editing])

  // Alt+L global shortcut
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.altKey && e.key.toLowerCase() === 'l') {
        e.preventDefault()
        startEditing()
      }
    }
    window.addEventListener('keydown', handler)
    return () => window.removeEventListener('keydown', handler)
  }, [startEditing])

  if (editing) {
    return (
      <div className="flex w-full items-center gap-1">
        <input
          ref={inputRef}
          type="text"
          value={inputValue}
          onChange={(e) => setInputValue(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              e.preventDefault()
              submitPath()
            } else if (e.key === 'Escape') {
              e.preventDefault()
              stopEditing()
            }
          }}
          className="min-w-0 flex-1 rounded border border-slate-300 bg-white px-3 py-1.5 text-sm text-slate-900 outline-none focus:border-indigo-400 focus:ring-1 focus:ring-indigo-400"
        />
        <button
          onClick={submitPath}
          className="flex h-8 w-8 shrink-0 items-center justify-center rounded text-slate-500 transition-colors hover:bg-slate-100 hover:text-slate-700"
          title="Go"
        >
          <CornerDownLeft className="h-4 w-4" />
        </button>
        <button
          onClick={() => stopEditing()}
          className="flex h-8 w-8 shrink-0 items-center justify-center rounded text-slate-500 transition-colors hover:bg-slate-100 hover:text-slate-700"
          title="Cancel (Esc)"
        >
          <X className="h-4 w-4" />
        </button>
      </div>
    )
  }

  return (
    <div className="flex w-full items-center">
      <nav className="flex min-w-0 flex-1 items-center gap-1 text-sm">
        <button
          onClick={() => navigateTo('.')}
          className="flex shrink-0 items-center gap-1 rounded-md px-2 py-1 text-slate-400 transition-colors hover:bg-slate-100 hover:text-slate-700"
        >
          <Home className="h-4 w-4" />
        </button>
        {segments.map((seg, i) => {
          const path = segments.slice(0, i + 1).join('/')
          const isLast = i === segments.length - 1
          return (
            <span key={path} className="flex items-center gap-1">
              <ChevronRight className="h-3.5 w-3.5 shrink-0 text-slate-300" />
              <button
                onClick={() => navigateTo(path)}
                className={`truncate rounded-md px-2 py-1 transition-colors ${
                  isLast
                    ? 'font-semibold text-slate-900'
                    : 'text-slate-500 hover:bg-slate-100 hover:text-slate-700'
                }`}
              >
                {seg}
              </button>
            </span>
          )
        })}
      </nav>
      <button
        onClick={startEditing}
        className="flex h-8 w-8 shrink-0 items-center justify-center rounded text-slate-400 transition-colors hover:bg-slate-100 hover:text-slate-700"
        title="Edit location (Alt+L)"
      >
        <EllipsisVertical className="h-4 w-4" />
      </button>
    </div>
  )
}

import { Home, ChevronRight } from 'lucide-react'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { listDirThunk, setCurrentPath } from '@/store/slices/fileSystemSlice'
import { useSearchParams } from 'react-router-dom'

export default function Breadcrumbs() {
  const dispatch = useAppDispatch()
  const currentPath = useAppSelector((s) => s.fileSystem.currentPath)
  const [, setSearchParams] = useSearchParams()

  const segments = currentPath === '.' ? [] : currentPath.split('/').filter(Boolean)

  const navigateTo = (path: string) => {
    dispatch(setCurrentPath(path))
    dispatch(listDirThunk(path))
    setSearchParams(path === '.' ? {} : { path })
  }

  return (
    <nav className="flex items-center gap-1 text-sm">
      <button
        onClick={() => navigateTo('.')}
        className="flex items-center gap-1 rounded-md px-2 py-1 text-slate-400 transition-colors hover:bg-slate-100 hover:text-slate-700"
      >
        <Home className="h-4 w-4" />
      </button>
      {segments.map((seg, i) => {
        const path = segments.slice(0, i + 1).join('/')
        const isLast = i === segments.length - 1
        return (
          <span key={path} className="flex items-center gap-1">
            <ChevronRight className="h-3.5 w-3.5 text-slate-300" />
            <button
              onClick={() => navigateTo(path)}
              className={`rounded-md px-2 py-1 transition-colors ${
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
  )
}

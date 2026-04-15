import { useEffect } from 'react'
import { useSearchParams, useLocation } from 'react-router-dom'
import { Home, HardDrive, Trash2, FolderOpen, Star, X } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { fetchBookmarksThunk, removeBookmarkThunk, type Bookmark } from '@/store/slices/bookmarksSlice'
import { listDirThunk, setCurrentPath } from '@/store/slices/fileSystemSlice'
import { listTrashThunk } from '@/store/slices/trashSlice'
import { cn } from '@/lib/utils'

const TRASH_PATH = 'trash:///'

function bookmarkIcon(bookmark: Bookmark) {
  const p = bookmark.path
  const l = bookmark.label.toLowerCase()
  if (p === '.' || l === 'home') return Home
  if (p === '/' || l === 'root') return HardDrive
  if (l === 'trash' || p.includes('trash')) return Trash2
  if (l === 'starred' || l === 'favorites') return Star
  return FolderOpen
}

export default function PlacesPanel() {
  const dispatch = useAppDispatch()
  const [searchParams, setSearchParams] = useSearchParams()
  const location = useLocation()
  const { bookmarks } = useAppSelector((s) => s.bookmarks)

  useEffect(() => {
    dispatch(fetchBookmarksThunk())
  }, [dispatch])

  const navigateTo = (path: string) => {
    dispatch(setCurrentPath(path))
    if (path === TRASH_PATH) {
      dispatch(listTrashThunk())
      setSearchParams({ path: TRASH_PATH })
    } else {
      dispatch(listDirThunk(path))
      setSearchParams(path === '.' ? {} : { path })
    }
  }

  const handleRemove = async (e: React.MouseEvent, path: string) => {
    e.stopPropagation()
    try {
      await dispatch(removeBookmarkThunk(path)).unwrap()
      toast.success('Bookmark removed')
    } catch {
      toast.error('Failed to remove bookmark')
    }
  }

  // Active when on Files page (/) and path search param matches
  const isActive = (bookmark: { path: string }) => {
    if (location.pathname !== '/') return false
    const urlPath = searchParams.get('path') || '.'
    return bookmark.path === urlPath
  }

  // Split into default places (first 2 or built-in) and user bookmarks
  const defaultPaths = new Set(['.', '/'])
  const places = bookmarks.filter((b) => defaultPaths.has(b.path))
  const userBookmarks = bookmarks.filter((b) => !defaultPaths.has(b.path))

  return (
    <div className="flex flex-col gap-1 py-1">
      {places.map((bookmark) => {
        const Icon = bookmarkIcon(bookmark)
        const active = isActive(bookmark)
        return (
          <button
            key={bookmark.path}
            onClick={() => navigateTo(bookmark.path)}
            className={cn(
              'group flex w-full items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-all duration-150',
              active
                ? 'bg-indigo-50 text-indigo-600'
                : 'text-slate-600 hover:bg-slate-50 hover:text-slate-900',
            )}
          >
            <Icon className="h-[16px] w-[16px] shrink-0" />
            <span className="truncate">{bookmark.label}</span>
          </button>
        )
      })}

      {/* Trash — hardcoded pseudo-directory */}
      <button
        onClick={() => navigateTo(TRASH_PATH)}
        className={cn(
          'group flex w-full items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-all duration-150',
          isActive({ path: TRASH_PATH })
            ? 'bg-indigo-50 text-indigo-600'
            : 'text-slate-600 hover:bg-slate-50 hover:text-slate-900',
        )}
      >
        <Trash2 className="h-[16px] w-[16px] shrink-0" />
        <span className="truncate">Trash</span>
      </button>

      {userBookmarks.length > 0 && (
        <>
          <div className="mx-3 my-1.5 border-t border-slate-100" />
          <span className="px-3 text-[11px] font-semibold uppercase tracking-wider text-slate-400">
            Bookmarks
          </span>
          {userBookmarks.map((bookmark) => {
            const Icon = bookmarkIcon(bookmark)
            const active = isActive(bookmark)
            return (
              <button
                key={bookmark.path}
                onClick={() => navigateTo(bookmark.path)}
                className={cn(
                  'group flex w-full items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-all duration-150',
                  active
                    ? 'bg-indigo-50 text-indigo-600'
                    : 'text-slate-600 hover:bg-slate-50 hover:text-slate-900',
                )}
              >
                <Icon className="h-[16px] w-[16px] shrink-0" />
                <span className="flex-1 truncate text-left">{bookmark.label}</span>
                <Button
                  variant="ghost"
                  size="icon-xs"
                  className="hidden h-5 w-5 shrink-0 text-slate-400 hover:text-red-500 group-hover:flex"
                  onClick={(e) => handleRemove(e, bookmark.path)}
                >
                  <X className="h-3 w-3" />
                </Button>
              </button>
            )
          })}
        </>
      )}
    </div>
  )
}

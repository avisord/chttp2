import { useEffect } from 'react'
import { X, Info } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Skeleton } from '@/components/ui/skeleton'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { setPreviewFile, fetchContentThunk, fetchStatThunk } from '@/store/slices/fileSystemSlice'

const TEXT_EXTS = new Set([
  'txt', 'md', 'log', 'csv', 'json', 'js', 'ts', 'tsx', 'jsx', 'py', 'c', 'h',
  'cpp', 'rs', 'go', 'java', 'yaml', 'yml', 'toml', 'xml', 'html', 'css', 'sh',
  'sql', 'env', 'gitignore', 'Makefile', 'Dockerfile',
])

const IMAGE_EXTS = new Set(['jpg', 'jpeg', 'png', 'gif', 'svg', 'webp', 'bmp', 'ico'])

function getExt(name: string): string {
  return name.split('.').pop()?.toLowerCase() || ''
}

export default function FilePreview() {
  const dispatch = useAppDispatch()
  const { previewFile, previewContent, fileStat, currentPath } = useAppSelector((s) => s.fileSystem)

  useEffect(() => {
    if (!previewFile) return
    const fullPath = currentPath === '.' ? previewFile.name : `${currentPath}/${previewFile.name}`
    dispatch(fetchStatThunk(fullPath))
    if (previewFile.type === 'file' && TEXT_EXTS.has(getExt(previewFile.name))) {
      dispatch(fetchContentThunk(fullPath))
    }
  }, [previewFile, currentPath, dispatch])

  if (!previewFile) return null

  const ext = getExt(previewFile.name)
  const fullPath = currentPath === '.' ? previewFile.name : `${currentPath}/${previewFile.name}`
  const isText = TEXT_EXTS.has(ext)
  const isImage = IMAGE_EXTS.has(ext)

  return (
    <div className="flex w-80 flex-col border-l border-slate-200 bg-white">
      <div className="flex items-center justify-between border-b border-slate-100 px-4 py-3">
        <h3 className="truncate text-sm font-semibold text-slate-900">{previewFile.name}</h3>
        <Button variant="ghost" size="icon" className="h-7 w-7 hover:bg-slate-100" onClick={() => dispatch(setPreviewFile(null))}>
          <X className="h-4 w-4 text-slate-400" />
        </Button>
      </div>

      <ScrollArea className="flex-1 p-4">
        {isImage && (
          <div className="mb-4 overflow-hidden rounded-lg border border-slate-100">
            <img
              src={`/fs/download?path=${encodeURIComponent(fullPath)}`}
              alt={previewFile.name}
              className="w-full object-contain"
            />
          </div>
        )}

        {isText && (
          <div className="mb-4">
            {previewContent !== null ? (
              <pre className="max-h-96 overflow-auto rounded-lg bg-slate-50 p-3 font-mono text-xs leading-relaxed text-slate-700">
                {previewContent}
              </pre>
            ) : (
              <Skeleton className="h-32 w-full" />
            )}
          </div>
        )}

        {fileStat && (
          <div className="space-y-2 text-sm">
            <h4 className="flex items-center gap-2 font-semibold text-slate-500">
              <Info className="h-4 w-4" /> Details
            </h4>
            <div className="space-y-0 rounded-lg bg-slate-50 p-3">
              {[
                ['Type', fileStat.type],
                ['Size', `${fileStat.size.toLocaleString()} bytes`],
                ['Permissions', fileStat.mode],
                ['Modified', new Date(fileStat.modified).toLocaleString()],
              ].map(([k, v]) => (
                <div key={k} className="flex justify-between border-b border-slate-100 py-2 last:border-0">
                  <span className="text-slate-500">{k}</span>
                  <span className="font-mono text-xs text-slate-900">{v}</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </ScrollArea>
    </div>
  )
}

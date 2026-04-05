import { useEffect } from 'react'
import { Info } from 'lucide-react'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { setPreviewFile, fetchStatThunk } from '@/store/slices/fileSystemSlice'

export default function FilePreview() {
  const dispatch = useAppDispatch()
  const { previewFile, fileStat, currentPath } = useAppSelector((s) => s.fileSystem)

  useEffect(() => {
    if (!previewFile) return
    const fullPath = currentPath === '.' ? previewFile.name : `${currentPath}/${previewFile.name}`
    dispatch(fetchStatThunk(fullPath))
  }, [previewFile, currentPath, dispatch])

  const fullPath = previewFile
    ? currentPath === '.' ? previewFile.name : `${currentPath}/${previewFile.name}`
    : '-'

  return (
    <Dialog open={previewFile !== null} onOpenChange={(open) => !open && dispatch(setPreviewFile(null))}>
      <DialogContent className="max-w-lg p-0">
        <DialogHeader className="border-b border-slate-100 px-5 py-4">
          <DialogTitle className="truncate text-base font-semibold text-slate-900">
            Properties
          </DialogTitle>
          <DialogDescription className="truncate text-sm text-slate-500">
            {previewFile?.name ?? 'Selected item'}
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-4 px-5 py-4">
          <h4 className="flex items-center gap-2 text-sm font-semibold text-slate-600">
            <Info className="h-4 w-4" /> Details
          </h4>
          <div className="space-y-0 rounded-xl border border-slate-100 bg-slate-50">
            {[
              ['Name', previewFile?.name ?? '-'],
              ['Path', fullPath],
              ['Type', fileStat?.type ?? previewFile?.type ?? '-'],
              ['MIME', fileStat?.mime ?? previewFile?.mime ?? '-'],
              ['Size', fileStat ? `${fileStat.size.toLocaleString()} bytes` : '-'],
              ['Permissions', fileStat?.mode ?? '-'],
              ['Modified', fileStat ? new Date(fileStat.modified).toLocaleString() : '-'],
            ].map(([k, v]) => (
              <div key={k} className="flex items-start justify-between gap-4 border-b border-slate-100 px-4 py-3 last:border-0">
                <span className="min-w-24 text-sm text-slate-500">{k}</span>
                <span className="text-right font-mono text-xs break-all text-slate-900">{v}</span>
              </div>
            ))}
          </div>
        </div>
      </DialogContent>
    </Dialog>
  )
}

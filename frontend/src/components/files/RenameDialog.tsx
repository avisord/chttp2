import { useState, useEffect } from 'react'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogFooter,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { renameFileThunk } from '@/store/slices/fileSystemSlice'
import { toast } from 'sonner'

interface RenameDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  currentName: string
}

export default function RenameDialog({ open, onOpenChange, currentName }: RenameDialogProps) {
  const dispatch = useAppDispatch()
  const currentPath = useAppSelector((s) => s.fileSystem.currentPath)
  const [name, setName] = useState(currentName)

  useEffect(() => {
    setName(currentName)
  }, [currentName])

  const handleRename = async () => {
    if (!name.trim() || name.trim() === currentName) return
    const fullPath = currentPath === '.' ? currentName : `${currentPath}/${currentName}`
    try {
      await dispatch(renameFileThunk({ path: fullPath, name: name.trim() })).unwrap()
      toast.success(`Renamed to "${name.trim()}"`)
      onOpenChange(false)
    } catch (err: unknown) {
      toast.error(`Rename failed: ${err instanceof Error ? err.message : 'Unknown error'}`)
    }
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle className="text-slate-900">Rename</DialogTitle>
        </DialogHeader>
        <div>
          <label className="mb-1.5 block text-sm font-semibold text-slate-700">New name</label>
          <Input
            value={name}
            onChange={(e) => setName(e.target.value)}
            autoFocus
            onKeyDown={(e) => e.key === 'Enter' && handleRename()}
            className="border-slate-200 focus-visible:ring-2 focus-visible:ring-indigo-500 focus-visible:ring-offset-1"
          />
        </div>
        <DialogFooter>
          <Button variant="outline" onClick={() => onOpenChange(false)} className="border-slate-200 text-slate-700 hover:bg-slate-50">
            Cancel
          </Button>
          <Button
            onClick={handleRename}
            disabled={!name.trim() || name.trim() === currentName}
            className="rounded-lg bg-gradient-to-r from-indigo-600 to-violet-600 text-white shadow-[0_4px_14px_0_rgba(79,70,229,0.3)] transition-all duration-200 hover:-translate-y-0.5 hover:shadow-[0_6px_20px_0_rgba(79,70,229,0.4)]"
          >
            Rename
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}

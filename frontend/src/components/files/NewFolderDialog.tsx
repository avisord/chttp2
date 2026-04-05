import { useState } from 'react'
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
import { createDirThunk } from '@/store/slices/fileSystemSlice'
import { toast } from 'sonner'

interface NewFolderDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
}

export default function NewFolderDialog({ open, onOpenChange }: NewFolderDialogProps) {
  const dispatch = useAppDispatch()
  const currentPath = useAppSelector((s) => s.fileSystem.currentPath)
  const [name, setName] = useState('')

  const handleCreate = async () => {
    if (!name.trim()) return
    const fullPath = currentPath === '.' ? name.trim() : `${currentPath}/${name.trim()}`
    try {
      await dispatch(createDirThunk(fullPath)).unwrap()
      toast.success(`Created folder "${name.trim()}"`)
      setName('')
      onOpenChange(false)
    } catch (err: unknown) {
      toast.error(`Failed to create folder: ${err instanceof Error ? err.message : 'Unknown error'}`)
    }
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle className="text-slate-900">New Folder</DialogTitle>
        </DialogHeader>
        <div>
          <label className="mb-1.5 block text-sm font-semibold text-slate-700">Folder name</label>
          <Input
            value={name}
            onChange={(e) => setName(e.target.value)}
            placeholder="My folder"
            autoFocus
            onKeyDown={(e) => e.key === 'Enter' && handleCreate()}
            className="border-slate-200 focus-visible:ring-2 focus-visible:ring-indigo-500 focus-visible:ring-offset-1"
          />
        </div>
        <DialogFooter>
          <Button variant="outline" onClick={() => onOpenChange(false)} className="border-slate-200 text-slate-700 hover:bg-slate-50">
            Cancel
          </Button>
          <Button
            onClick={handleCreate}
            disabled={!name.trim()}
            className="rounded-lg bg-gradient-to-r from-indigo-600 to-violet-600 text-white shadow-[0_4px_14px_0_rgba(79,70,229,0.3)] transition-all duration-200 hover:-translate-y-0.5 hover:shadow-[0_6px_20px_0_rgba(79,70,229,0.4)]"
          >
            Create
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}

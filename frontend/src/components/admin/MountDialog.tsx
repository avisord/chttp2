import { useState, useEffect } from 'react'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
  DialogFooter,
  DialogClose,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'

interface MountDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  device: string
  onSubmit: (data: { device: string; mountpoint: string; fstype?: string }) => void
}

export default function MountDialog({ open, onOpenChange, device, onSubmit }: MountDialogProps) {
  const [mountpoint, setMountpoint] = useState('')
  const [fstype, setFstype] = useState('')

  useEffect(() => {
    if (open) {
      setMountpoint('')
      setFstype('')
    }
  }, [open])

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    onSubmit({ device, mountpoint, ...(fstype ? { fstype } : {}) })
    onOpenChange(false)
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-md">
        <form onSubmit={handleSubmit}>
          <DialogHeader>
            <DialogTitle>Mount Device</DialogTitle>
            <DialogDescription>
              Mount <span className="font-mono font-medium text-slate-700">{device}</span> to a directory
            </DialogDescription>
          </DialogHeader>
          <div className="space-y-4 py-4">
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">Device</label>
              <Input value={device} disabled className="bg-slate-50 font-mono" />
            </div>
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">Mount Point</label>
              <Input
                value={mountpoint}
                onChange={(e) => setMountpoint(e.target.value)}
                placeholder="/mnt/data"
                required
                autoFocus
              />
            </div>
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">
                Filesystem Type <span className="text-slate-400">(optional, auto-detect)</span>
              </label>
              <Input
                value={fstype}
                onChange={(e) => setFstype(e.target.value)}
                placeholder="ext4, ntfs, btrfs..."
              />
            </div>
          </div>
          <DialogFooter>
            <DialogClose render={<Button variant="outline" />}>Cancel</DialogClose>
            <Button type="submit">Mount</Button>
          </DialogFooter>
        </form>
      </DialogContent>
    </Dialog>
  )
}

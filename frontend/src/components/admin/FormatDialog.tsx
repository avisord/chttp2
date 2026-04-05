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

interface FormatDialogProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  device: string
  onSubmit: (data: { device: string; fstype: string }) => void
}

export default function FormatDialog({ open, onOpenChange, device, onSubmit }: FormatDialogProps) {
  const [fstype, setFstype] = useState('ext4')

  useEffect(() => {
    if (open) setFstype('ext4')
  }, [open])

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    onSubmit({ device, fstype })
    onOpenChange(false)
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-md">
        <form onSubmit={handleSubmit}>
          <DialogHeader>
            <DialogTitle>Format Device</DialogTitle>
            <DialogDescription>
              This will erase all data on <span className="font-mono font-medium text-slate-700">{device}</span>. This action cannot be undone.
            </DialogDescription>
          </DialogHeader>
          <div className="space-y-4 py-4">
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">Device</label>
              <Input value={device} disabled className="bg-slate-50 font-mono" />
            </div>
            <div>
              <label className="mb-1.5 block text-sm font-medium text-slate-700">Filesystem Type</label>
              <Input
                value={fstype}
                onChange={(e) => setFstype(e.target.value)}
                placeholder="ext4"
                required
                autoFocus
              />
            </div>
          </div>
          <DialogFooter>
            <DialogClose render={<Button variant="outline" />}>Cancel</DialogClose>
            <Button type="submit" variant="destructive">Format</Button>
          </DialogFooter>
        </form>
      </DialogContent>
    </Dialog>
  )
}

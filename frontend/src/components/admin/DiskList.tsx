import { useEffect, useState, useRef } from 'react'
import { RefreshCw } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { Skeleton } from '@/components/ui/skeleton'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { fetchDisksThunk, mountDiskThunk, unmountDiskThunk, formatDiskThunk } from '@/store/slices/disksSlice'
import DiskCard from './DiskCard'
import MountDialog from './MountDialog'
import FormatDialog from './FormatDialog'
import ConfirmDialog from './ConfirmDialog'

export default function DiskList() {
  const dispatch = useAppDispatch()
  const { disks, status } = useAppSelector((s) => s.disks)
  const [mountDialogOpen, setMountDialogOpen] = useState(false)
  const [mountDevice, setMountDevice] = useState('')
  const [formatDialogOpen, setFormatDialogOpen] = useState(false)
  const [formatDevice, setFormatDevice] = useState('')
  const [unmountConfirmOpen, setUnmountConfirmOpen] = useState(false)
  const [unmountTarget, setUnmountTarget] = useState('')
  const [mountConfirmOpen, setMountConfirmOpen] = useState(false)
  const pendingMountData = useRef<{ device: string; mountpoint: string; fstype?: string } | null>(null)
  const [formatConfirmOpen, setFormatConfirmOpen] = useState(false)
  const pendingFormatData = useRef<{ device: string; fstype: string } | null>(null)

  useEffect(() => {
    dispatch(fetchDisksThunk())
  }, [dispatch])

  const handleMount = (device: string) => {
    setMountDevice(device)
    setMountDialogOpen(true)
  }

  const handleMountSubmit = (data: { device: string; mountpoint: string; fstype?: string }) => {
    pendingMountData.current = data
    setMountConfirmOpen(true)
  }

  const confirmMount = async () => {
    if (!pendingMountData.current) return
    const data = pendingMountData.current
    try {
      await dispatch(mountDiskThunk(data)).unwrap()
      toast.success(`Mounted ${data.device} at ${data.mountpoint}`)
    } catch {
      toast.error('Failed to mount device')
    }
    pendingMountData.current = null
  }

  const handleUnmount = (mountpoint: string) => {
    setUnmountTarget(mountpoint)
    setUnmountConfirmOpen(true)
  }

  const confirmUnmount = async () => {
    if (!unmountTarget) return
    try {
      await dispatch(unmountDiskThunk({ mountpoint: unmountTarget })).unwrap()
      toast.success(`Unmounted ${unmountTarget}`)
    } catch {
      toast.error('Failed to unmount device')
    }
  }

  const handleFormat = (device: string) => {
    setFormatDevice(device)
    setFormatDialogOpen(true)
  }

  const handleFormatSubmit = (data: { device: string; fstype: string }) => {
    pendingFormatData.current = data
    setFormatConfirmOpen(true)
  }

  const confirmFormat = async () => {
    if (!pendingFormatData.current) return
    const data = pendingFormatData.current
    try {
      await dispatch(formatDiskThunk(data)).unwrap()
      toast.success(`Formatted ${data.device} as ${data.fstype}`)
    } catch {
      toast.error('Failed to format device')
    }
    pendingFormatData.current = null
  }

  if (status === 'loading' && !disks.length) {
    return (
      <div className="space-y-3">
        {Array.from({ length: 3 }).map((_, i) => <Skeleton key={i} className="h-20 rounded-xl" />)}
      </div>
    )
  }

  return (
    <div>
      <div className="mb-4 flex justify-end">
        <Button variant="outline" size="sm" onClick={() => dispatch(fetchDisksThunk())}>
          <RefreshCw className="mr-2 h-4 w-4" />
          Refresh
        </Button>
      </div>

      {!disks.length ? (
        <p className="text-sm text-muted-foreground">No block devices found</p>
      ) : (
        <div className="space-y-2">
          {disks.map((disk) => (
            <DiskCard
              key={disk.name}
              disk={disk}
              onMount={handleMount}
              onUnmount={handleUnmount}
              onFormat={handleFormat}
            />
          ))}
        </div>
      )}

      <MountDialog
        open={mountDialogOpen}
        onOpenChange={setMountDialogOpen}
        device={mountDevice}
        onSubmit={handleMountSubmit}
      />
      <FormatDialog
        open={formatDialogOpen}
        onOpenChange={setFormatDialogOpen}
        device={formatDevice}
        onSubmit={handleFormatSubmit}
      />

      <ConfirmDialog
        open={unmountConfirmOpen}
        onOpenChange={setUnmountConfirmOpen}
        title={`Unmount ${unmountTarget}?`}
        description="Any processes using this mountpoint may be interrupted. Make sure no files are open on this device before unmounting."
        confirmLabel="Unmount"
        destructive
        onConfirm={confirmUnmount}
      />

      <ConfirmDialog
        open={mountConfirmOpen}
        onOpenChange={setMountConfirmOpen}
        title={`Mount ${pendingMountData.current?.device ?? ''}?`}
        description={`This will mount the device at ${pendingMountData.current?.mountpoint ?? ''}. Ensure the mount point exists and is empty.`}
        confirmLabel="Mount"
        onConfirm={confirmMount}
      />

      <ConfirmDialog
        open={formatConfirmOpen}
        onOpenChange={setFormatConfirmOpen}
        title={`Format ${pendingFormatData.current?.device ?? ''}?`}
        description={`This will ERASE ALL DATA on ${pendingFormatData.current?.device ?? ''} and create a new ${pendingFormatData.current?.fstype ?? ''} filesystem. This action cannot be undone.`}
        confirmLabel="Format"
        destructive
        onConfirm={confirmFormat}
      />
    </div>
  )
}

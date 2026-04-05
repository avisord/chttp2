import { useEffect, useState } from 'react'
import { RefreshCw } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { Skeleton } from '@/components/ui/skeleton'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { fetchDisksThunk, mountDiskThunk, unmountDiskThunk } from '@/store/slices/disksSlice'
import DiskCard from './DiskCard'
import MountDialog from './MountDialog'

export default function DiskList() {
  const dispatch = useAppDispatch()
  const { disks, status } = useAppSelector((s) => s.disks)
  const [mountDialogOpen, setMountDialogOpen] = useState(false)
  const [mountDevice, setMountDevice] = useState('')

  useEffect(() => {
    dispatch(fetchDisksThunk())
  }, [dispatch])

  const handleMount = (device: string) => {
    setMountDevice(device)
    setMountDialogOpen(true)
  }

  const handleMountSubmit = async (data: { device: string; mountpoint: string; fstype?: string }) => {
    try {
      await dispatch(mountDiskThunk(data)).unwrap()
      toast.success(`Mounted ${data.device} at ${data.mountpoint}`)
    } catch {
      toast.error('Failed to mount device')
    }
  }

  const handleUnmount = async (mountpoint: string) => {
    try {
      await dispatch(unmountDiskThunk({ mountpoint })).unwrap()
      toast.success(`Unmounted ${mountpoint}`)
    } catch {
      toast.error('Failed to unmount device')
    }
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
    </div>
  )
}

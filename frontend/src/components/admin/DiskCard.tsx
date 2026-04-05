import { HardDrive, Disc, Unplug, Eraser } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import type { DiskInfo } from '@/types/api'

function formatSize(bytes: number): string {
  if (bytes === 0) return '0 B'
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(1024))
  return `${(bytes / Math.pow(1024, i)).toFixed(i > 0 ? 1 : 0)} ${units[i]}`
}

interface DiskCardProps {
  disk: DiskInfo
  onMount: (device: string) => void
  onUnmount: (mountpoint: string) => void
  onFormat: (device: string) => void
}

export default function DiskCard({ disk, onMount, onUnmount, onFormat }: DiskCardProps) {
  const isMounted = !!disk.mountpoint
  const isDisk = disk.type === 'disk'
  const isLoop = disk.type === 'loop'
  const hasFs = !!disk.fstype
  const hasChildren = !!disk.children?.length
  const isBlank = isDisk && !hasFs && !hasChildren
  const isMountable = disk.type === 'part' || (isDisk && hasFs && !hasChildren)
  const Icon = isDisk ? HardDrive : Disc

  return (
    <div className="rounded-xl border border-slate-100 bg-white shadow-[0_4px_20px_-2px_rgba(79,70,229,0.04)] transition-all duration-200 hover:shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)]">
      <div className="flex items-center gap-3 p-3.5">
        <div className={`flex h-9 w-9 items-center justify-center rounded-lg ${isDisk ? 'bg-indigo-50' : 'bg-slate-50'}`}>
          <Icon className={`h-4.5 w-4.5 ${isDisk ? 'text-indigo-600' : 'text-slate-500'}`} />
        </div>
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2">
            <span className="text-sm font-semibold text-slate-900">/dev/{disk.name}</span>
            <Badge variant="outline" className="text-xs">
              {disk.type}
            </Badge>
            {isMounted && (
              <Badge variant="outline" className="gap-1 border-emerald-200 bg-emerald-50 text-emerald-700 text-xs">
                mounted
              </Badge>
            )}
            {isBlank && (
              <Badge variant="outline" className="gap-1 border-amber-200 bg-amber-50 text-amber-700 text-xs">
                unformatted
              </Badge>
            )}
          </div>
          <div className="flex items-center gap-3 text-xs text-slate-400">
            <span>{formatSize(disk.size)}</span>
            {disk.fstype && <span>{disk.fstype}</span>}
            {disk.label && <span>{disk.label}</span>}
            {disk.model && <span className="truncate">{disk.model}</span>}
            {disk.mountpoint && <span className="font-mono text-slate-500">{disk.mountpoint}</span>}
          </div>
        </div>
        <div className="flex gap-1">
          {isBlank && (
            <Button
              variant="ghost"
              size="sm"
              className="text-slate-500 hover:bg-amber-50 hover:text-amber-600"
              onClick={() => onFormat(`/dev/${disk.name}`)}
            >
              <Eraser className="mr-1.5 h-3.5 w-3.5" />
              Format
            </Button>
          )}
          {isMountable && (
            isMounted ? (
              <Button
                variant="ghost"
                size="sm"
                className="text-slate-500 hover:bg-red-50 hover:text-red-600"
                onClick={() => onUnmount(disk.mountpoint!)}
              >
                <Unplug className="mr-1.5 h-3.5 w-3.5" />
                Unmount
              </Button>
            ) : (
              <Button
                variant="ghost"
                size="sm"
                className="text-slate-500 hover:bg-indigo-50 hover:text-indigo-600"
                onClick={() => onMount(`/dev/${disk.name}`)}
              >
                <Disc className="mr-1.5 h-3.5 w-3.5" />
                Mount
              </Button>
            )
          )}
          {!isBlank && !isMountable && !isMounted && !isLoop && !hasChildren && (
            <Button
              variant="ghost"
              size="sm"
              className="text-slate-500 hover:bg-amber-50 hover:text-amber-600"
              onClick={() => onFormat(`/dev/${disk.name}`)}
            >
              <Eraser className="mr-1.5 h-3.5 w-3.5" />
              Format
            </Button>
          )}
        </div>
      </div>

      {/* Partitions / children */}
      {disk.children && disk.children.length > 0 && (
        <div className="border-t border-slate-50 pl-8">
          {disk.children.map((child) => (
            <DiskCard key={child.name} disk={child} onMount={onMount} onUnmount={onUnmount} onFormat={onFormat} />
          ))}
        </div>
      )}
    </div>
  )
}

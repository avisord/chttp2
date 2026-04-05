import { cn } from '@/lib/utils'
import { resolveFileIcon } from './fileIcons'

interface FileIconProps {
  name: string
  type: 'file' | 'dir'
  mime?: string
  className?: string
}

export default function FileIcon({ name, type, mime, className }: FileIconProps) {
  const spec = resolveFileIcon({ name, type, mime })

  return (
    <img
      src={spec.src}
      alt=""
      aria-hidden="true"
      className={cn('h-5 w-5 shrink-0 object-contain', className)}
    />
  )
}

import {
  File,
  FileText,
  FileImage,
  FileVideo2 as FileVideo,
  FileAudio,
  FileCode2 as FileCode,
  FileArchive,
  Folder,
} from 'lucide-react'
import { cn } from '@/lib/utils'

const EXT_MAP: Record<string, typeof File> = {
  // Text
  txt: FileText, md: FileText, log: FileText, csv: FileText,
  // Images
  jpg: FileImage, jpeg: FileImage, png: FileImage, gif: FileImage, svg: FileImage, webp: FileImage, bmp: FileImage, ico: FileImage,
  // Video
  mp4: FileVideo, mkv: FileVideo, avi: FileVideo, mov: FileVideo, webm: FileVideo,
  // Audio
  mp3: FileAudio, wav: FileAudio, ogg: FileAudio, flac: FileAudio,
  // Code
  js: FileCode, ts: FileCode, tsx: FileCode, jsx: FileCode, py: FileCode, c: FileCode, h: FileCode,
  cpp: FileCode, rs: FileCode, go: FileCode, java: FileCode, json: FileCode, yaml: FileCode, yml: FileCode,
  toml: FileCode, xml: FileCode, html: FileCode, css: FileCode, sh: FileCode,
  // Archives
  zip: FileArchive, tar: FileArchive, gz: FileArchive, bz2: FileArchive, xz: FileArchive, '7z': FileArchive, rar: FileArchive,
}

interface FileIconProps {
  name: string
  type: 'file' | 'dir'
  className?: string
}

export default function FileIcon({ name, type, className }: FileIconProps) {
  if (type === 'dir') {
    return <Folder className={cn('h-5 w-5 text-indigo-600', className)} />
  }

  const ext = name.split('.').pop()?.toLowerCase() || ''
  const Icon = EXT_MAP[ext] || File

  return <Icon className={cn('h-5 w-5 text-slate-400', className)} />
}

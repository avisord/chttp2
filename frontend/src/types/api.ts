export interface WhoamiResponse {
  uid: number
  gid: number
  username: string
  home: string
  shell: string
  cwd: string
}

export interface FileEntry {
  name: string
  type: 'file' | 'dir'
  size: number
  modified: string
  mime: string
}

export interface DirListResponse {
  path: string
  entries: FileEntry[]
}

export interface FileStat {
  name: string
  path: string
  type: 'file' | 'dir'
  size: number
  mode: string
  uid: number
  modified: string
  mime: string
}

export interface FileContentResponse {
  path: string
  content: string
}

export interface SessionInfo {
  session_id: string
  username: string
  expiry_time: number
  last_access_time: number
}

export interface UploadSessionCreateRequest {
  dest: string
  filename: string
  file_size: number
  chunk_size: number
  chunk_count: number
  chunk_hashes: string[]
}

export interface ApiError {
  error: string
  errno?: number
}

export interface SystemUser {
  username: string
  uid: number
  gid: number
  home: string
  shell: string
  groups: string[]
}

export interface DiskInfo {
  name: string
  size: number
  type: string
  fstype: string | null
  mountpoint: string | null
  label: string | null
  model: string | null
  children?: DiskInfo[]
}

import { apiRequest, apiRequestRaw } from './client'
import type { DirListResponse, FileStat, FileContentResponse } from '@/types/api'

export function listDir(path: string) {
  return apiRequest<DirListResponse>(`/fs/list?path=${encodeURIComponent(path)}`)
}

export function createDir(path: string) {
  return apiRequest<{ message: string }>('/fs/mkdir', {
    method: 'POST',
    body: JSON.stringify({ path }),
  })
}

export function deleteFile(path: string) {
  return apiRequest<{ message: string }>(`/fs/file?path=${encodeURIComponent(path)}`, {
    method: 'DELETE',
  })
}

export function deleteDir(path: string) {
  return apiRequest<{ message: string }>(`/fs/dir?path=${encodeURIComponent(path)}`, {
    method: 'DELETE',
  })
}

export function renameFile(path: string, name: string) {
  return apiRequest<{ message: string }>('/fs/rename', {
    method: 'POST',
    body: JSON.stringify({ path, name }),
  })
}

export function moveFile(from: string, to: string) {
  return apiRequest<{ message: string }>('/fs/move', {
    method: 'POST',
    body: JSON.stringify({ from, to }),
  })
}

export function copyFile(from: string, to: string) {
  return apiRequest<{ message: string }>('/fs/copy', {
    method: 'POST',
    body: JSON.stringify({ from, to }),
  })
}

export function fetchStat(path: string) {
  return apiRequest<FileStat>(`/fs/stat?path=${encodeURIComponent(path)}`)
}

export function readContent(path: string) {
  return apiRequest<FileContentResponse>(`/fs/content?path=${encodeURIComponent(path)}`)
}

export function writeContent(path: string, content: string) {
  return apiRequest<{ message: string; path: string; size: number }>(
    `/fs/content?path=${encodeURIComponent(path)}`,
    {
      method: 'PUT',
      body: content,
      headers: { 'Content-Type': 'text/plain' },
    },
  )
}

export function simpleUpload(dirPath: string, file: File, onProgress?: (pct: number) => void): Promise<{ message: string; path: string; size: number }> {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest()
    xhr.open('POST', `/fs/upload?path=${encodeURIComponent(dirPath)}`)
    xhr.withCredentials = true

    if (onProgress) {
      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) onProgress((e.loaded / e.total) * 100)
      })
    }

    xhr.addEventListener('load', () => {
      if (xhr.status >= 200 && xhr.status < 300) {
        resolve(JSON.parse(xhr.responseText))
      } else {
        reject(new Error(xhr.responseText || xhr.statusText))
      }
    })
    xhr.addEventListener('error', () => reject(new Error('Upload failed')))

    const fd = new FormData()
    fd.append('file', file)
    xhr.send(fd)
  })
}

export async function downloadFile(path: string, filename: string) {
  const res = await apiRequestRaw(`/fs/download?path=${encodeURIComponent(path)}`)
  const blob = await res.blob()
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  document.body.appendChild(a)
  a.click()
  a.remove()
  URL.revokeObjectURL(url)
}

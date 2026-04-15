import { apiRequest } from './client'

export interface TrashEntry {
  name: string
  original_path: string
  deleted_at: string
  type: 'file' | 'dir'
  size: number
}

export function listTrash() {
  return apiRequest<TrashEntry[]>('/trash/list')
}

export function restoreTrashItem(name: string) {
  return apiRequest<{ message: string }>('/trash/restore', {
    method: 'POST',
    body: JSON.stringify({ name }),
  })
}

export function deleteTrashItem(name: string) {
  return apiRequest<{ message: string }>(`/trash/${encodeURIComponent(name)}`, {
    method: 'DELETE',
  })
}

export function emptyTrash() {
  return apiRequest<{ message: string }>('/trash', {
    method: 'DELETE',
  })
}

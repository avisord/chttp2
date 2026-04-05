import { apiRequest } from './client'
import type { SystemUser } from '@/types/api'

/* User management */
export function getUsers() {
  return apiRequest<SystemUser[]>('/admin/users')
}

export function createUser(data: { username: string; password: string; shell?: string }) {
  return apiRequest<{ message: string }>('/admin/users', {
    method: 'POST',
    body: JSON.stringify(data),
  })
}

export function editUser(username: string, data: { password?: string; shell?: string; groups?: string }) {
  return apiRequest<{ message: string }>(`/admin/users/${encodeURIComponent(username)}`, {
    method: 'PUT',
    body: JSON.stringify(data),
  })
}

export function deleteUser(username: string) {
  return apiRequest<{ message: string }>(`/admin/users/${encodeURIComponent(username)}`, {
    method: 'DELETE',
  })
}

/* Disk management */
export interface LsblkResponse {
  blockdevices: import('@/types/api').DiskInfo[]
}

export function getDisks() {
  return apiRequest<LsblkResponse>('/admin/disks')
}

export function mountDisk(data: { device: string; mountpoint: string; fstype?: string }) {
  return apiRequest<{ message: string }>('/admin/disks/mount', {
    method: 'POST',
    body: JSON.stringify(data),
  })
}

export function unmountDisk(data: { mountpoint: string }) {
  return apiRequest<{ message: string }>('/admin/disks/unmount', {
    method: 'POST',
    body: JSON.stringify(data),
  })
}

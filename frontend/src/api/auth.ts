import { apiRequest } from './client'
import type { WhoamiResponse } from '@/types/api'

export function login(username: string, password: string) {
  return apiRequest<{ message: string }>('/login', {
    method: 'POST',
    body: JSON.stringify({ username, password }),
  })
}

export function whoami() {
  return apiRequest<WhoamiResponse>('/whoami')
}

export function logout() {
  return apiRequest<{ message: string }>('/logout', {
    method: 'DELETE',
  })
}

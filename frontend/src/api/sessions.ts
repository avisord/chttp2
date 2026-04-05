import { apiRequest } from './client'
import type { SessionInfo } from '@/types/api'

export function getSessions() {
  return apiRequest<SessionInfo[]>('/sessions')
}

export function deleteSession(sessionId: string) {
  return apiRequest<{ message: string }>(`/sessions/${sessionId}`, {
    method: 'DELETE',
  })
}

export function switchSession(sessionId: string) {
  return apiRequest<{ message: string }>(`/sessions/switch/${sessionId}`, {
    method: 'POST',
  })
}

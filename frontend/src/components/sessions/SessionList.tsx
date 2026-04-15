import { useEffect, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Loader2, Plus, X } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Skeleton } from '@/components/ui/skeleton'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { fetchSessionsThunk, deleteSessionThunk, switchSessionThunk, addSessionThunk } from '@/store/slices/sessionsSlice'
import SessionCard from './SessionCard'

export default function SessionList() {
  const dispatch = useAppDispatch()
  const navigate = useNavigate()
  const { sessions, status } = useAppSelector((s) => s.sessions)
  const user = useAppSelector((s) => s.auth.user)

  const [showForm, setShowForm] = useState(false)
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [adding, setAdding] = useState(false)
  const [addError, setAddError] = useState<string | null>(null)

  useEffect(() => {
    dispatch(fetchSessionsThunk())
  }, [dispatch])

  const handleSwitch = async (sessionId: string) => {
    try {
      await dispatch(switchSessionThunk(sessionId)).unwrap()
      toast.success('Session switched')
      navigate('/')
    } catch {
      toast.error('Failed to switch session')
    }
  }

  const handleDelete = async (sessionId: string) => {
    try {
      await dispatch(deleteSessionThunk(sessionId)).unwrap()
      toast.success('Session deleted')
    } catch {
      toast.error('Failed to delete session')
    }
  }

  const handleAddSession = async (e: React.FormEvent) => {
    e.preventDefault()
    setAdding(true)
    setAddError(null)
    try {
      await dispatch(addSessionThunk({ username, password })).unwrap()
      toast.success(`Signed in as ${username}`)
      setUsername('')
      setPassword('')
      setShowForm(false)
    } catch (err) {
      setAddError(err instanceof Error ? err.message : 'Login failed')
    } finally {
      setAdding(false)
    }
  }

  if (status === 'loading' && !sessions.length) {
    return (
      <div className="space-y-3">
        {Array.from({ length: 2 }).map((_, i) => <Skeleton key={i} className="h-16 rounded-lg" />)}
      </div>
    )
  }

  return (
    <div className="space-y-3">
      {sessions.length ? (
        <div className="space-y-2">
          {sessions.map((session) => (
            <SessionCard
              key={session.session_id}
              session={session}
              isActive={session.username === user?.username}
              onSwitch={() => handleSwitch(session.session_id)}
              onDelete={() => handleDelete(session.session_id)}
            />
          ))}
        </div>
      ) : (
        <p className="text-sm text-muted-foreground">No active sessions</p>
      )}

      {showForm ? (
        <form onSubmit={handleAddSession} className="rounded-xl border border-slate-200 bg-slate-50 p-4 space-y-3 dark:border-slate-700 dark:bg-white/5">
          <div className="flex items-center justify-between">
            <span className="text-sm font-semibold text-slate-700 dark:text-slate-200">Add Session</span>
            <Button
              type="button"
              variant="ghost"
              size="icon-sm"
              onClick={() => { setShowForm(false); setAddError(null) }}
            >
              <X className="h-4 w-4" />
            </Button>
          </div>
          <div>
            <Input
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              placeholder="Username"
              autoComplete="username"
              autoFocus
            />
          </div>
          <div>
            <Input
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="Password"
              autoComplete="current-password"
            />
          </div>
          {addError && (
            <div className="rounded-lg bg-red-50 px-3 py-2 text-sm text-red-600 dark:bg-red-500/10 dark:text-red-400">{addError}</div>
          )}
          <Button type="submit" size="sm" disabled={adding || !username || !password} className="w-full">
            {adding ? <Loader2 className="mr-2 h-4 w-4 animate-spin" /> : null}
            {adding ? 'Signing in...' : 'Sign In'}
          </Button>
        </form>
      ) : (
        <Button variant="outline" size="sm" className="w-full" onClick={() => setShowForm(true)}>
          <Plus className="mr-2 h-4 w-4" />
          Add Session
        </Button>
      )}
    </div>
  )
}

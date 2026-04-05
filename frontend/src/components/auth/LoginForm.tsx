import { useState, type FormEvent } from 'react'
import { useNavigate } from 'react-router-dom'
import { Loader2 } from 'lucide-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { useAppDispatch, useAppSelector } from '@/store/hooks'
import { loginThunk } from '@/store/slices/authSlice'

export default function LoginForm() {
  const dispatch = useAppDispatch()
  const navigate = useNavigate()
  const { status, loginError } = useAppSelector((s) => s.auth)
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')

  const loading = status === 'loading'

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault()
    const result = await dispatch(loginThunk({ username, password }))
    if (loginThunk.fulfilled.match(result)) {
      navigate('/')
    }
  }

  return (
    <form onSubmit={handleSubmit} className="space-y-5">
      <div>
        <label htmlFor="username" className="mb-1.5 block text-sm font-semibold text-slate-700">
          Username
        </label>
        <Input
          id="username"
          value={username}
          onChange={(e) => setUsername(e.target.value)}
          placeholder="Enter username"
          autoComplete="username"
          autoFocus
          className="border-slate-200 bg-white focus-visible:ring-2 focus-visible:ring-indigo-500 focus-visible:ring-offset-1"
        />
      </div>
      <div>
        <label htmlFor="password" className="mb-1.5 block text-sm font-semibold text-slate-700">
          Password
        </label>
        <Input
          id="password"
          type="password"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          placeholder="Enter password"
          autoComplete="current-password"
          className="border-slate-200 bg-white focus-visible:ring-2 focus-visible:ring-indigo-500 focus-visible:ring-offset-1"
        />
      </div>

      {loginError && (
        <div className="rounded-lg bg-red-50 px-3 py-2 text-sm text-red-600">
          {loginError}
        </div>
      )}

      <Button
        type="submit"
        disabled={loading || !username || !password}
        className="w-full rounded-full bg-gradient-to-r from-indigo-600 to-violet-600 py-2.5 font-semibold text-white shadow-[0_4px_14px_0_rgba(79,70,229,0.3)] transition-all duration-200 hover:-translate-y-0.5 hover:from-indigo-500 hover:to-violet-500 hover:shadow-[0_6px_20px_0_rgba(79,70,229,0.4)] disabled:opacity-60 disabled:hover:translate-y-0"
      >
        {loading ? <Loader2 className="mr-2 h-4 w-4 animate-spin" /> : null}
        {loading ? 'Signing in...' : 'Sign In'}
      </Button>
    </form>
  )
}

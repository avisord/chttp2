import { useEffect } from 'react'
import { useNavigate } from 'react-router-dom'
import LoginForm from '@/components/auth/LoginForm'
import SessionList from '@/components/sessions/SessionList'
import { useAppSelector, useAppDispatch } from '@/store/hooks'
import { whoamiThunk } from '@/store/slices/authSlice'

export default function LoginPage() {
  const navigate = useNavigate()
  const dispatch = useAppDispatch()
  const authStatus = useAppSelector((s) => s.auth.status)

  useEffect(() => {
    if (authStatus === 'idle') {
      dispatch(whoamiThunk()).then((res) => {
        if (whoamiThunk.fulfilled.match(res)) navigate('/')
      })
    } else if (authStatus === 'authenticated') {
      navigate('/')
    }
  }, [authStatus, dispatch, navigate])

  return (
    <div className="relative flex min-h-screen items-center justify-center bg-slate-50 p-5 dark:bg-slate-950">
      {/* Atmospheric blur orbs */}
      <div className="pointer-events-none fixed inset-0 overflow-hidden">
        <div className="absolute -left-48 -top-48 h-[500px] w-[500px] rounded-full bg-indigo-200/40 blur-3xl dark:bg-indigo-900/20" />
        <div className="absolute -bottom-48 -right-48 h-[500px] w-[500px] rounded-full bg-violet-200/40 blur-3xl dark:bg-violet-900/20" />
        <div className="absolute left-1/2 top-1/3 h-[300px] w-[300px] -translate-x-1/2 rounded-full bg-indigo-100/30 blur-3xl dark:bg-indigo-900/10" />
      </div>

      <div className="relative z-10 flex w-full max-w-[820px] items-start gap-7">
        {/* Login card */}
        <div className="w-[380px] shrink-0 rounded-xl border border-slate-100 bg-white p-8 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)] transition-all duration-200 dark:border-slate-700 dark:bg-slate-900">
          <div className="mb-8 text-center">
            <h1 className="bg-gradient-to-r from-indigo-600 to-violet-600 bg-clip-text text-2xl font-extrabold tracking-tight text-transparent">
              chttp2
            </h1>
            <p className="mt-1.5 text-sm text-slate-500">Sign in to your NAS dashboard</p>
          </div>
          <LoginForm />
        </div>

        {/* Sessions panel */}
        <div className="flex-1 rounded-xl border border-slate-100 bg-white p-7 shadow-[0_4px_20px_-2px_rgba(79,70,229,0.1)] dark:border-slate-700 dark:bg-slate-900">
          <h2 className="mb-5 text-base font-bold text-slate-900">Active Sessions</h2>
          <SessionList />
        </div>
      </div>
    </div>
  )
}
